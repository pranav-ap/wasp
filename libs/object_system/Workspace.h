#pragma once

#include <filesystem>
#include <map>

namespace Wasp {

struct Module {
    std::filesystem::path file_path;
};

struct Workspace {
    std::map<std::filesystem::path, Module> modules;
};

} // namespace Wasp