#pragma once

#include <string>

namespace obf {
    namespace core {

        class Logger {
        public:
            static void debug(const std::string& msg);
            static void info(const std::string& msg);
            static void warn(const std::string& msg);
            static void error(const std::string& msg);
        };

    } // namespace core
} // namespace obf