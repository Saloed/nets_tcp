#pragma once
#include <string>
#include <tuple>
#include "defines.h"

std::string make_chunk_success_packet(int chunk_number) {
    char info_str[sizeof(int) + 1];
    info_str[0] = CHUNK_SUCCESS_MESSAGE;
    auto packet_info = (int *) (info_str + 1);
    packet_info[0] = chunk_number;
    return std::string(info_str);
}

std::string make_chunk_request_packet(int chunk_number) {
    char info_str[sizeof(int) + 1];
    info_str[0] = CHUNK_REQUEST_MESSAGE;
    auto packet_info = (int *) (info_str + 1);
    packet_info[0] = chunk_number;
    return std::string(info_str);
}

std::string make_content_message(int chunk_number, int total, std::string_view content) {
    char info_str[sizeof(int) * 2 + 1];
    info_str[0] = CONTENT_MESSAGE;
    auto packet_info = (int *) (info_str + 1);
    packet_info[0] = chunk_number;
    packet_info[1] = total;
    return std::string(info_str) + std::string(content);
}

std::tuple<int, int, std::string> parse_content_message(char* received_message, int message_size){
    auto *as_int_array = (int *) (received_message + 1);
    int message_number = as_int_array[0];
    int message_total = as_int_array[1];
    char *content = received_message + 1 + 2 * sizeof(int);
    std::string message(content, message_size);
    return std::make_tuple(message_number, message_total, message);
};
