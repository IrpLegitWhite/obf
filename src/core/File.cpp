#include "../../include/core/File.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace obf {
    namespace core {

        std::string File::read(const std::string& path) {
            std::ifstream f(path, std::ios::binary);
            if (!f) throw std::runtime_error("Cannot open file: " + path);
            std::ostringstream ss;
            ss << f.rdbuf();
            return ss.str();
        }

        void File::write(const std::string& path, const std::string& data) {
            std::ofstream f(path, std::ios::binary);
            if (!f) throw std::runtime_error("Cannot write file: " + path);
            f.write(data.data(), data.size());
        }

    } // namespace core
} // namespace obf