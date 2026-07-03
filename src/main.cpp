#include "CLI11.hpp"
#include "Captain.h"

#include <string>

int main(int argc, char** argv)
{
    CLI::App app{"Wasp Language Compiler"};
    app.require_subcommand(1);

    CLI::App* run_cmd = app.add_subcommand("run", "Execute a .wasp file");

    std::string wasp_file_path;
    run_cmd->add_option("path", wasp_file_path, "Path to a .wasp file")->required()->check(CLI::ExistingPath);

    CLI11_PARSE(app, argc, argv);

    if (run_cmd->parsed())
    {
        Wasp::Captain captain(wasp_file_path);
        captain.run();
    }

    return 0;
}
