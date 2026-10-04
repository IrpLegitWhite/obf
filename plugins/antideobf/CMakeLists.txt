cmake_minimum_required(VERSION 3.15)
project(obf_antideobf CXX)

set(CMAKE_CXX_STANDARD 17)

add_library(obf_antideobf SHARED
    src/AntiDeobfObf.cpp
)

target_include_directories(obf_antideobf PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
)

if(MSVC)
    target_compile_options(obf_antideobf PRIVATE /W4 /wd5220 /wd4239)
    target_compile_definitions(obf_antideobf PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(obf_antideobf PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/plugins"
    OUTPUT_NAME "obf_antideobf"
)