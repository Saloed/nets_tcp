#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include "../shared/defines.h"

int receive_from_server(int server_socket, std::string &received) {
    char message_buf[MESSAGE_SIZE];
    while (true) {
        auto &&count = read(server_socket, message_buf, MESSAGE_SIZE);
        if (count == 0) {
            std::cerr << "socket closed" << std::endl;
            return -1;
        }
        if (count < 0) {
            std::cerr << "socket error" << std::endl;
            return -1;
        }
        received.append(message_buf, count);
        auto &&message_end = received.find(MESSAGE_END);
        if (message_end != std::string::npos) {
            received.erase(message_end);
            return 0;
        }
    }
}

int main() {
    sockaddr_in server_addr{};
    auto &&server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == -1) {
        std::cerr << "Error in socket creation" << std::endl;
        exit(1);
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(SERVER_PORT);
    auto &&server_sockaddr = reinterpret_cast<const sockaddr *>(&server_addr);
    auto &&conn_status = connect(server_socket, server_sockaddr, sizeof(sockaddr));
    if (conn_status < 0) {
        std::cerr << "Error in connection" << std::endl;
        exit(1);
    }

    std::string in_str;
    std::string received;

    while(true) {
        in_str.clear();
        received.clear();
        std::cout << "Enter message: " << std::endl;
        std::getline(std::cin, in_str);
        auto &&to_send = in_str + MESSAGE_END;
        if (send(server_socket, to_send.c_str(), to_send.length(), 0) == -1) {
            std::cerr << "Error in send" << std::endl;
            break;
        }
        std::cout << "Sent: " << in_str << std::endl;
        auto &&receive_code = receive_from_server(server_socket, received);
        if (receive_code < 0) break;
        std::cout << "Received: " << received << std::endl;
    }

    close(server_socket);
}