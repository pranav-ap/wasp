#include "SymbolScope.h"
#include "Doctor.h"
#include "Token.h"
#include "Workspace.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Wasp
{

SymbolScope::SymbolScope(ScopeType type, SymbolScope_ptr enclosing)
    : type(type), enclosing_scope(std::move(enclosing)), closure_depth(0),
      lexical_depth(0)
{
    if (enclosing_scope)
    {
        closure_depth = enclosing_scope->closure_depth;
        lexical_depth = enclosing_scope->lexical_depth;

        if (type == ScopeType::FUNCTION || type == ScopeType::PURE_FUNCTION ||
            type == ScopeType::METHOD || type == ScopeType::PURE_METHOD)
        {
            closure_depth++;
        }

        if (type != ScopeType::WORKSPACE && type != ScopeType::MODULE)
        {
            lexical_depth++;
        }
    }
}

Symbol_ptr SymbolScope::lookup_local(const std::string& name) const
{
    auto it = symbols.find(name);

    if (it != symbols.end())
    {
        return it->second;
    }

    return nullptr;
}

Symbol_ptr SymbolScope::lookup(const std::string& name) const
{
    const SymbolScope* current = this;

    while (current)
    {
        auto it = current->symbols.find(name);
        if (it != current->symbols.end())
        {
            return it->second;
        }

        current = current->enclosing_scope.get();
    }

    return nullptr;
}

Symbol_ptr SymbolScope::lookup_required(const std::string& name) const
{
    const SymbolScope* current = this;

    while (current)
    {
        auto it = current->symbols.find(name);
        if (it != current->symbols.end())
        {
            return it->second;
        }

        current = current->enclosing_scope.get();
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Undefined symbol: '" + name + "'"
    );
}

Symbol_ptr SymbolScope::lookup_required_and_resolve(
    const std::string& name
) const
{
    Symbol_ptr unresolved = this->lookup(name);
    Doctor::get().fatal_if_nullptr(
        unresolved,
        WaspStage::Semantics,
        "Undefined symbol: '" + name + "'"
    );

    return unresolved->resolve();
}

bool SymbolScope::contains_in_current_scope(const std::string& name) const
{
    return symbols.find(name) != symbols.end();
}

bool SymbolScope::contains_in_any_scope(const std::string& name) const
{
    return lookup(name) != nullptr;
}

bool SymbolScope::enclosed_in(ScopeType target_type) const
{
    const SymbolScope* current = this;
    while (current)
    {
        if (current->type == target_type)
        {
            return true;
        }

        current = current->enclosing_scope.get();
    }
    return false;
}

bool SymbolScope::enclosed_in(const std::vector<ScopeType>& types) const
{
    const SymbolScope* current = this;
    while (current)
    {
        for (auto t : types)
        {
            if (current->type == t)
            {
                return true;
            }
        }
        current = current->enclosing_scope.get();
    }
    return false;
}

ScopeType SymbolScope::get_type() const
{
    return type;
}

SymbolScope_ptr SymbolScope::get_enclosing_scope() const
{
    return enclosing_scope;
}

int SymbolScope::get_closure_depth() const
{
    return closure_depth;
}

int SymbolScope::get_lexical_depth() const
{
    return lexical_depth;
}

int SymbolScope::get_function_closure_distance(int target_closure_depth) const
{
    return this->closure_depth - target_closure_depth;
}

void SymbolScope::mark_as_required()
{
    placeholder = TokenType::REQUIRED;
}

bool SymbolScope::is_required() const
{
    return placeholder.has_value() &&
           placeholder.value() == TokenType::REQUIRED;
}

// ------------------------------------------
// Define
// ------------------------------------------

Symbol_ptr SymbolScope::define(Symbol_ptr symbol)
{
    Doctor::get().fatal_if_nullptr(
        symbol,
        WaspStage::Semantics,
        "Cannot define a null symbol"
    );

    Doctor::get().assert(
        !symbol->is<FunctionSymbol>(),
        WaspStage::Semantics,
        "Cannot directly define a function symbol"
    );

    Doctor::get().assert(
        !contains_in_current_scope(symbol->name),
        WaspStage::Semantics,
        symbol->name + " is already declared in this scope"
    );

    symbols[symbol->name] = symbol;
    return symbol;
}

} // namespace Wasp
