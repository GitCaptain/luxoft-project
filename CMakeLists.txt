cmake_minimum_required(VERSION 3.14)
project(luxoft)

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -pthread")
set(CMAKE_CXX_STANDARD 11)

add_executable(luxoft main.cpp)