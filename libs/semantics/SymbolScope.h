#pragma once

#include "Workspace.h"

#include <map>
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
    FUNCTION
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

    std::map<std::string, SymbolVector> symbols;

    Symbol_ptr define_function(Symbol_ptr symbol);

public:
    SymbolScope(ScopeType type, SymbolScope_ptr enclosing_scope = nullptr);

    SymbolScope(const SymbolScope&) = delete;
    SymbolScope& operator=(const SymbolScope&) = delete;

    Symbol_ptr define(Symbol_ptr symbol);

    Symbol_ptr lookup_solo(const std::string& name) const;
    SymbolVector lookup(const std::string& name) const;

    SymbolVector get_function_overloads(const std::string& name) const;

    SymbolVector get_function_overloads_from_module(
        const std::string& module_name,
        const std::string& function_name) const;

    SymbolVector assemble_overload_family(Symbol_ptr base_symbol, const std::string& error_message)
        const;

    Symbol_ptr get_parent_overload(const std::string& name) const;

    bool contains_in_current_scope(const std::string& name) const { return symbols.contains(name); }

    bool contains_in_any_scope(std::string name) const { return lookup(name).size() > 0; }

    bool enclosed_in(ScopeType target_type) const;
    bool enclosed_in(const std::vector<ScopeType>& types) const;

    ScopeType get_type() const { return type; }
    SymbolScope_ptr get_enclosing() const { return enclosing_scope; }

    int get_closure_depth() const { return closure_depth; }
    int get_lexical_depth() const { return lexical_depth; }

    int get_function_distance(int target_closure_depth) const
    {
        // 0 = local to the current function
        // 1 = in the immediate parent function
        return this->closure_depth - target_closure_depth;
    }

    std::map<std::string, SymbolVector> get_all_symbols() const { return symbols; }
};
} // namespace Wasp
