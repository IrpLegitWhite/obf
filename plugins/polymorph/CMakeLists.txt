cmake_minimum_required(VERSION 3.15)
project(obf_polymorph CXX)

set(CMAKE_CXX_STANDARD 17)

add_library(obf_polymorph SHARED
    src/PolymorphObf.cpp
)

target_include_directories(obf_polymorph PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
)

if(MSVC)
    target_compile_options(obf_polymorph PRIVATE /W4 /wd5220 /wd4239)
    target_compile_definitions(obf_polymorph PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(obf_polymorph PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/plugins"
    OUTPUT_NAME "obf_polymorph"
)