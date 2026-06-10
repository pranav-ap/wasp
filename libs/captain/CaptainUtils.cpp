#include "Captain.h"
#include "Doctor.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace Wasp {
std::string Captain::read_file(const std::filesystem::path& file_path) {
    std::ifstream file(file_path);

    Doctor::get().assert(
        file.is_open(), WaspStage::Captain, "Failed to open file: " + file_path.string()
    );

    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

} // namespace Wasp
