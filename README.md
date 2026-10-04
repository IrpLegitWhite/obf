# C++ Obfuscator

GUI-обфускатор исходного кода с поддержкой множества языков и плагинной архитектурой.

## Возможности

-  **Поддержка языков**: C, C++, C#, DLL-проекты, Python, JavaScript, TypeScript, Java, Rust, Go, PHP, Ruby, Lua, Perl, Kotlin, Swift, HTML, CSS
-  **Плагинная архитектура** — каждая обфускация это отдельная DLL
-  **Современный GUI** на ImGui + DirectX 11
-  **Тёмная и светлая темы**
-  **UTF-8 редактор** с подсветкой синтаксиса
-  **Асинхронная обработка** — не блокирует интерфейс

## Требования

- **Windows 10/11** (x64)
- **Visual Studio 2022** (или Build Tools с MSVC)
- **CMake 3.15+**
- **DirectX 11** (встроен в Windows)

## Сборка

### 1. Клонирование

```bat
git clone <repo-url> D:\Minecraft\obf
cd D:\Minecraft\obf
```

### 2. Сборка

```bat
mkdir out\build
cd out\build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

**Или** открой папку в **Visual Studio** (`File → Open → Folder`) — VS сам всё настроит через CMake.

### 3. Результат

```
bin\
├── obf.exe                  ← GUI-обфускатор
└── plugins\
    ├── obf_rename.dll
    ├── obf_polymorph.dll
    ├── obf_visual_noise.dll
    ├── obf_protect.dll
    ├── obf_broken_bytes.dll
    ├── obf_dll_noise.dll
    ├── obf_html_noise.dll
    └── ...
```

## Использование

### Запуск

```bat
bin\obf.exe
```

### Workflow

1. **Обзор** → выбери **исходник** (`.cpp`, `.py`, `.js`, ...)
2. Язык определится **автоматически**
3. **Фильтр** — отфильтруй плагины по языку
4. **Включи галочки** нужных плагинов
5. **Запуск** — жди обработки
6. **Сохранить** → получишь обфусцированный файл

###  Важно

**Обфусцируй ИСХОДНИК, а не собранный `.dll`!**

**Правильно:**
```
test_dll.cpp  →  обфускация  →  test_dll_obf.cpp  →  компиляция  →  test_dll.dll
```

**Неправильно:**
```
test_dll.dll  →  обфускация  ←  НЕ РАБОТАЕТ (это бинарник)
```

## Плагины

### C / C++ / C# / DLL

| Плагин | Что делает |
|---|---|
| `rename` | Переименование идентификаторов в `_0x...` |
| `polymorph` | Junk-код, dead-функции, split чисел |
| `visual_noise` | Визуальный хаос: гомоглифы, RTL, broken bytes |
| `protect` | Анти-отладка (безопасная, через флаг) |
| `dll_noise` | Ложные экспорты, COM, `.def`, GUID |
| `broken_bytes` | Невалидный UTF-8 в комментариях |
| `mba` | Mixed Boolean-Arithmetic |
| `deadcode` | Мёртвый код |
| `opaque` | Opaque predicates |
| `antidebug` | Анти-отладка |
| `controlflow` | Обфускация потока управления |
| `strings` | Шифрование строк |
| `stripcomments` | Удаление комментариев |

### HTML / JS / CSS

| Плагин | Что делает |
|---|---|
| `html_noise` | Комментарии-ловушки, фейковые `data-*`, `<script>`, `#if 0` блоки |

### Другие языки

`python_strings`, `python_mba`, `js_strings`, `js_mba`, `java_strings`, `java_mba`, `rust_strings`, `rust_mba`, `go_strings`, `go_mba`, `php_strings`, `ruby_strings`, `lua_strings`, `perl_strings`, `kotlin_strings`, `swift_strings`

## Создание своего плагина

### 1. Структура

```
plugins/my_plugin\
├── CMakeLists.txt
└── src\
    └── MyPlugin.cpp
```

