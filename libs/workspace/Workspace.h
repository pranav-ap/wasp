#pragma once

#include "AST.h"
#include "CFGraph.h"
#include "ConstantPool.h"
#include "NativeRegistry.h"
#include "Objects.h"

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Wasp
{

// ----------------------------------------------------------------
// Forward Declarations
// ----------------------------------------------------------------

struct Symbol;
using Symbol_ptr = std::shared_ptr<Symbol>;
using SymbolVector = std::vector<Symbol_ptr>;

struct Module;
using Module_ptr = std::shared_ptr<Module>;

// --------------------------------------------------------------------
// Symbol Payloads
// --------------------------------------------------------------------

struct TypedSymbolData
{
    Object_ptr type;
};

struct VariableData : public TypedSymbolData
{
    bool is_mutable;
};

struct FunctionData : public TypedSymbolData
{
    bool is_native;

    SymbolVector sibling_overloads;
    SymbolVector parent_overloads;

    void add_sibling_overload(Symbol_ptr sibling);
    void add_parent_overload(Symbol_ptr parent);

    Object_ptr get_return_type() const;
    SymbolVector get_overloads() const;
};

struct ClassData : public TypedSymbolData
{
};

struct EnumData : public TypedSymbolData
{
};

struct ModuleData
{
    Module_ptr mod;
};

struct AliasData
{
    Symbol_ptr target;
};

using SymbolPayload = std::
    variant<VariableData, FunctionData, ModuleData, ClassData, EnumData, AliasData>;

// --------------------------------------------------------------------
// Symbol
// --------------------------------------------------------------------

struct Symbol : public std::enable_shared_from_this<Symbol>
{
    std::string name;

    int id = -1;
    int declaration_depth = 0;
    int lexical_depth = 0;

    SymbolPayload payload;

    Symbol(int id, std::string name, int closure_depth, int lexical_depth, SymbolPayload payload)
        : id(id), name(std::move(name)), declaration_depth(closure_depth),
          lexical_depth(lexical_depth), payload(std::move(payload))
    {
    }

    bool is_global() const { return lexical_depth == 0; }

    Object_ptr get_type();
    void set_type(Object_ptr new_type);

    // If it was declared in a shallower scope than we are currently in, it's an upvalue!
    // only takes care at file level not inter file level
    bool should_be_captured(int usage_depth) const { return declaration_depth < usage_depth; }

    Symbol_ptr resolve();

    template <typename T> bool payload_is() const { return std::holds_alternative<T>(payload); }
    template <typename T> T& get_payload_as() { return std::get<T>(payload); }
};

// ----------------------------------------------------------------
// Symbol Factory
// ----------------------------------------------------------------

class SymbolFactory
{
private:
    inline static int symbol_id_counter{0};

public:
    static Symbol_ptr create_variable(
        std::string name,
        Object_ptr type,
        bool is_mutable = false,
        int closure_depth = 0,
        int lexical_depth = 0);

    static Symbol_ptr create_function(
        std::string name,
        Object_ptr type,
        bool is_native = false,
        int closure_depth = 0,
        int lexical_depth = 0);

    static Symbol_ptr create_class(
        std::string name,
        Object_ptr type = nullptr,
        int closure_depth = 0,
        int lexical_depth = 0);

    static Symbol_ptr create_enum(
        std::string name,
        Object_ptr type = nullptr,
        int closure_depth = 0,
        int lexical_depth = 0);

    static Symbol_ptr create_module(std::string name, Module_ptr mod);

    static Symbol_ptr create_alias(std::string name, Symbol_ptr target);
};

// ----------------------------------------------------------------
// Module
// ----------------------------------------------------------------

struct Module
{
    const std::filesystem::path absolute_filepath;

    StatementVector stmts;
    CFGraph graph;
    StaticFunctionObject_ptr blueprint;

    std::map<std::string, SymbolVector> hoisted_symbols;
    Object_ptr type;

    Module() = default;

    Module(std::filesystem::path file_path, StatementVector stmts)
        : absolute_filepath(file_path), stmts(stmts)
    {
    }

    std::string get_name() const { return absolute_filepath.stem().string(); }

    std::map<std::string, SymbolVector> get_exports() const
    {
        // TODO: export only public symbols
        return hoisted_symbols;
    }

    SymbolVector get_flat_hoists() const
    {
        SymbolVector all_symbols;

        for (const auto& [_, symbols] : hoisted_symbols)
        {
            all_symbols.insert(all_symbols.end(), symbols.begin(), symbols.end());
        }

        return all_symbols;
    }

    SymbolVector get_flat_exports() const { return get_flat_hoists(); }
};

// ----------------------------------------------------------------
// Workspace
// ----------------------------------------------------------------

class Workspace
{
private:
    std::map<std::filesystem::path, Module_ptr> module_registry;

public:
    const std::filesystem::path root_path;
    const std::filesystem::path build_path;
    const std::filesystem::path libs_path;

    ConstantPool_ptr pool;
    NativeRegistry_ptr native_registry;

    Workspace(std::filesystem::path root)
        : root_path(std::filesystem::absolute(root)), build_path(root_path / "build"),
          libs_path(root_path / "libs"), pool(std::make_shared<ConstantPool>()),
          native_registry(std::make_shared<NativeRegistry>(pool))
    {
        std::filesystem::create_directories(build_path);
        std::filesystem::create_directories(libs_path);
    }

    Module_ptr get_module(const std::filesystem::path& path)
    {
        auto it = module_registry.find(std::filesystem::absolute(path));
        return (it != module_registry.end()) ? it->second : nullptr;
    }

    const std::map<std::filesystem::path, Module_ptr>& get_all_modules() const
    {
        return module_registry;
    }

    void add_module(const std::filesystem::path& path, Module_ptr module)
    {
        module_registry[std::filesystem::absolute(path)] = module;
    }
};

using Workspace_ptr = std::shared_ptr<Workspace>;

} // namespace Wasp
