#ifndef ECHOSERVER_SERVER_H
#define ECHOSERVER_SERVER_H

#include <mutex>
#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>

#include<winsock2.h>
#pragma comment(lib,"ws2_32.lib")

#include "thread_pool/ThreadPool.h"
#include "database/FinanceDb.h"
#include "defines.h"

typedef int epoll_event;

namespace server {
    class Client {
    public:
        Client() : descriptor(0), is_active(false) {
            mutex = std::make_unique<std::mutex>();
        }

        explicit Client(SOCKET descriptor, std::string &client_ip) :
                descriptor(descriptor), is_active(true), client_ip_addr(client_ip) {
            mutex = std::make_unique<std::mutex>();
        }

        Client &operator=(Client &&other) noexcept {
            if (this != &other) {
                descriptor = other.descriptor;
                is_active = other.is_active.load();
                mutex = std::move(other.mutex);
                receive_buffer = std::move(other.receive_buffer);
                send_buffer = std::move(other.send_buffer);
            }
            return *this;
        }

        SOCKET descriptor;
        volatile std::atomic_bool is_active;
        std::vector<std::string> send_buffer;
        std::vector<std::string> receive_buffer;
        std::string client_ip_addr;
        std::unique_ptr<std::mutex> mutex;
    };


    class Server {

    public:
        Server() : server_socket(0), terminate(false), workers(4), database(),
                   read_event_d{}, completion_port{} {
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

        void process_client_json(std::string_view json_string, int client_id);

        void process_add_currency(std::string &currency, int client_id);

        void process_add_currency_value(std::string &currency, double value, int client_id);

        void process_del_currency(std::string &currency, int client_id);

        void process_list_all_currencies(int client_id);

        void process_currency_history(std::string &currency, int client_id);

        void serve_loop();

    public:
        void stop();

        void start();

        bool is_active();

        void close_client(int client_d);

        void close_all_clients();

        std::string list_clients();

    private:
        std::unordered_map<int64_t , Client> clients;
        std::thread server_thread;
        std::mutex clients_mutex;
        ThreadPool workers;
        FinanceDb database;
        volatile std::atomic_bool terminate;
        SOCKET server_socket;

        void handle_client_datagram(WSAEVENT);

        void disconnect_time_outed();

        void refresh_client_timeout(sockaddr_storage client_info);

        void client_message_status(sockaddr_storage client_info, int chunk_number, int status);

        void client_message_chunk(sockaddr_storage client_info, int chunk_number, int total, std::string& message);
    };
};

#endif //ECHOSERVER_SERVER_H
