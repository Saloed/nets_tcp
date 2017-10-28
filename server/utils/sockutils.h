#ifndef _SOCKET_UTILS
#define _SOCKET_UTILS
#include "logging/logger.h"

namespace socket_utils {
    bool set_socket_nonblock(int &sock) {
        auto &&flags = fcntl(sock, F_GETFL, 0);
        if (flags == -1) {
            Logger::logger_inst->error("fcntl failed (F_GETFL)");
            return false;
        }

        flags |= O_NONBLOCK;
        auto &&stat = fcntl(sock, F_SETFL, flags);
        if (stat == -1) {
            Logger::logger_inst->error("fcntl failed (F_SETFL)");
            return false;
        }
        return true;
    }
}
#endif