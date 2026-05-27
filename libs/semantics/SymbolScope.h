#pragma once

#include "Workspace.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wasp
{

enum class ScopeType
{
    WORKSPACE,
    MODULE,
    CLASS,
    TRAIT,
    FUNCTION,
    PURE_FUNCTION,
    METHOD,
    PURE_METHOD,
    BLOCK,
    LOOP
};

struct SymbolScope;
using SymbolScope_ptr = std::shared_ptr<SymbolScope>;

struct SymbolScope : public std::enable_shared_from_this<SymbolScope>
{
    ScopeType type;
    SymbolScope_ptr enclosing_scope;
    std::unordered_map<std::string, Symbol_ptr> symbols;

    int closure_depth;
    int lexical_depth;

    explicit SymbolScope(
        ScopeType type,
        SymbolScope_ptr enclosing_scope = nullptr
    );

    // define

    Symbol_ptr define(Symbol_ptr symbol);
    Symbol_ptr define_function(Symbol_ptr symbol);
    Symbol_ptr define_overloads(Symbol_ptr symbol);

    void merge_parent_overloads(Symbol_ptr overload_group);

    // Lookup
    OptionalSymbol lookup_local(const std::string& name) const;
    OptionalSymbol lookup(const std::string& name) const;

    // Queries
    bool contains_in_current_scope(const std::string& name) const;
    bool contains_in_any_scope(const std::string& name) const;
    bool enclosed_in(ScopeType target_type) const;
    bool enclosed_in(const std::vector<ScopeType>& types) const;

    // Getters
    ScopeType get_type() const;
    SymbolScope_ptr get_enclosing_scope() const;
    int get_closure_depth() const;
    int get_lexical_depth() const;
    int get_function_closure_distance(int target_closure_depth) const;
};

} // namespace Wasp
