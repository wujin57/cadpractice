cmake_minimum_required(VERSION 3.10)
project(APB_Recognizer CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include_directories(src)

set(SOURCES
    src/main.cpp
    src/vcd_parser.cpp
    src/vcd_parser.hpp
    src/apb_analyzer.cpp
    src/apb_analyzer.hpp
    src/apb_types.hpp
    src/error_logger.cpp
    src/error_logger.hpp
    src/report_generator.cpp
    src/report_generator.hpp
    src/signal_manager.cpp
    src/signal_manager.hpp
    src/statistics.cpp
    src/statistics.hpp
    src/report_generator.cpp
    src/report_generator.hpp)



add_executable(${PROJECT_NAME} ${SOURCES})
