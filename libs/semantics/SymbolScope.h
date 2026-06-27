#pragma once

#include "Symbol.h"
#include "Type.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wasp
{

class SymbolScope;
using SymbolScope_ptr = std::shared_ptr<SymbolScope>;

enum class ScopeType
{
    WORKSPACE,
    MODULE,

    CLASS,
    TRAIT,
    PRIMITIVE,

    ENUM,
    TYPE_ALIAS,

    FUNCTION,
    PURE_FUNCTION,
    METHOD,
    PURE_METHOD,

    LOOP,
    BRANCH
};

class SymbolScope : public std::enable_shared_from_this<SymbolScope>
{
public:
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

    void define(Symbol_ptr);
    void define(TemplateType_ptr);

    // Lookup
    Symbol_ptr lookup_local(const std::string& name) const;
    Symbol_ptr lookup(const std::string& name) const;
    Symbol_ptr lookup_required(const std::string& name) const;
    Symbol_ptr lookup_required_and_resolve(const std::string& name) const;

    Symbol_ptr lookup_variable(const std::string& name) const;
    Symbol_ptr lookup_overload(const std::string& name) const;
    Symbol_ptr lookup_overload_maybe(const std::string& name) const;
    Symbol_ptr lookup_parent_overload(const std::string& name) const;

    // Queries
    bool contains_in_current_scope(const std::string& name) const;
    bool contains_in_any_scope(const std::string& name) const;
    bool enclosed_in(ScopeType target_type) const;
    bool enclosed_in(const std::vector<ScopeType>& types) const;

    // Getters
    int get_function_closure_distance(int target_closure_depth) const;

private:
    // overload

    void overload_function(Symbol_ptr);
    void overload_method(Symbol_ptr);
    void overload_class(Symbol_ptr);
    void overload_trait(Symbol_ptr);
};

} // namespace Wasp
