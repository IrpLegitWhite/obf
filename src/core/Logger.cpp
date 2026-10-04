#include "../../include/core/Logger.h"
#include "../../include/gui/LogBuffer.h"

namespace obf {
    namespace core {

        void Logger::debug(const std::string& msg) {
            LogBuffer::instance().push("[DEBUG] " + msg);
        }

        void Logger::info(const std::string& msg) {
            LogBuffer::instance().push("[INFO] " + msg);
        }

        void Logger::warn(const std::string& msg) {
            LogBuffer::instance().push("[WARN] " + msg);
        }

        void Logger::error(const std::string& msg) {
            LogBuffer::instance().push("[ERROR] " + msg);
        }

    } // namespace core
} // namespace obf