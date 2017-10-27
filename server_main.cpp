#include <iostream>
#include <sstream>
#include "server.h"

void help() {
    std::stringstream out_string;

    out_string << "help: print this help message\n";
    out_string << "list: list connected clients\n";
    out_string << "kill [id]: disconnect client with specified id\n";
    out_string << "killall: disconnect all clients\n";
    out_string << "shutdown: shutdown server\n";

    std::cout << out_string.str() << std::endl;
}

int main(int argc, char **argv) {
    auto &&server = server::Server();
    server.start();
    std::string command;
    while (server.is_active()) {
        std::getline(std::cin, command);
        if (command == "help") help();
        else if (command == "list") std::cout << server.list_clients() << std::endl;
        else if (command == "killall") server.close_all_clients();
        else if (!command.compare(0, 4, "kill")) {
            auto&& client_id = std::stoi(command.substr(5));
            server.close_client(client_id);
        } else if (command == "shutdown") {
            break;
        }
    }
    server.stop();
}