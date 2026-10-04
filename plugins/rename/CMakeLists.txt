cmake_minimum_required(VERSION 3.15)
project(obf_rename CXX)

set(CMAKE_CXX_STANDARD 17)

add_library(obf_rename SHARED
    src/RenameObf.cpp
)

target_include_directories(obf_rename PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
)

set_target_properties(obf_rename PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/plugins"
    OUTPUT_NAME "obf_rename"
)