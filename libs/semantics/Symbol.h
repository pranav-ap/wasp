#pragma once

#include "AST.h"
#include "Type.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
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

struct Module;
using Module_ptr = std::shared_ptr<Module>;

// ============================================================================
// Symbol Payloads
// ============================================================================

struct VariableSymbol
{
    Type_ptr type;
    bool is_mutable;
};

struct TypeSymbol
{
    Type_ptr type;
};

struct TypeAliasSymbol
{
    Type_ptr type;
};

struct SymbolAliasSymbol
{
    Symbol_ptr target;
};

struct ModuleSymbol
{
    Type_ptr type;
};

struct OverloadSymbol
{
    // is local to a scope
    // contains all overloads of a function name in local scope
    SymbolVector overloads;
    Type_ptr type;
};

// ============================================================================
// Symbol
// ============================================================================

using SymbolVariant = std::variant<
    std::monostate,

    VariableSymbol,
    TypeSymbol,
    TypeAliasSymbol,
    SymbolAliasSymbol,
    ModuleSymbol,
    OverloadSymbol>;

struct Symbol : public std::enable_shared_from_this<Symbol>
{
    std::string name;
    std::string module_path = "";

    int id = -1;
    int closure_depth = 0;
    int lexical_depth = 0;

    SymbolVariant payload;

    Symbol() = default;

    Symbol(
        int id,
        std::string name,
        int closure_depth,
        int lexical_depth,
        SymbolVariant payload
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

    Type_ptr get_type() const;
    void set_type(Type_ptr new_type);

    Symbol_ptr resolve();
    bool should_be_captured(int usage_depth) const;

    bool is_global() const;
    bool is_exportable() const;

    std::string to_string() const;
};

} // namespace Wasp
