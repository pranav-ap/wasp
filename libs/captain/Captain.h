#pragma once

#include "CFGraph.h"
#include "Workspace.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Wasp {

class Captain {
private:
    std::shared_ptr<Workspace> workspace;
    std::filesystem::path entry_file;

    void parse_modules();
    void parse_module(const std::filesystem::path& file_path);

    std::vector<Module_ptr> calculate_build_order();
    void hoist_symbols(const std::vector<Module_ptr>& build_order);
    void type_check_and_link(const std::vector<Module_ptr>& build_order);
    void compile(const std::vector<Module_ptr>& build_order);

    std::string read_file(const std::filesystem::path& file_path);

    void dump_build_artifacts(
        std::shared_ptr<Workspace> workspace,
        const std::filesystem::path& source_file_path,
        const CodeObject& bytecode
    );

public:
    explicit Captain(const std::filesystem::path& target_path);
    std::shared_ptr<Workspace> build();
    void execute();
};

} // namespace Wasp