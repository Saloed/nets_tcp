cmake_minimum_required(VERSION 3.8)
project(EchoServer)

set(CMAKE_CXX_STANDARD 17)

# libs
include_directories(libs)
set(THREAD_POOL_SRC libs/thread_pool/ThreadPool.h)
set(JSON_SRC libs/json/src/json.hpp)
add_subdirectory(libs/SQLiteCpp)
add_subdirectory(libs/spdlog)

if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest")
endif()

# shared
include_directories(shared)
set(LOGGER_SRC shared/logging/logger.h shared/logging/logger.cpp)
set(DEFINES shared/defines.h)

set(FINANCE_DB_SRC server/database/FinanceDb.h server/database/FinanceDb.cpp)

set(SERVER_SRC server/server.cpp server/server.h)
set(SERVER_SRC ${SERVER_SRC} ${DEFINES} ${FINANCE_DB_SRC} ${LOGGER_SRC} ${JSON_SRC} ${THREAD_POOL_SRC})

add_executable(server server/server_main.cpp ${SERVER_SRC})

set(CLIENT_SRC ${DEFINES} client/client_defs.h)
add_executable(client client/client.cpp ${CLIENT_SRC})
add_executable(db_init server/database/initialize.cpp ${FINANCE_DB_SRC} ${LOGGER_SRC})

target_link_libraries(server SQLiteCpp sqlite3 spdlog ws2_32)
target_link_libraries(db_init SQLiteCpp sqlite3 spdlog)

#add_executable(tmp tmp.cpp)