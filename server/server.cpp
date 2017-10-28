#include <sstream>
#include "server.h"
#include "logging/logger.h"
#include "utils/sockutils.h"

void server::Server::create_server_socket() {
    auto &&server_d = socket(AF_INET, SOCK_STREAM, 0);
    if (server_d < 0) {
        Logger::logger_inst->error("Cannot open socket");
        std::exit(1);
    }
    int enable_options = 1;
    setsockopt(server_d, SOL_SOCKET, SO_REUSEADDR, &enable_options, sizeof(enable_options));

    sockaddr_in server_address{};
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(SERVER_PORT);
    auto &&bind_addr = reinterpret_cast<const sockaddr *>(&server_address);
    auto &&bind_stat = bind(server_d, bind_addr, sizeof(server_address));
    if (bind_stat < 0) {
        Logger::logger_inst->error("Cannot bind");
        std::exit(1);
    }
    if (!socket_utils::set_socket_nonblock(server_d)) {
        Logger::logger_inst->error("Cannot set server socket nonblock");
        std::exit(1);
    }
    auto &&listen_stat = listen(server_d, 2);
    if (listen_stat == -1) {
        Logger::logger_inst->error("set server socket listen error");
        std::exit(1);
    }
    server_socket = server_d;
}

void server::Server::close_client(int client_d) {
    std::unique_lock<std::mutex> lock(clients_mutex);
    if(clients.find(client_d) == clients.end())
        return;
    auto &&client = clients[client_d];
    clients.erase(client_d);
    epoll_ctl(epoll_descriptor, EPOLL_CTL_DEL, client_d, &client.event);
    close(client.descriptor);
    client.is_active = false;
    lock.unlock();
    Logger::logger_inst->info("Client {} disconnected", client_d);
}

void server::Server::accept_client() {
    sockaddr_in client_addr{};
    auto &&client_addr_in = reinterpret_cast<sockaddr *>(&client_addr);
    auto &&client_addr_len = static_cast<socklen_t>(sizeof(sockaddr));
    auto &&client_d = accept(server_socket, client_addr_in, &client_addr_len);
    if (client_d == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            Logger::logger_inst->error("Accept failed");
        }
        return;
    }
    if (!socket_utils::set_socket_nonblock(client_d)) {
        Logger::logger_inst->error("Cannot set client socket {} nonblock", client_d);
        return;
    }
    epoll_event event{};
    event.data.fd = client_d;
    event.events = EPOLLIN;
    auto &&ctl_stat = epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, client_d, &event);
    if (ctl_stat == -1) {
        Logger::logger_inst->error("epoll_ctl failed");
        return;
    }
    std::string client_info = inet_ntoa(client_addr.sin_addr);
    std::unique_lock<std::mutex> lock(clients_mutex);
    clients[client_d] = Client(client_d, event, client_info);
    lock.unlock();
    Logger::logger_inst->info("New connection from {} on socket {}", client_info, client_d);
}

void server::Server::read_client_data(int client_id) {
    char read_buffer[MESSAGE_SIZE];
    auto &&count = read(client_id, read_buffer, MESSAGE_SIZE);
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


void send_message(int client_id, std::string_view message){
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
    } else {
        Logger::logger_inst->error("Client {} Unknown message type: {}", client_id, message);
        auto&& err_message = ERROR_PREFIX + std::string("Unknown message type");
        send_message(client_id, err_message);
    }
}

void server::Server::process_client_command(std::string_view command, int client_id) {
    Logger::logger_inst->info("Command from client {}: {}", client_id, command.data());
    if (command == "disconnect") {
        close_client(client_id);
    }
}


void server::Server::process_client_text(std::string_view text, int client_id) {
    Logger::logger_inst->info("Text from client {}: {}", client_id, text.data());
    auto &&to_send = std::string(text) + MESSAGE_END;
    send_message(client_id, to_send);
}


void server::Server::epoll_loop() {
    epoll_descriptor = epoll_create(1);
    if (epoll_descriptor == -1) {
        Logger::logger_inst->error("Cannot create epoll descriptor");
        std::exit(1);
    }
    epoll_event event{};
    event.events = EPOLLIN | EPOLLHUP;
    event.data.fd = server_socket;
    auto &&ctl_stat = epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, server_socket, &event);
    if (ctl_stat == -1) {
        Logger::logger_inst->error("epoll_ctl failed");
        std::exit(1);
    }
    std::array<epoll_event, 10> events{};
    Logger::logger_inst->info("Server started on port {}", SERVER_PORT);
    while (!terminate) {
        auto &&event_cnt = epoll_wait(epoll_descriptor, events.data(), 10, 1000);
        for (auto &&i = 0; i < event_cnt; ++i) {
            auto &&evt = events[i];
            if (evt.events & EPOLLERR) {
                Logger::logger_inst->error("Epoll error for socket {}", evt.data.fd);
                close_client(evt.data.fd);
            }
            if (evt.events & EPOLLHUP) {
                Logger::logger_inst->info("client socket closed {}", evt.data.fd);
                close_client(evt.data.fd);
            }
            if (evt.events & EPOLLIN) {
                if (evt.data.fd == server_socket) accept_client();
                else {
                    read_client_data(evt.data.fd);
                    handle_client_if_possible(evt.data.fd);
                }
            }
        }
    }
}

void server::Server::stop() {
    if (terminate) return;
    terminate = true;
    close(server_socket);
    if (server_thread.joinable()) {
        server_thread.join();
    }
}

void server::Server::start() {
    terminate = false;
    server_thread = std::move(std::thread(&Server::epoll_loop, this));
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
    for (auto && [client_id, client]: clients) {
        out_string << "\nid: " << client_id << " " << client.client_ip_addr;
    }
    return out_string.str();
}
