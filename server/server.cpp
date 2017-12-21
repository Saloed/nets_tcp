#include <sstream>
#include <WS2tcpip.h>
#include "server.h"
#include "udp_utils.h"
#include "logging/logger.h"
#include "json/src/json.hpp"



void server::Server::create_server_socket() {
    WSADATA wsa{};
    auto init_status = WSAStartup(MAKEWORD(2, 2), &wsa);

    if (init_status != 0) {
        Logger::logger_inst->error("WSA init failed with code {}", WSAGetLastError());
        std::exit(EXIT_FAILURE);
    }
    auto server_d = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server_d == INVALID_SOCKET) {
        Logger::logger_inst->error("Error in socket creation {}", WSAGetLastError());
        std::exit(EXIT_FAILURE);
    }
    BOOL fFlag = TRUE;
    auto reuse_status = setsockopt(server_d, SOL_SOCKET, SO_REUSEADDR, (char *) &fFlag, sizeof(fFlag));
    if (reuse_status == SOCKET_ERROR) {
        Logger::logger_inst->error("Error in set socket reuse addr{}", WSAGetLastError());
        std::exit(EXIT_FAILURE);
    }
    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(SERVER_PORT);

    auto bind_status = bind(server_d, (sockaddr *) &server, sizeof(sockaddr));
    if (bind_status == SOCKET_ERROR) {
        Logger::logger_inst->error("Error in socket bind {}", WSAGetLastError());
        std::exit(EXIT_FAILURE);
    }
//    u_long argp = 1;
//    auto nonblock_status = ioctlsocket(server_d, 0x8004667E, &argp);
//    if (nonblock_status == SOCKET_ERROR) {
//        Logger::logger_inst->error("Error in setting socket nonblock {}", WSAGetLastError());
//        std::exit(EXIT_FAILURE);
//    }
    server_socket = server_d;
    Logger::logger_inst->info("Server initialized");
}

void server::Server::close_client(int64_t client_d) {
    lock_clients();
    if (clients.find(client_d) == clients.end())
        return;
//    auto &&client = clients[client_d];
    clients.erase(client_d);
    unlock_clients();
    Logger::logger_inst->info("Client {} disconnected", client_d);
}


void server::Server::process_client_message(std::string &message, int64_t client_id) {
    Logger::logger_inst->info(message);
    std::string_view message_view(message);
    if (message_view.compare(0, MESSAGE_PREFIX_LEN, CMD_PREFIX) == 0) {
        message_view.remove_prefix(MESSAGE_PREFIX_LEN);
        process_client_command(message_view, client_id);
    } else if (message_view.compare(0, MESSAGE_PREFIX_LEN, TXT_PREFIX) == 0) {
        message_view.remove_prefix(MESSAGE_PREFIX_LEN);
        process_client_text(message_view, client_id);
    } else if (message_view.compare(0, MESSAGE_PREFIX_LEN, JSON_PREFIX) == 0) {
        message_view.remove_prefix(MESSAGE_PREFIX_LEN);
        process_client_json(message_view, client_id);
    } else {
        Logger::logger_inst->error("Client {} Unknown message type: {}", client_id, message);
        auto &&err_message = ERROR_PREFIX + std::string("Unknown message type") + MESSAGE_END;
        send_message(client_id, err_message);
    }
}


void server::Server::process_client_text(std::string_view text, int64_t client_id) {
    Logger::logger_inst->info("Text from client {}: {}", client_id, text.data());
    auto &&to_send = std::string(text) + MESSAGE_END;
    send_message(client_id, to_send);
}


void server::Server::process_add_currency(std::string &currency, int64_t client_id) {
    Logger::logger_inst->info("Client {} add currency {}", client_id, currency);
    auto &&status = database.add_currency(currency);
    if (status == 0) {
        auto &&response = TXT_PREFIX + std::string("Successfully add currency ") + currency + MESSAGE_END;
        send_message(client_id, response);
    } else if (status == 1) {
        auto &&err_message = ERROR_PREFIX + std::string("Currency already exists: ") + currency + MESSAGE_END;
        send_message(client_id, err_message);
    } else {
        auto &&err_message = ERROR_PREFIX + std::string("Database error") + MESSAGE_END;
        send_message(client_id, err_message);
    }
}

