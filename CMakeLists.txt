cmake_minimum_required(VERSION 3.16)
project(HTTP_Server)

set(CMAKE_CXX_STANDARD 11)

link_libraries(pthread)
include_directories(./)

set(server main.cpp locker.cpp http_connection.cpp timer.cpp)

add_executable(server ${server})
