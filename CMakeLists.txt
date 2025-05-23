cmake_minimum_required(VERSION 3.10)
project(ICCAD)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(ICCAD
    src/main.cpp
    src/apb_parser.cpp
    src/apb_parser.hpp
    src/apb_analyzer.cpp
    src/apb_analyzer.hpp
    src/apb_types.hpp
    src/error_logger.cpp
    src/error_logger.hpp
    src/vcd_parser.cpp
    src/report_generator.cpp
    src/report_generator.hpp
    src/signal_manager.cpp
    src/signal_manager.hpp
    src/statistics.cpp
    src/statistics.hpp)

