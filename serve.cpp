#include <mutex>
#include <vector>
#include <unordered_map>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

#include "thread_pool/ThreadPool.h"
#include "logger.h"
#include "sockutils.h"
#include "defines.h"

namespace server {
    class Client {
    public:
        Client() : descriptor(-1), is_active(false), event{} {
            mutex = std::make_unique<std::mutex>();
        }

        explicit Client(int descriptor, epoll_event &event) :
                descriptor(descriptor), is_active(true), event(event) {
            mutex = std::make_unique<std::mutex>();
        }

        Client &operator=(Client &&other) noexcept {
            if (this != &other) {
                descriptor = other.descriptor;
                is_active = other.is_active.load();
                mutex = std::move(other.mutex);
                receive_buffer = std::move(other.receive_buffer);
            }
            return *this;
        }

        int descriptor;
        epoll_event event;
        volatile std::atomic_bool is_active;

        std::string receive_buffer;

        std::unique_ptr<std::mutex> mutex;
    };

    class Server {

    public:
        Server() : server_socket(-1), epoll_descriptor(-1), terminate(false), workers(4) {
            create_server_socket();
        }

        ~Server() {
            stop();
        }

    private:

        void create_server_socket() {
            auto &&server_d = socket(AF_INET, SOCK_STREAM, 0);
            if (server_d < 0) {
                Logger::logger->error("Cannot open socket");
                std::exit(1);
            }
            int enable_options = 1;
            setsockopt(server_d, SOL_SOCKET, SO_REUSEADDR, &enable_options, sizeof(enable_options));

            sockaddr_in server_address{};
            bzero(&server_address, sizeof(server_address));
            server_address.sin_family = AF_INET;
            server_address.sin_addr.s_addr = INADDR_ANY;
            server_address.sin_port = htons(SERVER_PORT);
            auto &&bind_addr = reinterpret_cast<const sockaddr *>(&server_address);
            auto &&bind_stat = bind(server_d, bind_addr, sizeof(server_address));
            if (bind_stat < 0) {
                Logger::logger->error("Cannot bind");
                std::exit(1);
            }
            if (!socket_utils::set_socket_nonblock(server_d)) {
                Logger::logger->error("Cannot set server socket nonblock");
                std::exit(1);
            }
            auto &&listen_stat = listen(server_d, 2);
            if (listen_stat == -1) {
                Logger::logger->error("set server socket listen error");
                std::exit(1);
            }
            server_socket = server_d;
        }

        void close_client(int client_d) {
            Client &&client = clients[client_d];
            clients.erase(client_d);
            epoll_ctl(epoll_descriptor, EPOLL_CTL_DEL, client_d, &client.event);
            close(client.descriptor);
            client.is_active = false;
        }

        void accept_client() {
            sockaddr_in client_addr{};
            auto &&client_addr_in = reinterpret_cast<sockaddr *>(&client_addr);
            auto &&client_addr_len = static_cast<socklen_t>(sizeof(sockaddr));
            auto &&client_d = accept(server_socket, client_addr_in, &client_addr_len);
            if (client_d == -1) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    Logger::logger->error("Accept failed");
                }
                return;
            }
            if (!socket_utils::set_socket_nonblock(client_d)) {
                Logger::logger->error("Cannot set client socket {} nonblock", client_d);
                return;
            }
            epoll_event event{};
            event.data.fd = client_d;
            event.events = EPOLLIN;
            auto &&ctl_stat = epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, client_d, &event);
            if (ctl_stat == -1) {
                Logger::logger->error("epoll_ctl failed");
                return;
            }
            clients[client_d] = Client(client_d, event);
            std::string client_info = inet_ntoa(client_addr.sin_addr);
            Logger::logger->info("New connection from {} on socket {}", client_info, client_d);
        }

        void read_client_data(int client_id) {
            char read_buffer[MESSAGE_SIZE];
            auto &&count = read(client_id, read_buffer, MESSAGE_SIZE);
            if (count == -1 && errno == EAGAIN) return;
            if (count == -1) Logger::logger->error("Error in read for socket {}", client_id);
            if (count <= 0) {
                close_client(client_id);
                return;
            }
            Client &&client = clients[client_id];
            client.receive_buffer.append(read_buffer, static_cast<unsigned long>(count));
        }

        void handle_client_if_possible(int client_id){
            Client &&client = clients[client_id];
            auto&& message_end = client.receive_buffer.find(MESSAGE_END);
            if(message_end != std::string::npos){
                auto&& message = client.receive_buffer.substr(0, message_end);
                client.receive_buffer.erase(0, message_end+3);
                workers.enqueue([&message, &client]{
                    //TODO
                });
            }
        }

        void epoll_loop() {
            epoll_descriptor = epoll_create(0);
            if (epoll_descriptor == -1) {
                Logger::logger->error("Cannot create epoll descriptor");
                std::exit(1);
            }
            epoll_event event{};
            event.events = EPOLLIN;
            event.data.fd = server_socket;
            auto &&ctl_stat = epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, server_socket, &event);
            if (ctl_stat == -1) {
                Logger::logger->error("epoll_ctl failed");
                std::exit(1);
            }
            std::array<epoll_event, 10> events{};
            while (!terminate) {
                auto &&event_cnt = epoll_wait(epoll_descriptor, events.data(), 10, 1000);
                for (auto &&i = 0; i < event_cnt; ++i) {
                    auto &&evt = events[i];
                    if (evt.events & EPOLLERR) {
                        Logger::logger->error("Epoll error for socket {}", evt.data.fd);
                        close_client(evt.data.fd);
                    }
                    if (evt.events & EPOLLHUP) {
                        Logger::logger->info("client socket closed {}", evt.data.fd);
                        close_client(evt.data.fd);
                    }
                    if (evt.events & EPOLLIN) {
                        if (evt.data.fd == server_socket) accept_client();
                        else {
                            read_client_data(evt.data.fd);
                            handle_client_if_possible(evt.data.fd);
                        }
                    }
                }
            }
        }

    public:
        void stop() {
            if (terminate)
                return;
            terminate = true;
            if (server_thread.joinable()) {
                server_thread.join();
            }
        }

        void start() {
            terminate = false;
            server_thread = std::move(std::thread(&Server::epoll_loop, this));
        }

    private:
        std::unordered_map<int, Client> clients;
        std::thread server_thread;
        ThreadPool workers;
        volatile std::atomic_bool terminate;
        int server_socket;
        int epoll_descriptor;

    };

}

