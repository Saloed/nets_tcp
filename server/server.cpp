#include <sstream>
#include <WS2tcpip.h>
#include "server.h"
#include "logging/logger.h"
#include "json/src/json.hpp"


void server::Server::create_server_socket() {
    WSADATA wsa{};
    auto init_status = WSAStartup(MAKEWORD(2,2),&wsa);

    if (init_status != 0) {
        Logger::logger_inst->error("WSA init failed with code {}", WSAGetLastError());
        std::exit(EXIT_FAILURE);
    }
    auto server_d = socket(AF_INET , SOCK_DGRAM , IPPROTO_UDP );
    if(server_d == INVALID_SOCKET){
        Logger::logger_inst->error("Error in socket creation {}", WSAGetLastError());
        std::exit(EXIT_FAILURE);
    }
    BOOL fFlag = TRUE;
    auto reuse_status = setsockopt(server_d,SOL_SOCKET, SO_REUSEADDR, (char *)&fFlag, sizeof(fFlag));
    if (reuse_status == SOCKET_ERROR) {
        Logger::logger_inst->error("Error in set socket reuse addr{}", WSAGetLastError());
        std::exit(EXIT_FAILURE);
    }
    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( SERVER_PORT );

    auto bind_status = bind(server_d , (sockaddr *)&server , sizeof(sockaddr_in));
    if(bind_status == SOCKET_ERROR){
        Logger::logger_inst->error("Error in socket bind {}", WSAGetLastError());
        std::exit(EXIT_FAILURE);
    }

    server_socket = server_d;
    read_event_d = CreateEvent(NULL, TRUE, TRUE, "ReadEvent");
    completion_port = CreateIoCompletionPort (INVALID_HANDLE_VALUE, NULL, 0, 1);
    if(!completion_port){
        Logger::logger_inst->error("Error in completion port creation");
        std::exit(EXIT_FAILURE);
    }
    CreateIoCompletionPort((HANDLE)server_d, completion_port, (DWORD)server_d, 2);
    Logger::logger_inst->info("Server initialized");
}

void server::Server::close_client(int client_d) {
    std::unique_lock<std::mutex> lock(clients_mutex);
    if (clients.find(client_d) == clients.end())
        return;
    auto &&client = clients[client_d];
    clients.erase(client_d);
    closesocket(client.descriptor);
    client.is_active = false;
    lock.unlock();
    Logger::logger_inst->info("Client {} disconnected", client_d);
}

void server::Server::accept_client() {
//    sockaddr_in client_addr{};
//    auto &&client_addr_in = reinterpret_cast<sockaddr *>(&client_addr);
//    auto &&client_addr_len = static_cast<socklen_t>(sizeof(sockaddr));
//    auto &&client_d = accept(server_socket, client_addr_in, &client_addr_len);
//    if (client_d == -1) {
//        if (errno != EAGAIN && errno != EWOULDBLOCK) {
//            Logger::logger_inst->error("Accept failed");
//        }
//        return;
//    }
//    if (!socket_utils::set_socket_nonblock(client_d)) {
//        Logger::logger_inst->error("Cannot set client socket {} nonblock", client_d);
//        return;
//    }
//
//    std::string client_info = inet_ntoa(client_addr.sin_addr);
//    std::unique_lock<std::mutex> lock(clients_mutex);
//    clients[client_d] = Client(client_d, event, client_info);
//    lock.unlock();
//    Logger::logger_inst->info("New connection from {} on socket {}", client_info, client_d);
}

void server::Server::read_client_data(int client_id) {
    char read_buffer[MESSAGE_SIZE];
    auto &&count = 0; //read(client_id, read_buffer, MESSAGE_SIZE);
    if (count == -1 && errno == EAGAIN) return;
    if (count == -1) Logger::logger_inst->error("Error in read for socket {}", client_id);
    if (count <= 0) {
        close_client(client_id);
        return;
    }
    auto &&client = clients[client_id];
    client.receive_buffer.append(read_buffer, static_cast<unsigned long>(count));
}

void server::Server::handle_client_if_possible(int client_id) {
    auto &&client = clients[client_id];
    auto &&message_end = client.receive_buffer.find(MESSAGE_END);
    if (message_end != std::string::npos) {
        auto &&message = client.receive_buffer.substr(0, message_end);
        client.receive_buffer.erase(0, message_end + strlen(MESSAGE_END));
        workers.enqueue(&Server::process_client_message, this, message, client_id);
    }
}


