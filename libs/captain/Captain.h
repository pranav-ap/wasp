#pragma once

#include "Objects.h"
#include "Workspace.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Wasp {

class Captain {
private:
    Workspace_ptr workspace;
    std::filesystem::path entry_file;

    void parse_modules();
    void parse_module(const std::filesystem::path& file_path);

    std::vector<Module_ptr> calculate_build_order();
    std::string read_file(const std::filesystem::path& file_path);

    void dump_build_artifacts(
        Workspace_ptr workspace,
        const std::filesystem::path& source_file_path,
        const FunctionBlueprintObject_ptr function_object
    );

public:
    explicit Captain(const std::filesystem::path& target_path);
    Workspace_ptr build();
    void execute();
};

} // namespace Wasp
