#pragma once

#include "Symbol.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wasp {
enum class ScopeType { NONE, WORKSPACE, MODULE, EXPRESSION, LOOP, BRANCH, FUNCTION };

class SymbolScope;
using SymbolScope_ptr = std::shared_ptr<SymbolScope>;

class SymbolScope {
private:
    ScopeType type;
    SymbolScope_ptr enclosing_scope;

    int closure_depth;
    int lexical_depth;

    std::unordered_map<std::string, Symbol_ptr> symbols;
    std::unordered_map<std::string, std::vector<Symbol_ptr>> function_symbols;

public:
    SymbolScope(ScopeType type, SymbolScope_ptr enclosing_scope = nullptr);

    SymbolScope(const SymbolScope&) = delete;
    SymbolScope& operator=(const SymbolScope&) = delete;

    Symbol_ptr define(Symbol_ptr symbol);
    Symbol_ptr lookup(std::string name) const;
    std::vector<Symbol_ptr> collect_function_symbols(const std::string& name) const;

    int get_symbols_count() const;

    bool contains_in_current_scope(std::string name) const {
        return symbols.find(name) != symbols.end();
    }

    bool contains_in_any_scope(std::string name) const { return lookup(name) != nullptr; }

    bool enclosed_in(ScopeType target_type) const;
    bool enclosed_in(const std::vector<ScopeType>& types) const;

    ScopeType get_type() const { return type; }
    SymbolScope_ptr get_enclosing() const { return enclosing_scope; }

    int get_closure_depth() const { return closure_depth; }
    int get_lexical_depth() const { return lexical_depth; }

    int get_function_distance(int target_closure_depth) const {
        // 0 = local to the current function
        // 1 = in the immediate parent function
        return this->closure_depth - target_closure_depth;
    }
};
} // namespace Wasp
