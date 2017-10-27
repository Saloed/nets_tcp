#ifndef _LOGGER
#define _LOGGER
#include <spdlog/spdlog.h>

class Logger {
public:
    static std::shared_ptr<spdlog::logger> logger_inst;

};
#endif
