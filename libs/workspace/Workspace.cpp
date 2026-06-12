#include "Workspace.h"

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

// ============================================================================
// Workspace Implementation
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

const std::map<std::filesystem::path, Module_ptr>& Workspace::
    get_all_modules() const
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
