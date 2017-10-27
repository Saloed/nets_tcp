cmake_minimum_required(VERSION 3.8)
project(EchoServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -lSQLiteCpp -lsqlite3")

set(logger shared/logging/logger.h shared/logging/logger.cpp)
set(thread_pool thread_pool/ThreadPool.h)
set(server_cls server/server.cpp server/server.h)
set(finance_db server/database/FinanceDb.h server/database/FinanceDb.cpp)

add_executable(server server/server_main.cpp shared/defines.h server/utils/sockutils.h ${logger} ${thread_pool} ${server_cls} ${finance_db})
add_executable(client client/client.cpp)
