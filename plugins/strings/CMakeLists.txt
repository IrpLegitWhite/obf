cmake_minimum_required(VERSION 3.15)
project(obf_strings CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(obf_strings SHARED
    src/StringObf.cpp
)

target_include_directories(obf_strings PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(MSVC)
    target_compile_options(obf_strings PRIVATE /W4 /wd5220 /wd4239)
    target_compile_definitions(obf_strings PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(obf_strings PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/plugins"
    OUTPUT_NAME "obf_strings"
)