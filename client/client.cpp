#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#include <iostream>
#include "json/src/json.hpp"
#include "defines.h"
#include "client_defs.h"

int receive_from_server(SOCKET server_socket, std::string &received) {
    char message_buf[MESSAGE_SIZE];
    while (true) {
        auto &&count = recv(server_socket, message_buf, MESSAGE_SIZE, 0);
        if (count == 0) {
            std::cerr << "socket closed" << std::endl;
            return -1;
        }
        if (count < 0) {
            std::cerr << "socket error " << WSAGetLastError() << std::endl;
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

    WSADATA wsa_data{0};

    auto &&startup_status = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (startup_status != 0) {
        std::cerr << "WSAStartup failed with error code " << startup_status << std::endl;
        std::exit(1);
    }

    addrinfo *result = nullptr;
    addrinfo hints{};
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    auto &&getaddr_status = getaddrinfo("192.168.1.73", "7777", &hints, &result);
    if (getaddr_status != 0) {
        std::cerr << "Getaddr failed with status " << getaddr_status << std::endl;
        WSACleanup();
        std::exit(2);
    }

    auto &&server_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (server_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed with status " << WSAGetLastError() << std::endl;
        WSACleanup();
        std::exit(3);
    }

    auto &&connect_status = connect(server_socket, result->ai_addr, static_cast<int>(result->ai_addrlen));
    if (connect_status == SOCKET_ERROR) {
        std::cerr << "Connection to server failed with status " << WSAGetLastError() << std::endl;
        WSACleanup();
        std::exit(4);
    }
    freeaddrinfo(result);

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

    closesocket(server_socket);
    WSACleanup();
}