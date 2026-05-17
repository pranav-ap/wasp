#pragma once

#include "Workspace.h"

#include <memory>
#include <string>
#include <vector>

namespace Wasp
{
enum class ScopeType
{
    NONE,
    WORKSPACE,
    MODULE,
    EXPRESSION,
    LOOP,
    BRANCH,
    FUNCTION,
    PURE_FUNCTION,
    METHOD,
    PURE_METHOD,
    CLASS
};

class SymbolScope;
using SymbolScope_ptr = std::shared_ptr<SymbolScope>;

class SymbolScope
{
private:
    ScopeType type;
    SymbolScope_ptr enclosing_scope;

    int closure_depth;
    int lexical_depth;

    Symbol_ptr define_function(Symbol_ptr symbol);
    Symbol_ptr define_method(Symbol_ptr new_symbol);

public:
    SymbolStringMap symbols;

    SymbolScope(ScopeType type, SymbolScope_ptr enclosing_scope = nullptr);

    SymbolScope(const SymbolScope&) = delete;
    SymbolScope& operator=(const SymbolScope&) = delete;

    Symbol_ptr define(Symbol_ptr symbol);
    Symbol_ptr lookup(const std::string& name) const;
    Symbol_ptr lookup_local(const std::string& name) const;

    bool contains_in_current_scope(const std::string& name) const;
    bool contains_in_any_scope(const std::string& name) const;

    bool enclosed_in(ScopeType target_type) const;
    bool enclosed_in(const std::vector<ScopeType>& types) const;

    ScopeType get_type() const;
    SymbolScope_ptr get_enclosing() const;

    int get_closure_depth() const;
    int get_lexical_depth() const;
    int get_function_distance(int target_closure_depth) const;
};
} // namespace Wasp
