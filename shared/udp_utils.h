#pragma once
#include <string>
#include <tuple>
#include "defines.h"

#include <iostream>

#define success_header_size (sizeof(int) + sizeof(char))
std::string make_chunk_success_packet(int chunk_number) {
    char info_str[success_header_size];
    info_str[0] = CHUNK_SUCCESS_MESSAGE;
    auto packet_info = (int *) (info_str + sizeof(char));
    packet_info[0] = chunk_number;
    return std::string(info_str, success_header_size);
}

//std::string make_chunk_request_packet(int chunk_number) {
//    char info_str[sizeof(int) + 1];
//    info_str[0] = CHUNK_REQUEST_MESSAGE;
//    auto packet_info = (int *) (info_str + 1);
//    packet_info[0] = chunk_number;
//    return std::string(info_str);
//}
#define content_header_size (sizeof(int) * 2 + sizeof(char))
std::string make_content_message(int chunk_number, int total, std::string& content) {
    char info_str[content_header_size];
    info_str[0] = CONTENT_MESSAGE;
    auto packet_info = reinterpret_cast<int*>(info_str + sizeof(char));
    packet_info[0] = chunk_number;
    packet_info[1] = total;
    auto packet = std::string(info_str, content_header_size) + content;
    return packet;
}

std::tuple<int, int, std::string> parse_content_message(char* received_message, int message_size){
    auto as_int_array = reinterpret_cast<int*>(received_message + 1);
    int message_number = as_int_array[0];
    int message_total = as_int_array[1];
    char *content = received_message + content_header_size;
    std::string message(content, message_size - content_header_size);
    return std::make_tuple(message_number, message_total, message);
};


#ifdef WIN32
#define socket_t SOCKET
#else
#define socket_t int
#endif

int send_udp(socket_t socket, std::string& packet, sockaddr* addr){
    return sendto(socket, packet.data(), packet.size(), 0, addr, sizeof(sockaddr));
}