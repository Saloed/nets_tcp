#include <mutex>
#include <vector>
#include <unordered_map>
#include <netinet/in.h>
#include <sys/epoll.h>

#include "logger.h"
#include "sockutils.h"
#include "defines.h"

namespace server {
    struct Client {
        int descriptor = -1;
        volatile std::atomic_bool is_active = true;

        std::vector<char> receive_buffer;
        std::vector<char> send_buffer;

        std::mutex mutex;
    };

    class Server {

    public:
        Server() : server_socket(-1), terminate(false) {
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
            auto &&client = clients[client_d];
            clients.erase(client_d);
            close(client.descriptor);
            client.is_active = false;
        }

        void accept_client() {

        }

        void read_client_data(int client_id) {

        }

        void epoll_loop() {
            auto &&epoll_d = epoll_create(0);
            if (epoll_d == -1) {
                Logger::logger->error("Cannot create epoll descriptor");
                std::exit(1);
            }
            epoll_event event{};
            event.events = EPOLLIN | EPOLLET;
            event.data.fd = server_socket;
            auto &&ctl_stat = epoll_ctl(epoll_d, EPOLL_CTL_ADD, server_socket, &event);
            if (ctl_stat == -1) {
                Logger::logger->error("epoll_ctl failed");
                std::exit(1);
            }
            std::array<epoll_event, 10> events{};
            while (!terminate) {
                auto &&event_cnt = epoll_wait(epoll_d, events.data(), 10, 1000);
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
                        else read_client_data(evt.data.fd);
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

        volatile std::atomic_bool terminate;
        int server_socket;

    };

}

