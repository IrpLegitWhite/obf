cmake_minimum_required(VERSION 3.15)
project(obf_deadcode CXX)

set(CMAKE_CXX_STANDARD 17)

add_library(obf_deadcode SHARED
    src/DeadCodeObf.cpp
)

target_include_directories(obf_deadcode PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
)

set_target_properties(obf_deadcode PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/plugins"
    OUTPUT_NAME "obf_deadcode"
)