void server::Server::process_add_currency_value(std::string &currency, double value, int64_t client_id) {
    Logger::logger_inst->info("Client {} add currency {} value {}", client_id, currency, value);
    auto &&status = database.add_currency_value(currency, value);
    if (status == 0) {
        auto &&response = TXT_PREFIX + std::string("Successfully add value for currency ") + currency + MESSAGE_END;
        send_message(client_id, response);
    } else if (status == 1) {
        auto &&err_message = ERROR_PREFIX + std::string("No such currency ") + currency + MESSAGE_END;
        send_message(client_id, err_message);
    } else {
        auto &&err_message = ERROR_PREFIX + std::string("Database error") + MESSAGE_END;
        send_message(client_id, err_message);
    }
}

void server::Server::process_del_currency(std::string &currency, int64_t client_id) {
    Logger::logger_inst->info("Client {} del currency {}", client_id, currency);
    auto &&status = database.del_currency(currency);
    if (status == 0) {
        auto &&response = TXT_PREFIX + std::string("Successfully del currency ") + currency + MESSAGE_END;
        send_message(client_id, response);
    } else if (status == 1) {
        auto &&err_message = ERROR_PREFIX + std::string("No such currency ") + currency + MESSAGE_END;
        send_message(client_id, err_message);
    } else {
        auto &&err_message = ERROR_PREFIX + std::string("Database error") + MESSAGE_END;
        send_message(client_id, err_message);
    }
}

void server::Server::process_list_all_currencies(int64_t client_id) {
    Logger::logger_inst->info("Client {} list all currencies", client_id);
    nlohmann::json json_response;
    auto &&status = database.currency_list(json_response);
    if (status == 0) {
        auto &&response = JSON_PREFIX + json_response.dump() + MESSAGE_END;
        send_message(client_id, response);
    } else {
        auto &&err_message = ERROR_PREFIX + std::string("Database error") + MESSAGE_END;
        send_message(client_id, err_message);
    }
}

void server::Server::process_currency_history(std::string &currency, int64_t client_id) {
    nlohmann::json json_response;
    auto &&status = database.currency_history(currency, json_response);
    if (status == 0) {
        auto &&response = JSON_PREFIX + json_response.dump() + MESSAGE_END;
        send_message(client_id, response);
    } else if (status == 1) {
        auto &&err_message = ERROR_PREFIX + std::string("No such currency ") + currency + MESSAGE_END;
        send_message(client_id, err_message);
    } else {
        auto &&err_message = ERROR_PREFIX + std::string("Database error") + MESSAGE_END;
        send_message(client_id, err_message);
    }
}

void server::Server::process_client_command(std::string_view command, int64_t client_id) {
    Logger::logger_inst->info("Command from client {}: {}", client_id, command.data());
    if (command == "disconnect") {
        close_client(client_id);
    } else if (command == REQUEST_GET_ALL_CURRENCIES) {
        process_list_all_currencies(client_id);
    } else {
        Logger::logger_inst->error("Client {} Unknown command {}", client_id, command.data());
        auto &&err_message = ERROR_PREFIX + std::string("Unknown command") + MESSAGE_END;
        send_message(client_id, err_message);
    }
}


void server::Server::process_client_json(std::string_view json_string, int64_t client_id) {
    Logger::logger_inst->info("Json from client {}: {}", client_id, json_string.data());
    try {
        auto &&client_json = nlohmann::json::parse(json_string);
        std::string request_type = client_json["type"];
        std::string currency = client_json["currency"];
        if (request_type == REQUEST_ADD_CURRENCY) {
            process_add_currency(currency, client_id);
        } else if (request_type == REQUEST_ADD_CURRENCY_VALUE) {
            double value = client_json["value"];
            process_add_currency_value(currency, value, client_id);
        } else if (request_type == REQUEST_DEL_CURRENCY) {
            process_del_currency(currency, client_id);
        } else if (request_type == REQUEST_GET_CURRENCY_HISTORY) {
            process_currency_history(currency, client_id);
        } else {
            Logger::logger_inst->error("Client {} Unknown request type: {}", client_id, request_type);
            auto &&err_message = ERROR_PREFIX + std::string("Unknown request type") + MESSAGE_END;
            send_message(client_id, err_message);
        }

    } catch (nlohmann::json::parse_error &ex) {
        Logger::logger_inst->error("{} client {} json {}", ex.what(), client_id, json_string.data());
        auto &&err_message = ERROR_PREFIX + std::string("Incorrect json") + MESSAGE_END;
        send_message(client_id, err_message);
        return;
    }

}

