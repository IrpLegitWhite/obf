cmake_minimum_required(VERSION 3.15)
project(obf CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# ============================================================
# Основное приложение
# ============================================================
file(GLOB_RECURSE APP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# ============================================================
# ImGui
# ============================================================
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")
set(IMGUI_SOURCES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_dx11.cpp"
)

# ============================================================
# ImGuiColorTextEdit (UTF-8 fork от pthom)
# ============================================================
set(TEXTEDIT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuiColorTextEdit")
set(TEXTEDIT_SOURCES
    "${TEXTEDIT_DIR}/TextEditor.cpp"
)

# Проверка наличия файлов
if(NOT EXISTS "${TEXTEDIT_DIR}/TextEditor.h")
    message(FATAL_ERROR "TextEditor.h not found at: ${TEXTEDIT_DIR}/TextEditor.h")
endif()
if(NOT EXISTS "${TEXTEDIT_DIR}/TextEditor.cpp")
    message(FATAL_ERROR "TextEditor.cpp not found at: ${TEXTEDIT_DIR}/TextEditor.cpp")
endif()

add_executable(obf
    ${APP_SOURCES}
    ${IMGUI_SOURCES}
    ${TEXTEDIT_SOURCES}
)

target_include_directories(obf PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
    "${TEXTEDIT_DIR}"
)

if(MSVC)
    target_compile_options(obf PRIVATE /W4 /wd5220 /wd4239)
    target_compile_definitions(obf PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
    target_link_options(obf PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
else()
    target_compile_options(obf PRIVATE -Wall -Wextra)
endif()

if(WIN32)
    target_link_libraries(obf PRIVATE
        d3d11 dxgi dwmapi comctl32 msimg32
    )
endif()

set_target_properties(obf PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)

# ============================================================
# Плагины — C / C++ / C#
# ============================================================
add_subdirectory(plugins/strings)
add_subdirectory(plugins/mba)
add_subdirectory(plugins/rename)
add_subdirectory(plugins/deadcode)
add_subdirectory(plugins/opaque)
add_subdirectory(plugins/antidebug)
add_subdirectory(plugins/controlflow)
add_subdirectory(plugins/stripcomments)
add_subdirectory(plugins/polymorph)
add_subdirectory(plugins/VisualNoise)
add_subdirectory(plugins/protect)
add_subdirectory(plugins/BrokenBytes)
add_subdirectory(plugins/dll_noise)
add_subdirectory(plugins/html_noise)
add_subdirectory(plugins/test)

# ============================================================
# Плагины — Python
# ============================================================
add_subdirectory(plugins/python_strings)
add_subdirectory(plugins/python_mba)

# ============================================================
# Плагины — JavaScript / TypeScript
# ============================================================
add_subdirectory(plugins/js_strings)
add_subdirectory(plugins/js_mba)

# ============================================================
# Плагины — Java
# ============================================================
add_subdirectory(plugins/java_strings)
add_subdirectory(plugins/java_mba)

# ============================================================
# Плагины — Rust
# ============================================================
add_subdirectory(plugins/rust_strings)
add_subdirectory(plugins/rust_mba)

# ============================================================
# Плагины — Go
# ============================================================
add_subdirectory(plugins/go_strings)
add_subdirectory(plugins/go_mba)

# ============================================================
# Плагины — PHP
# ============================================================
add_subdirectory(plugins/php_strings)

# ============================================================
# Плагины — Ruby
# ============================================================
add_subdirectory(plugins/ruby_strings)

# ============================================================
# Плагины — Lua
# ============================================================
add_subdirectory(plugins/lua_strings)

# ============================================================
# Плагины — Perl
# ============================================================
add_subdirectory(plugins/perl_strings)

# ============================================================
# Плагины — Kotlin
# ============================================================
add_subdirectory(plugins/kotlin_strings)

# ============================================================
# Плагины — Swift
# ============================================================
add_subdirectory(plugins/swift_strings)