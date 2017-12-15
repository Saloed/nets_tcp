#define WIN32_LEAN_AND_MEAN

#include <iostream>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <udp_utils.h>
#include "json/src/json.hpp"
#include "defines.h"
#include "client_defs.h"


void send_to_server(sockaddr_in &, int, std::string &);

int receive_from_server(sockaddr_in &, int, std::string &);


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

int main(int argc,  char* argv[]) {
    if(argc < 2) argv[1] = "192.168.1.73";
    sockaddr_in server_address{};
    int client_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    memset((char *) &server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);
    server_address.sin_addr.s_addr = inet_addr(argv[1]);

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
        send_to_server(server_address, client_sock, to_send);
        std::cout << "Sent: " << in_str << std::endl;
        auto &&receive_code = receive_from_server(server_address, client_sock, received);
        if (receive_code < 0) break;
        auto &&response = parse_response(received);
        std::cout << "Received: " << response << std::endl;
    }
    close(client_sock);
}

void send_to_server(sockaddr_in &server_addr, int socket, std::string &message) {
    std::cout << "Sending " << message << std::endl;
    std::vector<std::string> send_buffer;
    auto size = message.size() / UDP_PACKET_SIZE + 1;
    for (auto i = 0, j = 0; i < size; i += UDP_PACKET_SIZE, ++j) {
        std::string chunk = message.substr(i, UDP_PACKET_SIZE);
        auto packet = make_content_message(j, size, chunk);
        send_buffer.emplace_back(packet);
    }
    auto _server_addr = reinterpret_cast<sockaddr *>(&server_addr);
    for (auto &&packet: send_buffer) {
        auto &&send_stat = sendto(socket, packet.data(), packet.size(), 0, _server_addr, sizeof(sockaddr));
    }
    char receive_buffer[MESSAGE_SIZE + 1];
    sockaddr client_info{};
    auto client_info_size = sizeof(sockaddr);
    auto client_info_size_ptr = reinterpret_cast<socklen_t *>(&client_info_size);
    while (true) {
        std::cout << "help me" << std::endl;
        bzero(receive_buffer, MESSAGE_SIZE + 1);
        auto bytes = recvfrom(socket, receive_buffer, MESSAGE_SIZE, 0, &client_info, client_info_size_ptr);
        auto address = reinterpret_cast<sockaddr_in *>(&client_info);
        if (address->sin_addr.s_addr != server_addr.sin_addr.s_addr) continue;
        if (bytes < 0) {
            std::cerr << "Receive error" << std::endl;
            continue;
        }
        if (bytes == 0) continue;
        auto message_type = receive_buffer[0];
        if (message_type == CONTENT_MESSAGE) continue;
        if (message_type == CHUNK_REQUEST_MESSAGE) {
            int message_number = *(int *) (receive_buffer + 1);
            auto packet = send_buffer.at(message_number);
            auto &&send_stat = sendto(socket, packet.data(), packet.size(), 0, _server_addr, sizeof(sockaddr));
            continue;
        }
        if (message_type == CHUNK_SUCCESS_MESSAGE) {
            int message_number = *(int *) (receive_buffer + 1);
            if (message_number == send_buffer.size()) break;
        }
    }
}

int receive_from_server(sockaddr_in &server_addr, int socket, std::string &received) {
    char buffer[MESSAGE_SIZE + 1];
    std::vector<std::string> receive_buffer;
    sockaddr client_info{};
    auto client_info_size = sizeof(sockaddr);
    auto client_info_size_ptr = reinterpret_cast<socklen_t *>(&client_info_size);
    auto _server_addr = reinterpret_cast<sockaddr *>(&server_addr);
    while (true) {
        bzero(buffer, MESSAGE_SIZE + 1);
        auto bytes = recvfrom(socket, buffer, MESSAGE_SIZE, 0, &client_info, client_info_size_ptr);
        auto address = reinterpret_cast<sockaddr_in *>(&client_info);
        if (address->sin_addr.s_addr != server_addr.sin_addr.s_addr) continue;
        if (bytes < 0) {
            std::cerr << "Receive error" << std::endl;
            continue;
        }
        if (bytes == 0) continue;
        auto message_type = buffer[0];
        if (message_type == CHUNK_REQUEST_MESSAGE || message_type == CHUNK_SUCCESS_MESSAGE) continue;
        if (message_type == CONTENT_MESSAGE) {
            auto expected_chunk = receive_buffer.size();
            int message_number;
            int message_total;
            std::string message;
            std::tie(message_number, message_total, message) = parse_content_message(buffer, bytes);
            if(message_number < expected_chunk) continue;
            if(message_number > expected_chunk){
                auto packet = make_chunk_request_packet(expected_chunk);
                auto &&send_stat = sendto(socket, packet.data(), packet.size(), 0, _server_addr, sizeof(sockaddr));
                continue;
            }
            receive_buffer.emplace_back(message);
            if(receive_buffer.size() == message_total){
                auto packet = make_chunk_success_packet(message_total);
                auto &&send_stat = sendto(socket, packet.data(), packet.size(), 0, _server_addr, sizeof(sockaddr));
                break;
            }
        }
    }
    for(auto&& chunk : receive_buffer){
        received += chunk;
    }
    auto &&message_end = received.find(MESSAGE_END);
    if (message_end != std::string::npos) {
        received.erase(message_end);
        return 0;
    } else {
        return -1;
    }
}