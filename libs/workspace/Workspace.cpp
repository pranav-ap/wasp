#include "Workspace.h"
#include "Doctor.h"
#include "Statement.h"

#include <cstddef>
#include <filesystem>
#include <map>
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

    return nullptr;
}

Module_ptr Workspace::get_module(int module_index)
{
    for (const auto& [path, module] : module_registry)
    {
        if (get_module_index(path) == module_index)
        {
            return module;
        }
    }

    return nullptr;
}

const std::map<std::filesystem::path, Module_ptr>& Workspace::get_all_modules() const
{
    return module_registry;
}

void Workspace::add_module(const std::filesystem::path& path, Module_ptr module)
{
    module_registry[path] = module;
}

int Workspace::get_module_index(const std::filesystem::path& path) const
{
    int index = 0;
    for (const auto& [p, _] : module_registry)
    {
        if (p == path)
        {
            return index;
        }
        ++index;
    }
    return -1;
}

std::string Workspace::get_module_path(int module_index) const
{
    int index = 0;
    for (const auto& [path, _] : module_registry)
    {
        if (index == module_index)
        {
            return path.string();
        }
        ++index;
    }
    return "";
}

} // namespace Wasp
