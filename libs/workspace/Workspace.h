#pragma once

#include "AST.h"

#include <filesystem>
#include <map>
#include <memory>
#include <string>

namespace Wasp
{

struct Module;
using Module_ptr = std::shared_ptr<Module>;

struct Workspace;
using Workspace_ptr = std::shared_ptr<Workspace>;

// ============================================================================
// Module
// ============================================================================

struct Module
{
    const std::filesystem::path absolute_filepath;

    StatementVector stmts;

    Module() = default;

    Module(std::filesystem::path file_path, StatementVector stmts);

    std::string get_name() const;
    std::string get_path() const;
};

// ============================================================================
// Workspace
// ============================================================================

class Workspace
{
private:
    std::map<std::filesystem::path, Module_ptr> module_registry;

public:
    const std::filesystem::path root_path;
    const std::filesystem::path build_path;
    const std::filesystem::path libs_path;

    explicit Workspace(std::filesystem::path root);

    Module_ptr get_module(const std::filesystem::path& path);
    Module_ptr get_module(int module_index);

    const std::map<std::filesystem::path, Module_ptr>& get_all_modules() const;

    void add_module(const std::filesystem::path& path, Module_ptr module);

    int get_module_index(const std::filesystem::path& path) const;
    std::string get_module_path(int module_index) const;
};

} // namespace Wasp
