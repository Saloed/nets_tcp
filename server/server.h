#ifndef ECHOSERVER_SERVER_H
#define ECHOSERVER_SERVER_H

#include <mutex>
#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

#include "thread_pool/ThreadPool.h"
#include "defines.h"

namespace server {
    class Client {
    public:
        Client() : descriptor(-1), is_active(false), event{} {
            mutex = std::make_unique<std::mutex>();
        }

        explicit Client(int descriptor, epoll_event &event, std::string& client_ip) :
                descriptor(descriptor), is_active(true), event(event), client_ip_addr(client_ip) {
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
        std::string client_ip_addr;
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
        void create_server_socket();

        void accept_client();

        void read_client_data(int client_id);

        void handle_client_if_possible(int client_id);

        void process_client_message(std::string &message, int client_id);

        void process_client_command(std::string_view command, int client_id);

        void process_client_text(std::string_view text, int client_id);

        void epoll_loop();

    public:
        void stop();

        void start();

        bool is_active();

        void close_client(int client_d);

        void close_all_clients();

        std::string list_clients();

    private:
        std::unordered_map<int, Client> clients;
        std::thread server_thread;
        std::mutex clients_mutex;
        ThreadPool workers;
        volatile std::atomic_bool terminate;
        int server_socket;
        int epoll_descriptor;

    };
};

#endif //ECHOSERVER_SERVER_H
