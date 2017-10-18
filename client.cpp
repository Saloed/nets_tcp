#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <memory.h>
#include <iostream>
#include "defines.h"
#include "utils.h"

int main() {
    sockaddr_in server_addr{};
    char buffer[MESSAGE_SIZE];
    char message[MESSAGE_SIZE];

    int server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == -1) {
        std::cerr << "Error in socket creation" << std::endl;
        exit(1);
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(SERVER_PORT);

    int conn_status = connect(server_socket, reinterpret_cast<const sockaddr *>(&server_addr), sizeof(server_addr));
    if (conn_status < 0) {
        std::cerr << "Error in connection" << std::endl;
        exit(1);
    }

    std::string in_str;

    do {
        bzero(buffer, MESSAGE_SIZE);
        bzero(message, MESSAGE_SIZE);

        std::cout << "Enter message: " << std::endl;
        std::getline(std::cin, in_str);
        memcpy(buffer, in_str.c_str(), in_str.size());
        if (send(server_socket, buffer, MESSAGE_SIZE, 0) == -1) {
            std::cerr << "Error in send" << std::endl;
            break;
        }
        std::cout << "Sent: " << buffer << std::endl;
        auto &&read_status = readn(server_socket, message, MESSAGE_SIZE);
        if (read_status == 0) {
            std::cout << "Received: " << message << std::endl;
        } else {
            std::cerr << "Read complete with status code " << read_status << std::endl;
            break;
        }
    } while (in_str != "/disconnect");

    close(server_socket);
}