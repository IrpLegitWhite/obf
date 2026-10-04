#include "../../include/gui/LogBuffer.h"

namespace obf {
    namespace core {

        LogBuffer& LogBuffer::instance() {
            static LogBuffer inst;
            return inst;
        }

        void LogBuffer::push(const std::string& line) {
            std::lock_guard<std::mutex> lock(mutex_);
            lines_.push_back(line);
            if (lines_.size() > 5000)
                lines_.erase(lines_.begin(), lines_.begin() + 1000);
        }

        std::vector<std::string> LogBuffer::snapshot() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return lines_;
        }

        void LogBuffer::clear() {
            std::lock_guard<std::mutex> lock(mutex_);
            lines_.clear();
        }

    } // namespace core
} // namespace obf