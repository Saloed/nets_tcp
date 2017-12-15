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
#include "udp_utils.h"

#define TIMEOUT_DELTA 30

namespace server {
    class Client {
    public:
        Client() : descriptor(0), timer(0), ip_addr{}, next_receive_packet_timer(nullptr) {}

        explicit Client(int64_t descriptor, std::string &ip_str, sockaddr_in& ip_addr) :
                descriptor(descriptor), timer(0), ip_str(ip_str),
                ip_addr(ip_addr), next_receive_packet_timer(nullptr) {}

        Client &operator=(Client &&other) noexcept {
            if (this != &other) {
                descriptor = other.descriptor;
                receive_buffer = std::move(other.receive_buffer);
                send_buffer = std::move(other.send_buffer);
                timer = other.timer;
                ip_str = std::move(other.ip_str);
                ip_addr = other.ip_addr;
                next_receive_packet_timer = other.next_receive_packet_timer;
            }
            return *this;
        }

        void stop_timer_if_running(HANDLE timer_queue){
            if(next_receive_packet_timer != nullptr){
                DeleteTimerQueueTimer(timer_queue, next_receive_packet_timer, nullptr);
            }
        }

        int64_t descriptor;
        int timer;
        std::vector<std::string> send_buffer;
        std::vector<std::string> receive_buffer;
        std::string ip_str;
        sockaddr_in ip_addr;
        HANDLE next_receive_packet_timer;
    };


    class Server {

    public:
        Server() : server_socket(0), terminate(false), workers(4), database(), clients_lock(), receive_timers(nullptr) {
            create_server_socket();
            InitializeCriticalSection(&clients_lock);
        }

        ~Server() {
            stop();
            DeleteCriticalSection(&clients_lock);
        }

    private:
        void create_server_socket();

        void handle_client_if_possible(int64_t client_id);

        void process_client_message(std::string &message, int64_t client_id);

        void process_client_command(std::string_view command, int64_t client_id);

        void process_client_text(std::string_view text, int64_t client_id);

        void process_client_json(std::string_view json_string, int64_t client_id);

        void process_add_currency(std::string &currency, int64_t client_id);

        void process_add_currency_value(std::string &currency, double value, int64_t client_id);

        void process_del_currency(std::string &currency, int64_t client_id);

        void process_list_all_currencies(int64_t client_id);

        void process_currency_history(std::string &currency, int64_t client_id);

        void serve_loop();

    public:
        void stop();

        void start();

        bool is_active();

        void close_client(int64_t client_d);

        void close_all_clients();

        std::string list_clients();

        void send_chunk(int64_t client_id, std::string_view message);

        std::unordered_map<int64_t , Client> clients;
    private:
        std::thread server_thread;
        std::thread timer_thread;
        CRITICAL_SECTION clients_lock;
        ThreadPool workers;
        FinanceDb database;
        volatile std::atomic_bool terminate;
        SOCKET server_socket;
        HANDLE receive_timers;

        void handle_client_datagram(WSAEVENT);

        void lock_clients(){
            EnterCriticalSection(&clients_lock);
        }

        void unlock_clients(){
            LeaveCriticalSection(&clients_lock);
        }

        void refresh_client_timeout(int64_t client_id);

        void client_message_status(int64_t client_id, int chunk_number, int status);

        void client_message_chunk(int64_t client_id, int chunk_number, int total, std::string& message);

        int64_t get_client_id(sockaddr_in *client_addr);

        void timer_loop();

        void send_message(int64_t client_id, std::string_view message);

        void send_chunk(sockaddr_in &client_addr, std::string_view message);

    };
};

#endif //ECHOSERVER_SERVER_H
