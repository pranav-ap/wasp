#pragma once

#include "Workspace.h"

#include <filesystem>

namespace Wasp {

class Captain {
private:
    Workspace_ptr workspace;
    std::filesystem::path entry_file;

    void parse_modules();
    void parse_module(const std::filesystem::path& file_path);

public:
    explicit Captain(const std::filesystem::path& target_path);

    void build();
    void execute();
};

} // namespace Wasp
