
#include "logger.h"

namespace socket_utils {
    auto set_socket_nonblock(int &sock) {
        auto &&flags = fcntl(sock, F_GETFL, 0);
        if (flags == -1) {
            Logger::logger->error("fcntl failed (F_GETFL)");
            return false;
        }

        flags |= O_NONBLOCK;
        auto &&stat = fcntl(sock, F_SETFL, flags);
        if (stat == -1) {
            Logger::logger->error("fcntl failed (F_SETFL)");
            return false;
        }
        return true;
    }
}