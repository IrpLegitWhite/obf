cmake_minimum_required(VERSION 3.15)
project(obf_mba CXX)

set(CMAKE_CXX_STANDARD 17)

add_library(obf_mba SHARED
    src/MbaObf.cpp
)

target_include_directories(obf_mba PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
)

set_target_properties(obf_mba PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/plugins"
    OUTPUT_NAME "obf_mba"
)