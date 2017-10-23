#pragma once

#include <spdlog/spdlog.h>

namespace Logger{
    auto&& logger = spdlog::basic_logger_mt("server_logger", "server_log.txt");
}