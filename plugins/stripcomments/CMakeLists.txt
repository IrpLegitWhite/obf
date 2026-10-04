cmake_minimum_required(VERSION 3.15)
project(obf_stripcomments CXX)

set(CMAKE_CXX_STANDARD 17)

add_library(obf_stripcomments SHARED
    src/StripCommentsObf.cpp
)

target_include_directories(obf_stripcomments PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
)

if(MSVC)
    target_compile_options(obf_stripcomments PRIVATE /W4 /wd5220 /wd4239)
    target_compile_definitions(obf_stripcomments PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(obf_stripcomments PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/plugins"
    OUTPUT_NAME "obf_stripcomments"
)