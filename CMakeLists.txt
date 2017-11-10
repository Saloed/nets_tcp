cmake_minimum_required(VERSION 3.8)
project(EchoServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -lSQLiteCpp -lsqlite3")

# libs
include_directories(libs)
set(THREAD_POOL_SRC libs/thread_pool/ThreadPool.h)
set(JSON_SRC libs/json/src/json.hpp)

# shared
include_directories(shared)
set(LOGGER_SRC shared/logging/logger.h shared/logging/logger.cpp)
set(DEFINES shared/defines.h)

set(FINANCE_DB_SRC server/database/FinanceDb.h server/database/FinanceDb.cpp)

set(SERVER_SRC server/server.cpp server/server.h server/utils/sockutils.h)
set(SERVER_SRC ${SERVER_SRC} ${DEFINES} ${FINANCE_DB_SRC} ${LOGGER_SRC} ${JSON_SRC} ${THREAD_POOL_SRC})

add_executable(server server/server_main.cpp ${SERVER_SRC})

set(CLIENT_SRC ${DEFINES} client/client_defs.h)
add_executable(client client/client.cpp ${CLIENT_SRC})
