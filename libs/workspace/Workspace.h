#pragma once

#include "AST.h"
#include "CFGraph.h"
#include "ConstantPool.h"
#include "NativeRegistry.h"
#include "Objects.h"

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Wasp
{

struct Symbol;
using Symbol_ptr = std::shared_ptr<Symbol>;
using SymbolVector = std::vector<Symbol_ptr>;
using SymbolStringMap = std::map<std::string, Symbol_ptr>;
using SymbolIntMap = std::map<int, Symbol_ptr>;
using OptionalSymbol = std::optional<Symbol_ptr>;

struct SymbolScope;
using SymbolScope_ptr = std::shared_ptr<SymbolScope>;

struct Module;
using Module_ptr = std::shared_ptr<Module>;

struct Workspace;
using Workspace_ptr = std::shared_ptr<Workspace>;

// ============================================================================
// Symbol Payloads
// ============================================================================

struct FunctionSymbol
{
    Object_ptr type;
    bool is_native;
    Statement_ptr definition = nullptr;
    std::shared_ptr<SymbolScope> declaration_scope = nullptr;
    bool required_in_class = false;

    FunctionSymbol(Object_ptr type, bool is_native);
};

struct OverloadsSymbol
{
    SymbolVector overloads;
    SymbolVector parents;

    OverloadsSymbol() = default;

    const SymbolVector& get_overloads() const;
    std::vector<std::pair<Symbol_ptr, int>> get_overloads_with_indices() const;
};

struct VariableSymbol
{
    Object_ptr type;
    bool is_mutable;

    VariableSymbol(Object_ptr type, bool is_mutable);
};

struct OopsSymbol
{
    Object_ptr type;
    Statement_ptr definition;
    std::shared_ptr<SymbolScope> declaration_scope;

    explicit OopsSymbol(Object_ptr type);
};

struct TemplateParameterSymbol
{
    Object_ptr type;
};

struct SymbolAliasSymbol
{
    Symbol_ptr target;
};

struct TypeAliasSymbol
{
    Object_ptr type;
};

struct ModuleSymbol
{
    Module_ptr mod;
};

struct EnumSymbol
{
    Object_ptr type;
};

// ============================================================================
// Symbol
// ============================================================================

struct Symbol : public std::enable_shared_from_this<Symbol>
{
    std::string name;
    std::filesystem::path module_path;
    int id = -1;
    int closure_depth = 0;
    int lexical_depth = 0;

    using UnderlyingVariant = std::variant<
        std::monostate,

        VariableSymbol,
        OverloadsSymbol,
        FunctionSymbol,
        ModuleSymbol,
        OopsSymbol,
        TemplateParameterSymbol,
        EnumSymbol,
        TypeAliasSymbol,
        SymbolAliasSymbol>;

    UnderlyingVariant payload;

    Symbol() = default;

    Symbol(
        int id,
        std::string name,
        int closure_depth,
        int lexical_depth,
        UnderlyingVariant payload
    );

    template <typename T> bool is() const
    {
        return std::holds_alternative<T>(payload);
    }

    template <typename T> const T& as() const
    {
        return std::get<T>(payload);
    }

    template <typename T> T& as()
    {
        return std::get<T>(payload);
    }

    template <typename... Ts> bool is_any_of() const
    {
        return (is<Ts>() || ...);
    }

    Object_ptr get_type() const;
    void set_type(Object_ptr new_type);

    bool is_native() const;
    void mark_as_native();
    void mark_as_required();

    void add_overload(Symbol_ptr overload);
    SymbolVector get_overloads() const;

    Symbol_ptr resolve();
    bool should_be_captured(int usage_depth) const;

    bool is_global() const;
    bool is_exportable() const;

    std::string to_string() const;
};

// ============================================================================
// SymbolFactory
// ============================================================================

class SymbolFactory
{
private:
    static int symbol_id_counter;

    static Symbol_ptr create_symbol(
        const std::string& name,
        Symbol::UnderlyingVariant&& payload,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_symbol(Symbol::UnderlyingVariant&& payload);

public:
    static void reset_counter();
    static int get_current_id();

    static Symbol_ptr create_variable(
        const std::string& name,
        Object_ptr type,
        bool is_mutable = false,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_function(
        const std::string& name,
        Object_ptr type,
        bool is_native = false,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_overloads(
        const std::string& name,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_oops(
        const std::string& name,
        Object_ptr type = nullptr,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_template_parameter(
        const std::string& name,
        Object_ptr type = nullptr,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_enum(
        const std::string& name,
        Object_ptr type = nullptr,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_type_alias(
        const std::string& name,
        Object_ptr type,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_symbol_alias(
        const std::string& name,
        Symbol_ptr target,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_module(
        const std::string& name,
        Module_ptr mod,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_dummy(
        const std::string& name,
        Object_ptr type,
        int closure_depth = 0,
        int lexical_depth = 0
    );
};

// ============================================================================
// Module
// ============================================================================

struct Module
{
    const std::filesystem::path absolute_filepath;

    StatementVector stmts;
    CFGraph graph;
    FunctionBlueprintObject_ptr blueprint;

    SymbolVector exports;
    Object_ptr type = nullptr;

    Module() = default;

    Module(std::filesystem::path file_path, StatementVector stmts);

    std::string get_name() const;
    std::string get_path() const;

    int get_member_index(const std::string& member_name) const;
    Symbol_ptr get_member(const std::string& member_name) const;
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

    ConstantPool_ptr pool;
    NativeRegistry_ptr native_registry;

    explicit Workspace(std::filesystem::path root);

    Module_ptr get_module(const std::filesystem::path& path);
    Module_ptr get_module(int module_index);

    const std::map<std::filesystem::path, Module_ptr>& get_all_modules() const;

    void add_module(const std::filesystem::path& path, Module_ptr module);

    int get_module_index(const std::filesystem::path& path) const;
    std::string get_module_path(int module_index) const;
};

} // namespace Wasp
