#include "../../include/gui/FileDialog.h"
#include <commdlg.h>
#include <vector>

namespace obf {
    namespace gui {

        static std::string wideToUtf8(const std::wstring& w) {
            if (w.empty()) return {};
            int n = ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                nullptr, 0, nullptr, nullptr);
            std::string s(n, 0);
            ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                s.data(), n, nullptr, nullptr);
            return s;
        }

        std::string FileDialog::openFile(const wchar_t* title, const wchar_t* filter) {
            wchar_t buf[MAX_PATH] = {};
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = nullptr;
            ofn.lpstrFilter = filter;
            ofn.lpstrFile = buf;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrTitle = title;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

            if (::GetOpenFileNameW(&ofn)) {
                return wideToUtf8(buf);
            }
            return {};
        }

        std::string FileDialog::saveFile(const wchar_t* title) {
            wchar_t buf[MAX_PATH] = {};
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = nullptr;
            ofn.lpstrFilter = L"All Files\0*.*\0";
            ofn.lpstrFile = buf;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrTitle = title;
            ofn.Flags = OFN_OVERWRITEPROMPT;

            if (::GetSaveFileNameW(&ofn)) {
                return wideToUtf8(buf);
            }
            return {};
        }

    } // namespace gui
} // namespace obf