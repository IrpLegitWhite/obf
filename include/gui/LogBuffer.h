#pragma once

#include <string>
#include <vector>
#include <mutex>

namespace obf {
    namespace core {

        class LogBuffer {
        public:
            static LogBuffer& instance();

            void push(const std::string& line);
            std::vector<std::string> snapshot() const;
            void clear();

        private:
            LogBuffer() = default;
            mutable std::mutex mutex_;
            std::vector<std::string> lines_;
        };

    } // namespace core
} // namespace obf