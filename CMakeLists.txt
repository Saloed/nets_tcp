cmake_minimum_required(VERSION 3.8)
project(EchoServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -lSQLiteCpp -lsqlite3")

add_executable(server server.cpp defines.h utils.h database.cpp logger.h serve.cpp sockutils.h)

add_executable(client client.cpp defines.h utils.h)
add_executable(db_test database.cpp logger.h)