void server::Server::handle_client_if_possible(int64_t client_id) {
    std::vector<std::string>* recv_buffer = &clients[client_id].receive_buffer;
    std::string received_message;
    for(auto&& chunk : *recv_buffer){
        received_message += chunk;
    }
    auto &&message_end = received_message.find(MESSAGE_END);
    if (message_end != std::string::npos) {
        auto &&message = received_message.substr(0, message_end);
        workers.enqueue(&Server::process_client_message, this, message, client_id);
    } else{
        Logger::logger_inst->error("Incorrect message received from client {}", client_id);
    }
    recv_buffer->clear();
}

void server::Server::send_message(int64_t client_id, std::string_view message) {
    std::vector<std::string>* send_buffer = &clients[client_id].send_buffer;
    send_buffer->clear();
    auto mess_size = message.size();
    int size = mess_size / UDP_PACKET_SIZE;
    int reminder = mess_size % UDP_PACKET_SIZE;
    if(reminder > 0) ++size;
    for (auto i = 0; i < size; ++i) {
        auto chunk = std::string(message.substr(i * UDP_PACKET_SIZE, UDP_PACKET_SIZE));
        auto packet = make_content_message(i, size, chunk);
        send_buffer->emplace_back(packet);
    }
    auto client_addr = clients[client_id].ip_addr;
    for(auto&& packet: *send_buffer) {
        send_chunk(client_addr, packet);
    }
    clients[client_id].send_index = 0;
}

void server::Server::send_chunk(int64_t client_id, std::string_view message){
    auto client_addr = clients[client_id].ip_addr;
    send_chunk(client_addr, message);
}

void server::Server::send_chunk(sockaddr_in& client_addr, std::string_view message){
    auto addr = reinterpret_cast<sockaddr*>(&client_addr);
    auto _message = std::string(message);
    auto &&send_stat = send_udp(server_socket, _message, addr);
    if (send_stat == SOCKET_ERROR) {
        Logger::logger_inst->error("Error in send {}", WSAGetLastError());
    }
}

int drop = 0;

void server::Server::client_message_chunk(int64_t client_id, int chunk_number, int total, std::string &message) {
    std::vector<std::string>* recv_buffer = &clients[client_id].receive_buffer;
    auto expected_message = recv_buffer->size();
    if(chunk_number <= expected_message) {
        if(drop && chunk_number == 1) {
            drop = 0;
            return;
        }
        auto packet = make_chunk_success_packet(chunk_number);
        send_chunk(client_id, packet);

    }
    if(chunk_number == 0){
        recv_buffer->clear();
    }
    if(chunk_number == expected_message) {
        recv_buffer->emplace_back(message);
    } else {
        Logger::logger_inst->info("Received incorrect packet {} expected {}", chunk_number, expected_message);
        return;
    }
    if(recv_buffer->size() == total && expected_message == total - 1){
        handle_client_if_possible(client_id);
        recv_buffer->clear();
    }
}

void server::Server::refresh_client_timeout(int64_t client_id) {
    clients[client_id].timer = 0;
}

void server::Server::client_message_status(int64_t client_id, int chunk_number, int status) {
    auto&& client = clients[client_id];
    if(client.send_index < 0) return;
    if(chunk_number == client.send_index) client.send_index++;
    if(client.send_index == client.send_buffer.size()) {
        client.send_index = -1;
        client.send_buffer.clear();
    }
}


int64_t get_id_for_client_info(sockaddr_in *client_addr) {
    int64_t result = client_addr->sin_addr.s_addr;
    return result << 32 | client_addr->sin_port;
}

