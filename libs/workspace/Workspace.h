#pragma once

#include "Statement.h"

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Wasp
{

struct Symbol;
using Symbol_ptr = std::shared_ptr<Symbol>;
using SymbolVector = std::vector<Symbol_ptr>;

struct Module;
using Module_ptr = std::shared_ptr<Module>;

struct ModuleType;
using ModuleType_ptr = std::shared_ptr<ModuleType>;

class Workspace;
using Workspace_ptr = std::shared_ptr<Workspace>;

// ============================================================================
// Module
// ============================================================================

struct Module
{
    const std::filesystem::path absolute_filepath;

    Block block;
    ModuleType_ptr type = nullptr;
    SymbolVector exported_symbols;

    std::string cpp_code;

    Module() = default;

    Module(std::filesystem::path file_path, Block block);

    std::string get_name() const;
    std::string get_path() const;
    std::string get_qualified_name() const;

    void save_ast(const std::string& tag);
    void save_cpp_code(const std::string& tag);
};

// ============================================================================
// Workspace
// ============================================================================

class Workspace
{
public:
    const std::filesystem::path root_path;
    const std::filesystem::path build_path;
    const std::filesystem::path libs_path;

    std::map<std::filesystem::path, Module_ptr> module_registry;
    std::map<std::filesystem::path, Symbol_ptr> module_symbols;

    explicit Workspace(std::filesystem::path root);

    Module_ptr get_module(const std::filesystem::path& path);
    Symbol_ptr get_module_symbol(const std::filesystem::path& path);

    void add_module(const std::filesystem::path&, Module_ptr);
    void add_module_symbol(const std::filesystem::path&, Symbol_ptr);
};

} // namespace Wasp
