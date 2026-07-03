#pragma once

#include "Workspace.h"

#include <filesystem>

namespace Wasp
{

class Captain
{
private:
    Workspace_ptr workspace;
    std::filesystem::path entry_wasp_file_path;

    void parse_modules();
    void parse_module(const std::filesystem::path&);

public:
    explicit Captain(const std::filesystem::path&);

    void run();
};

} // namespace Wasp
