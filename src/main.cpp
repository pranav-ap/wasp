#include "CLI11.hpp"
#include "Captain.h"

#include <exception>
#include <iostream>
#include <string>

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
            Wasp::Captain captain(target_path);
            captain.build();
            captain.execute();
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}