cmake_minimum_required(VERSION 3.10)
project(ICCAD)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(ICCAD
    main.cpp
    parser.cpp
    parsr.hpp
    signal_table.cpp
    signal_table.hpp
    transaction.cpp
    transaction.hpp
        transaction.cpp)