void send_message(int client_id, std::string_view message) {
    auto &&send_stat = send(client_id, message.data(), message.length(), 0);
    if (send_stat == -1) {
        Logger::logger_inst->error("Error in send for id {}", client_id);
    }
}


void server::Server::process_client_message(std::string &message, int client_id) {
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


void server::Server::process_client_text(std::string_view text, int client_id) {
    Logger::logger_inst->info("Text from client {}: {}", client_id, text.data());
    auto &&to_send = std::string(text) + MESSAGE_END;
    send_message(client_id, to_send);
}


void server::Server::process_add_currency(std::string &currency, int client_id) {
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

void server::Server::process_add_currency_value(std::string &currency, double value, int client_id) {
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

void server::Server::process_del_currency(std::string &currency, int client_id) {
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

void server::Server::process_list_all_currencies(int client_id) {
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

void server::Server::process_currency_history(std::string &currency, int client_id) {
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

void server::Server::process_client_command(std::string_view command, int client_id) {
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


void server::Server::process_client_json(std::string_view json_string, int client_id) {
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

void server::Server::client_message_chunk(sockaddr_storage client_info, int chunk_number, int total, std::string& message){

}

void server::Server::refresh_client_timeout(sockaddr_storage client_info){

}

void server::Server::client_message_status(sockaddr_storage client_info, int chunk_number, int status){

}


int64_t get_id_for_client_info(sockaddr_in client_info){
    int64_t result = client_info.sin_addr.s_addr;
    return result << 32 | client_info.sin_port;
}


void server::Server::handle_client_datagram(WSAEVENT event){
    WSANETWORKEVENTS network_events{};
    sockaddr_storage client_info{};
    socklen_t client_info_size = sizeof(sockaddr_storage);
    char receive_buffer[MESSAGE_SIZE + 1];
    WSAEnumNetworkEvents(server_socket, event, &network_events);
    auto bytes = recvfrom(server_socket, receive_buffer, MESSAGE_SIZE, 0,
                     (sockaddr*) &client_info, &client_info_size);

    if(bytes < 0){
        auto err_code = GetLastError();
        if(err_code == WSAECONNRESET) return;
        Logger::logger_inst->error("Error in recv {}", err_code);
        terminate = true;
    }

    if(bytes == 0){
        return refresh_client_timeout(client_info);
    }
    char message_type = receive_buffer[0];
    if(message_type == CHUNK_REQUEST_MESSAGE || message_type == CHUNK_SUCCESS_MESSAGE){
        int message_number = *(int *)(receive_buffer + 1);
        return client_message_status(client_info, message_number, message_type);
    } else if (message_type == CONTENT_MESSAGE) {
        auto * as_int_array = (int *)(receive_buffer + 1);
        int message_number = as_int_array[0];
        int message_total  = as_int_array[1];
        receive_buffer[bytes] = '\0';
        char* content = receive_buffer + 1 + 2 * sizeof(int);
        std::string message(content);
        return client_message_chunk(client_info, message_number, message_total, message);
    } else {
        Logger::logger_inst->error("Unknown message type");
    }
}

void server::Server::disconnect_time_outed(){

}

void server::Server::serve_loop() {
    Logger::logger_inst->info("Server started on port {}", SERVER_PORT);
    WSAEVENT events[1] = {WSACreateEvent()};
    WSAEventSelect(server_socket, events[0], FD_READ);
    while(!terminate)
    {
        auto result = WSAWaitForMultipleEvents(1, events, FALSE, 2000, FALSE);
        switch(result)
        {
            case WAIT_TIMEOUT:
                disconnect_time_outed();
                break;
            case WAIT_OBJECT_0:
                handle_client_datagram(events[0]);
                break;
            case WAIT_FAILED:{
                Logger::logger_inst->error("Select failed {}", result);
                terminate = true;
                break;
            }
            default:{
                Logger::logger_inst->error("Unexpected select event {}", result);
               terminate = true;
            }
        }
    }
    CloseHandle(events[0]);
}

void server::Server::stop() {
    if (terminate) return;
    terminate = true;
    closesocket(server_socket);
    if (server_thread.joinable()) {
        server_thread.join();
    }
    WSACleanup();
}

void server::Server::start() {
    terminate = false;
    server_thread = std::move(std::thread(&Server::serve_loop, this));
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
    for (auto && entry: clients) {
        auto&& client_id = entry.first;
        auto&& client = entry.second;
        out_string << "\nid: " << client_id << " " << client.client_ip_addr;
    }
    return out_string.str();
}
