cmake_minimum_required(VERSION 3.10)
project(ICCAD)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(ICCAD
    src/main.cpp
    src/parser.cpp
    src/parser.hpp
    src/signal_table.cpp
    src/signal_table.hpp
    src/transaction.hpp
    src/transaction.cpp)
