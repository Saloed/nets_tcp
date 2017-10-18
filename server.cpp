#include <netinet/in.h>
#include <arpa/inet.h>
#include <memory.h>
#include <iostream>
#include <list>
#include <thread>
#include <unistd.h>
#include <mutex>
#include <atomic>
#include <sstream>
#include <unordered_map>
#include "defines.h"
#include "utils.h"

std::mutex clients_mutex;
volatile std::atomic_bool terminate;

class Client {
public:
    Client() = default;
    Client(int sock, const sockaddr_in &addr) : sock(sock), active(true), addr(addr) {}

    void shutdown() {
        active = false;
        ::shutdown(sock, 2);
        close(sock);
    }

    void deactivate() {
        active = false;
    }

public:
    int sock;
    bool active;
    sockaddr_in addr;
};

std::unordered_map<int, Client> clients;
std::unordered_map<int, std::thread> workers;

void handle_client(int client_id) {
    auto&& client = clients[client_id];
    char message[MESSAGE_SIZE];
    while (!terminate && client.active) {
        bzero(message, MESSAGE_SIZE);
        auto &&result_code = readn(client.sock, message, sizeof(message));
        if (result_code == ERROR_MESSAGE_SIZE || result_code == RECV_ERROR) break;
        if (result_code == RECV_TIMEOUT) continue;
        if (result_code == -1) break;
        if (strcmp(message, "/disconnect") == 0) break;
        if (send(client.sock, message, MESSAGE_SIZE, 0) == -1) {
            std::cerr << "Error in send" << std::endl;
            break;
        }
    }
    client.shutdown();
}



std::string socket_info(sockaddr_in &client_info) {
    std::string connected_ip = inet_ntoa(client_info.sin_addr);
    int port = ntohs(client_info.sin_port);
    return connected_ip + ":" + std::to_string(port);
}


void remove_disconnected() {
    std::unique_lock<std::mutex> lock(clients_mutex);
    std::stringstream out_string;
    auto &&it = clients.begin();
    while (it != clients.end()) {
        if (it->second.active) {
            ++it;
            continue;
        }
        auto&& client_id = it->first;
        auto&& client_addr = it->second.addr;

        auto&& worker = workers[client_id];
        if(worker.joinable()) worker.join();
        it = clients.erase(it);
        workers.erase(client_id);
        out_string << "disconnected id: " << client_id << " " << socket_info(client_addr) << "\n";
    }
    lock.unlock();
    std::cout << out_string.str() << std::flush;
}

int get_id_for_client() {
    int id = 0;
    int border = (clients.size() + 1) * 2;
    do {
        id = std::rand() % border;
    } while (clients.find(id) != clients.end());
    return id;
}

void accept_new_client(int server_d) {
    sockaddr_in clientaddr{};
    socklen_t addrlen = sizeof(clientaddr);
    fd_set server_descriptor_set{};
    FD_ZERO(&server_descriptor_set);
    FD_SET(server_d, &server_descriptor_set);
    timeval timeout{0, 1000};
    int select_res = select(server_d + 1, &server_descriptor_set, nullptr, nullptr, &timeout);
    if (select_res < 0) {
        std::cerr << "Error in select" << std::endl;
        terminate = true;
    }
    if (select_res == 0 || !FD_ISSET(server_d, &server_descriptor_set)) {
        return;
    }
    int client_d = accept(server_d, (struct sockaddr *) &clientaddr, &addrlen);
    if (client_d == -1) {
        std::cerr << "Error in accept" << std::endl;
        terminate = true;
    } else {
        std::unique_lock<std::mutex> lock(clients_mutex);
        struct timeval tv{2, 0};
        setsockopt(client_d, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(timeval));
        auto &&id = get_id_for_client();
        clients[id] = Client(client_d, clientaddr);
        workers[id] = std::thread(handle_client, id);
        lock.unlock();
        std::string client_info = inet_ntoa(clientaddr.sin_addr);
        std::cout << "New connection from " << client_info << " on socket " << client_d << std::endl;
    }
}


bool disconnect_by_id(int id) {
    auto &&client = clients.find(id);
    if (client == clients.end()) {
        return false;
    }
    client->second.deactivate();
}

int disconnect_all() {
    for (auto&&[client_id, client]: clients) {
        client.deactivate();
    }
    return 0;
}

int server_main(int server_d) {
    while (!terminate) {
        accept_new_client(server_d);
        remove_disconnected();
    }

    disconnect_all();
    remove_disconnected();
    return 0;
}

void list_clients() {
    std::stringstream out_string;
    out_string << "Clients connected:\n";
    for (auto[client_id, client]: clients) {
        out_string << "id: " << client_id << " " << socket_info(client.addr) << "\n";
    }
    std::cout << out_string.str() << std::endl;
}

void help() {
    std::stringstream out_string;

    out_string << "help: print this help message\n";
    out_string << "list: list connected clients\n";
    out_string << "kill [id]: disconnect client with specified id\n";
    out_string << "killall: disconnect all clients\n";
    out_string << "shutdown: shutdown server\n";

    std::cout << out_string.str() << std::endl;
}

int main() {

    sockaddr_in server_address{};
    int server_d = socket(AF_INET, SOCK_STREAM, 0);
    if (server_d < 0) {
        std::cerr << "Cannot open socket" << std::endl;
        exit(1);
    }

    int enable_options = 1;
    setsockopt(server_d, SOL_SOCKET, SO_REUSEADDR, &enable_options, sizeof(enable_options));

    bzero(&server_address, sizeof(server_address));

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(SERVER_PORT);

    if (bind(server_d, reinterpret_cast<const sockaddr *>(&server_address), sizeof(server_address)) < 0) {
        std::cerr << "Cannot bind" << std::endl;
        exit(1);
    }
    if (listen(server_d, 10) == -1) {
        std::cerr << "Error while marking socket as listener" << std::endl;
        exit(1);
    }
    terminate = false;
    std::thread server_thread(server_main, server_d);

    std::cout << "Server started on port " << SERVER_PORT << std::endl;

    std::string command;

    while (!terminate) {
        std::getline(std::cin, command);
        if (command == "help") help();
        else if (command == "list") list_clients();
        else if (command == "killall") disconnect_all();
        else if (!command.compare(0, 4, "kill")) {
            int client_id = std::stoi(command.substr(5));
            disconnect_by_id(client_id);
        } else if (command == "shutdown") {
            terminate = true;
            break;
        }
    }

    shutdown(server_d, SHUT_RDWR);
    close(server_d);
    if (server_thread.joinable()) {
        server_thread.join();
    }
    std::cout << "server stopped" << std::endl;
    return 0;
}