### 2. Минимальный плагин

```cpp
#include "../../../include/plugin/IPlugin.h"
#include <cstring>

extern "C" __declspec(dllexport) const char* obf_name() {
    return "my_plugin";
}

extern "C" __declspec(dllexport) const char* obf_description() {
    return "My custom obfuscation";
}

extern "C" __declspec(dllexport) int obf_api_version() {
    return OBFS_PLUGIN_API_VERSION;
}

extern "C" __declspec(dllexport) const char* obf_languages() {
    return "cpp,c,dll";
}

extern "C" __declspec(dllexport) int obf_can_apply(const char* code) {
    return code ? 1 : 0;
}

extern "C" __declspec(dllexport) char* obf_apply(const char* code, size_t size,
    size_t* out_size) {
    // Твоя логика
    char* r = (char*)std::malloc(size + 1);
    std::memcpy(r, code, size + 1);
    *out_size = size;
    return r;
}

extern "C" __declspec(dllexport) void obf_free(char* ptr) {
    std::free(ptr);
}
```

### 3. `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.15)
project(obf_my_plugin CXX)

set(CMAKE_CXX_STANDARD 17)

add_library(obf_my_plugin SHARED src/MyPlugin.cpp)

target_include_directories(obf_my_plugin PRIVATE "${CMAKE_SOURCE_DIR}/include")

if(MSVC)
    target_compile_options(obf_my_plugin PRIVATE /W4)
    target_compile_definitions(obf_my_plugin PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(obf_my_plugin PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/plugins"
    OUTPUT_NAME "obf_my_plugin"
)
```

### 4. Добавь в корневой `CMakeLists.txt`

```cmake
add_subdirectory(plugins/my_plugin)
```

## Порядок применения (рекомендуемый)

```
rename → polymorph → visual_noise → dll_noise → protect → broken_bytes
```

**Правила:**
- `rename` — **первым** (переименует пользовательский код)
- `broken_bytes` — **последним** (вставит невалидные байты)
- `protect` — **после** `rename` (использует `_jnk_*` префикс, не тронет)

## Архитектура

```
src\
├── main.cpp
├── AppWindow.cpp              ← GUI, логика обработки
├── core\
│   ├── File.cpp               ← чтение/запись файлов
│   ├── Logger.cpp             ← логирование
│   ├── Config.cpp             ← конфиг
│   └── Utils.cpp
├── gui\
│   ├── FileDialog.cpp         ← диалог выбора файлов
│   └── LogBuffer.cpp          ← буфер логов
└── plugin\
    ├── PluginLoader.cpp       ← загрузка DLL-плагинов
    ├── PluginManager.cpp
    └── IPlugin.h              ← интерфейс плагина

include\                       ← публичные заголовки
third_party\
├── imgui\                     ← GUI-библиотека
└── ImGuiColorTextEdit\        ← UTF-8 редактор (pthom fork)

plugins\                       ← все плагины
bin\                           ← выходная директория
├── obf.exe
└── plugins\                   ← собранные плагины
```

## API плагина

```cpp
struct PluginInfo {
    std::string name;            // "rename"
    std::string description;     // "C/C++/C#: rename user identifiers"
    std::string languages;       // "cpp,c,csharp,dll"
    int api_version;             // OBFS_PLUGIN_API_VERSION
    void* fn_can_apply;          // obf_can_apply()
    void* fn_apply;              // obf_apply()
    void* fn_free;               // obf_free()
};
```

### `obf_can_apply`

```cpp
extern "C" int obf_can_apply(const char* code);
```
Возвращает `1` если плагин **может применить** к коду, `0` если нет.

### `obf_apply`

```cpp
extern "C" char* obf_apply(const char* code, size_t size, size_t* out_size);
```
Возвращает **новую строку** (выделенную через `malloc`), `out_size` — её длина.

### `obf_free`

```cpp
extern "C" void obf_free(char* ptr);
```
Освобождает память, выделенную в `obf_apply`.

## Лицензия

MIT