int64_t server::Server::get_client_id(sockaddr_in *client_addr) {
    auto id = get_id_for_client_info(client_addr);
    if (clients.find(id) == std::end(clients)) {
        char ip_buf[INET_ADDRSTRLEN];
        inet_ntop(client_addr->sin_family, &client_addr->sin_addr, ip_buf, sizeof(ip_buf));
        std::string ip_str(ip_buf);
        lock_clients();
        clients[id] = Client{id, ip_str, *client_addr};
        unlock_clients();
        Logger::logger_inst->info("New connection from {} with id {}", ip_str, id);
    }
    return id;
}

int server::Server::handle_client_datagram(WSAEVENT event) {
    sockaddr client_info{};
    socklen_t client_info_size = sizeof(sockaddr);
    char receive_buffer[MESSAGE_SIZE];
    std::memset(receive_buffer, 0, MESSAGE_SIZE);
    auto bytes = recvfrom(server_socket, receive_buffer, MESSAGE_SIZE, 0, &client_info, &client_info_size);
    auto client_addr = reinterpret_cast<sockaddr_in*>(& client_info);
    if (bytes < 0) {
        auto err_code = GetLastError();
        if (err_code == WSAECONNRESET) return 1;
        Logger::logger_inst->error("Error in recv {}", err_code);
        return -1;
    }
    auto client_id = get_client_id(client_addr);
    refresh_client_timeout(client_id);

    if (bytes == 0)
        return 0;

    char message_type = receive_buffer[0];
    if (message_type == CHUNK_SUCCESS_MESSAGE) {
        int message_number = *(int *) (receive_buffer + 1);
        client_message_status(client_id, message_number, message_type);
        return 2;
    } else if (message_type == CONTENT_MESSAGE) {
        int message_number;
        int message_total;
        std::string message;
        std::tie(message_number, message_total, message) = parse_content_message(receive_buffer, bytes);
        client_message_chunk(client_id, message_number, message_total, message);
        return 3;
    } else {
        Logger::logger_inst->error("Unknown message type");
        return 4;
    }
    return -2;
}

void server::Server::serve_loop() {
    Logger::logger_inst->info("Server started on port {}", SERVER_PORT);
    while (!terminate){
        auto status = handle_client_datagram(nullptr);
        if(status < 0){
            Logger::logger_inst->error("Receive with status {}", status);
            break;
        }
    }
}

void server::Server::stop() {
    if (terminate) return;
    terminate = true;
    closesocket(server_socket);
    if (server_thread.joinable()) {
        server_thread.join();
    }
    if (timer_thread.joinable()) {
        timer_thread.join();
    }
    WSACleanup();
}

void server::Server::timer_loop() {
    auto timer = CreateWaitableTimer(nullptr, true, "Server timer");
    LARGE_INTEGER timer_time{};
    timer_time.QuadPart =  -1000000LL; // 100 ms
    while (!terminate) {
        SetWaitableTimer(timer, &timer_time, 0, nullptr, nullptr, 0);
        WaitForSingleObject(timer, INFINITE);
        std::vector<int64_t> client_ids;
        client_ids.reserve(clients.size());
        for (auto &&entry: clients) {
            client_ids.emplace_back(entry.first);
        }
        for (auto client_id: client_ids) {
            auto&& client = clients[client_id];
            ++client.timer;
            if (client.timer > TIMEOUT_DELTA) {
                close_client(client_id);
            } else if(client.send_index >= 0 && client.timer > 2){
                auto&& send_buffer = client.send_buffer;
                auto client_addr = client.ip_addr;
                for (int i = client.send_index; i < send_buffer.size(); ++i) {
                    auto&& packet = send_buffer[i];
                    send_chunk(client_addr, packet);
                }
            }
        }
    }
}

void server::Server::start() {
    terminate = false;
    server_thread = std::move(std::thread(&Server::serve_loop, this));
    timer_thread = std::move(std::thread(&Server::timer_loop, this));
}

bool server::Server::is_active() {
    return !terminate;
}

void server::Server::close_all_clients() {
    auto &&it = std::begin(clients);
    while (it != std::end(clients)) {
        close_client(it->first);
        it = std::begin(clients);
    }
}

std::string server::Server::list_clients() {
    std::stringstream out_string;
    out_string << "Clients connected:";
    for (auto &&entry: clients) {
        auto &&client_id = entry.first;
        auto &&client = entry.second;
        out_string << "\nid: " << client_id << " " << client.ip_str;
    }
    return out_string.str();
}
