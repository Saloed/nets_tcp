#include "logger.h"

//std::shared_ptr<spdlog::logger> Logger::logger_inst = spdlog::basic_logger_mt("server_logger", "server_log.txt");
std::shared_ptr<spdlog::logger> Logger::logger_inst = spdlog::stdout_logger_mt("server_logger");
