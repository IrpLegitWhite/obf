#pragma once

#include <string>
#include <windows.h>

namespace obf {
    namespace gui {

        class FileDialog {
        public:
            static std::string openFile(const wchar_t* title,
                const wchar_t* filter);
            static std::string saveFile(const wchar_t* title);
        };

    } // namespace gui
} // namespace obf