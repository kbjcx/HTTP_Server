cmake_minimum_required(VERSION 3.16)
project(HTTP_Server)

set(CMAKE_CXX_STANDARD 11)

include_directories(./)

set(server locker.cpp thread_pool.cpp)

add_executable(HTTP_Server ${server})
