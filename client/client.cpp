#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include "json/src/json.hpp"
#include "defines.h"
#include "client_defs.h"

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

std::vector<std::string> split_by_space(std::string &str) {
    std::istringstream buf(str);
    std::istream_iterator<std::string> beg(buf), end;
    std::vector<std::string> tokens(beg, end);
    return tokens;
}


std::string parse_response(std::string &basic_string) {
    auto &&response_type = basic_string.substr(0, MESSAGE_PREFIX_LEN);
    auto &&result = basic_string.substr(MESSAGE_PREFIX_LEN);
    if (response_type == TXT_PREFIX || response_type == ERROR_PREFIX) {
        return result;
    } else if (response_type == JSON_PREFIX) {
        return nlohmann::json::parse(result).dump(4);
    }
    return basic_string;
}


std::string help_message() {
    std::stringstream message;
    message << "/add [currency] : add new currency\n"
            << "/addv [currency] [value] : add new value to currency\n"
            << "/del [currency] : remove currency\n"
            << "/all : list all currencies\n"
            << "/hist [currency] : history for currency";
    return message.str();
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
    std::string to_send;

    while (true) {
        in_str.clear();
        to_send.clear();
        received.clear();
        std::cout << "Enter message: " << std::endl;
        std::getline(std::cin, in_str);
        if (in_str[0] == '/') {
            auto &&cleaned = in_str.substr(1);
            auto &&tokens = split_by_space(cleaned);
            if (tokens.empty()) {
                std::cerr << "Incorrect input" << std::endl;
                continue;
            }
            auto &&cmd = tokens[0];

            if (cmd == "help") {
                std::cout << help_message() << std::endl;
                continue;
            }

            if (cmd == ADD_CURRENCY_CMD ||
                cmd == DEL_CURRENCY_CMD ||
                cmd == HISTORY_CURRENCY_CMD) {
                if (tokens.size() != 2) {
                    std::cerr << "Invalid command arguments" << std::endl;
                    continue;
                }
                auto &&currency = tokens[1];
                nlohmann::json request_json = {
                        {"currency", currency}
                };
                if (cmd == ADD_CURRENCY_CMD) request_json["type"] = REQUEST_ADD_CURRENCY;
                else if (cmd == DEL_CURRENCY_CMD) request_json["type"] = REQUEST_DEL_CURRENCY;
                else if (cmd == HISTORY_CURRENCY_CMD) request_json["type"] = REQUEST_GET_CURRENCY_HISTORY;
                else continue;
                to_send = JSON_PREFIX + request_json.dump() + MESSAGE_END;
            } else if (cmd == ADD_CURRENCY_VALUE_CMD) {
                if (tokens.size() != 3) {
                    std::cerr << "Invalid command arguments" << std::endl;
                    continue;
                }
                auto &&currency = tokens[1];
                auto &&value = strtod(tokens[2].data(), nullptr);
                nlohmann::json request_json = {
                        {"type", REQUEST_ADD_CURRENCY_VALUE},
                        {"currency", currency},
                        {"value",    value}
                };
                to_send = JSON_PREFIX + request_json.dump() + MESSAGE_END;

            } else if (cmd == LIST_CURRENCIES_CMD) {
                to_send = CMD_PREFIX + std::string(REQUEST_GET_ALL_CURRENCIES) + MESSAGE_END;
            } else {
                to_send = CMD_PREFIX + cmd + MESSAGE_END;
            }
        } else {
            to_send = TXT_PREFIX + in_str + MESSAGE_END;
        }
        if (send(server_socket, to_send.c_str(), to_send.length(), 0) == -1) {
            std::cerr << "Error in send" << std::endl;
            break;
        }
        std::cout << "Sent: " << in_str << std::endl;
        auto &&receive_code = receive_from_server(server_socket, received);
        if (receive_code < 0) break;
        auto &&response = parse_response(received);
        std::cout << "Received: " << response << std::endl;
    }

    close(server_socket);
}