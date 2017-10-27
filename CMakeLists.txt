cmake_minimum_required(VERSION 3.8)
project(EchoServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -lSQLiteCpp -lsqlite3")

set(logger logger.h logger.cpp)
set(thread_pool thread_pool/ThreadPool.h)
set(server_cls server.cpp server.h)
set(finance_db FinanceDb.h FinanceDb.cpp)

add_executable(server server_main.cpp defines.h sockutils.h ${logger} ${thread_pool} ${server_cls} ${finance_db})
add_executable(client client.cpp)
