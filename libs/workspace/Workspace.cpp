#include "Workspace.h"
#include "ASTPrinter.h"
#include "Doctor.h"
#include "Statement.h"
#include "nlohmann/json_fwd.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

namespace Wasp
{

// ============================================================================
// Module Implementation
// ============================================================================

Module::Module(std::filesystem::path file_path, Block block)
    : absolute_filepath(std::move(file_path)), block(std::move(block))
{
}

std::string Module::get_name() const
{
    return absolute_filepath.stem().string();
}

std::string Module::get_path() const
{
    Doctor::semantics().fatal_if_empty_string(
        absolute_filepath,
        "Module file path cannot be empty"
    );

    return absolute_filepath.string();
}

std::string Module::get_qualified_name() const
{
    Doctor::semantics().fatal_if_empty_string(
        absolute_filepath,
        "Module file path cannot be empty"
    );

    // todo - remove hardcoding
    std::filesystem::path project_root = "/workspaces/wasp/code";

    std::string result;

    // Get the relative path from project root
    auto rel_path = std::filesystem::relative(absolute_filepath, project_root);
    result = rel_path.string();

    // Replace path separators with underscores

    for (char& c : result)
    {
        if (c == '/' || c == '\\')
        {
            c = '_';
        }
    }

    // Remove file extension
    size_t dot_pos = result.find_last_of('.');

    if (dot_pos != std::string::npos)
    {
        result = result.substr(0, dot_pos);
    }

    return result;
}

void Module::save_ast(const std::string& tag)
{
    std::string wasp_file = this->absolute_filepath.string();

    // TODO remove hardcoding
    std::filesystem::path code_path = "/workspaces/wasp/code";
    std::filesystem::path relative_path = std::filesystem::relative(
        wasp_file,
        code_path
    );

    // Build output path
    std::filesystem::path output_path = std::filesystem::path(
                                            "/workspaces/wasp/code/build/ast"
                                        ) /
                                        tag / relative_path;
    output_path += ".ast.json";

    // Create directories
    std::filesystem::create_directories(output_path.parent_path());

    // Generate JSON
    nlohmann::json json = ASTPrinter::get().print(this->block);

    // Write to file
    std::ofstream file(output_path.string());
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file: " + output_path.string());
    }

    file << json.dump(4);
    file.close();

    // std::cout << "AST saved to: " << output_path.string() << std::endl;
}

// ============================================================================
// Workspace
// ============================================================================

Workspace::Workspace(std::filesystem::path root)
    : root_path(std::move(root)), build_path(root_path / "build"),
      libs_path(root_path / "libs")
{
}

Module_ptr Workspace::get_module(const std::filesystem::path& path)
{
    auto it = module_registry.find(path);

    if (it != module_registry.end())
    {
        return it->second;
    }

    Doctor::captain().fatal("Module not found: " + path.string());
}

Symbol_ptr Workspace::get_module_symbol(const std::filesystem::path& path)
{
    auto it = module_symbols.find(path);

    if (it != module_symbols.end())
    {
        return it->second;
    }

    Doctor::captain().fatal("Module symbol not found: " + path.string());
}

void Workspace::add_module(const std::filesystem::path& path, Module_ptr mod)
{
    module_registry[path] = mod;
}

void Workspace::add_module_symbol(
    const std::filesystem::path& path,
    Symbol_ptr module_symbol
)
{
    module_symbols[path] = module_symbol;
}

} // namespace Wasp
