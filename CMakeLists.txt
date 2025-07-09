cmake_minimum_required(VERSION 2.8.12)
project(APB_Recognizer CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXE_LINKER_FLAGS "-static")
include_directories(src)

set(SOURCES
    src/main.cpp
    src/vcd_parser.cpp
    src/vcd_parser.hpp
    src/apb_analyzer.cpp
    src/apb_analyzer.hpp
    src/apb_types.hpp
    src/report_generator.cpp
    src/report_generator.hpp
    src/signal_manager.cpp
    src/signal_manager.hpp
    src/statistics.cpp
    src/statistics.hpp
    src/report_generator.cpp
    src/report_generator.hpp)



add_executable(APB_Recognizer ${SOURCES})
