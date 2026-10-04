#pragma once

#include <string>

namespace obf {
    namespace core {

        class File {
        public:
            static std::string read(const std::string& path);
            static void        write(const std::string& path, const std::string& data);
        };

    } // namespace core
} // namespace obf