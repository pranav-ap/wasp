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
using SymbolStringMap = std::map<std::string, Symbol_ptr>;
using SymbolIntMap = std::map<int, Symbol_ptr>;

struct Module;
using Module_ptr = std::shared_ptr<Module>;

struct Workspace;
using Workspace_ptr = std::shared_ptr<Workspace>;

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
    Object_ptr bound_instance_type = nullptr;

    Object_ptr get_return_type() const;

    bool is_method() const
    {
        return bound_instance_type != nullptr;
    }
};

struct OverloadGroupData : public TypedSymbolData
{
    SymbolVector siblings;
    SymbolVector parents;

    SymbolVector get_all_overloads() const;
    int get_overload_index(const Symbol_ptr& target) const;

    bool is_native() const;
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

using SymbolPayload = std::variant<
    VariableData,
    FunctionData,
    ModuleData,
    ClassData,
    EnumData,
    AliasData,
    OverloadGroupData>;

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

    Symbol(int id, std::string name, int closure_depth, int lexical_depth, SymbolPayload payload);

    bool is_global() const;
    bool is_exported() const;

    Object_ptr get_type();
    void set_type(Object_ptr new_type);

    bool should_be_captured(int usage_depth) const;

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
        Object_ptr bound_instance_type = nullptr,
        int closure_depth = 0,
        int lexical_depth = 0
    );

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

    static Symbol_ptr create_overload_set(
        std::string name,
        int closure_depth = 0,
        int lexical_depth = 0);
};

// ----------------------------------------------------------------
// Module
// ----------------------------------------------------------------

struct Module
{
    const std::filesystem::path absolute_filepath;

    StatementVector stmts;
    CFGraph graph;
    FunctionBlueprintObject_ptr blueprint;

    SymbolVector exports;
    Object_ptr type;

    Module() = default;
    Module(std::filesystem::path file_path, StatementVector stmts);

    std::string get_name() const;

    int get_member_index(const std::string& member_name) const;
    Symbol_ptr get_member(const std::string& member_name) const;
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

    Workspace(std::filesystem::path root);

    Module_ptr get_module(const std::filesystem::path& path);
    Module_ptr get_module(int module_index);

    const std::map<std::filesystem::path, Module_ptr>& get_all_modules() const;

    void add_module(const std::filesystem::path& path, Module_ptr module);

    int get_module_index(const std::filesystem::path& path) const;
    std::string get_module_path(int module_index) const;
};

} // namespace Wasp
