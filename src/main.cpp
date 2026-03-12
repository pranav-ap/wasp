#include "Captain.h"
#include "Doctor.h"
#include "Objects.h"
#include "VM.h"

#include "CLI11.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace Wasp {

void run(const std::filesystem::path& target_path) {
    std::filesystem::path workspace_root =
        std::filesystem::is_directory(target_path) ? target_path : target_path.parent_path();

    if (workspace_root.empty()) {
        workspace_root = std::filesystem::current_path();
    }

    Captain captain(workspace_root);
    auto workspace = captain.build();

    std::filesystem::path entry_file =
        std::filesystem::is_regular_file(target_path)
            ? std::filesystem::absolute(target_path)
            : std::filesystem::absolute(workspace_root / "main.wasp");

    auto main_module = workspace->get_module(entry_file);
    Doctor::get().fatal_if_nullptr(main_module, WaspStage::Captain);

    auto main_function = std::make_shared<FunctionObject>(main_module->code);
    VM vm(workspace->pool, workspace->native_registry);
    vm.run(main_function);
}

} // namespace Wasp

int main(int argc, char** argv) {
    CLI::App app{"Wasp Lang"};

    std::string target_path;

    CLI::App* run_cmd = app.add_subcommand("run", "Execute a .wasp file or workspace");

    run_cmd->add_option("path", target_path, "The .wasp file or workspace directory to execute")
        ->required()
        ->check(CLI::ExistingPath);

    app.require_subcommand(1);

    CLI11_PARSE(app, argc, argv);

    try {
        if (run_cmd->parsed()) {
            Wasp::run(target_path);
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}