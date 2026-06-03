#pragma once

#include "Token.h"
#include "Workspace.h"

#include <memory>
#include <optional>
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
    LOOP,
    BRANCH
};

struct SymbolScope : public std::enable_shared_from_this<SymbolScope>
{
    ScopeType type;
    SymbolScope_ptr enclosing_scope;
    std::unordered_map<std::string, Symbol_ptr> symbols;
    int closure_depth;
    int lexical_depth;

    std::optional<TokenType> placeholder = std::nullopt;

    explicit SymbolScope(
        ScopeType type,
        SymbolScope_ptr enclosing_scope = nullptr
    );

    // define

    Symbol_ptr define(Symbol_ptr symbol);

    // Lookup
    Symbol_ptr lookup_local(const std::string& name) const;
    Symbol_ptr lookup(const std::string& name) const;
    Symbol_ptr lookup_required(const std::string& name) const;
    Symbol_ptr lookup_required_and_resolve(const std::string& name) const;

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

    void mark_as_required();
    bool is_required() const;
};

} // namespace Wasp
