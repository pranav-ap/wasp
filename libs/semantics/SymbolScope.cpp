#include "SymbolScope.h"
#include "AST.h"
#include "Doctor.h"
#include "Symbol.h"
#include "SymbolFactory.h"

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

    Doctor::semantics().fatal("Undefined symbol: '" + name + "'");
}

Symbol_ptr SymbolScope::lookup_required_and_resolve(
    const std::string& name
) const
{
    Symbol_ptr unresolved = this->lookup(name);
    Doctor::semantics().fatal_if_nullptr(
        unresolved,
        "Undefined symbol: '" + name + "'"
    );

    return unresolved->resolve();
}

Symbol_ptr SymbolScope::lookup_variable(const std::string& name) const
{
    Symbol_ptr symbol = this->lookup_required_and_resolve(name);

    Doctor::semantics().assert(
        symbol->is<VariableSymbol>(),
        "Expected variable symbol for '" + name + "'"
    );

    return symbol;
}

Symbol_ptr SymbolScope::lookup_functions(const std::string& name) const
{
    Symbol_ptr symbol = this->lookup_required_and_resolve(name);

    Doctor::semantics().assert(
        symbol->is<FunctionOverloadsSymbol>(),
        "Expected function overloads symbol for '" + name + "'"
    );

    return symbol;
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

int SymbolScope::get_function_closure_distance(int target_closure_depth) const
{
    return this->closure_depth - target_closure_depth;
}

// ------------------------------------------
// Define
// ------------------------------------------

void SymbolScope::define(Symbol_ptr symbol)
{
    Doctor::semantics().fatal_if_nullptr(
        symbol,
        "Cannot define a null symbol"
    );

    Doctor::semantics().assert(
        !symbol->is<FunctionSymbol>(),
        "Cannot directly define a function symbol. Use overload() instead."
    );

    Doctor::semantics().assert(
        !contains_in_current_scope(symbol->name),
        symbol->name + " is already declared in this scope"
    );

    symbols[symbol->name] = symbol;
}

void SymbolScope::define_overload(Symbol_ptr symbol)
{
    Doctor::semantics().fatal_if_nullptr(
        symbol,
        "Cannot define a null symbol"
    );

    Doctor::semantics().assert(
        symbol->is<FunctionOverloadsSymbol>(),
        "Expected a FunctionOverloadsSymbol for overload definition"
    );

    if (!symbols.contains(symbol->name))
    {
        symbols[symbol->name] = symbol;
    }
}

Symbol_ptr SymbolScope::overload(Symbol_ptr symbol)
{
    Doctor::semantics().fatal_if_nullptr(
        symbol,
        "Cannot define a null symbol"
    );

    Doctor::semantics().assert(
        symbol->is<FunctionSymbol>(),
        "Only Function Symbol can be overloaded. Use define() instead."
    );

    auto overload_symbol = this->lookup(symbol->name);

    if (!overload_symbol)
    {
        overload_symbol = SymbolFactory::create_function_overloads(
            symbol->name
        );
    }

    overload_symbol->as<FunctionOverloadsSymbol>().add_overload(symbol);

    symbols[symbol->name] = overload_symbol;

    return overload_symbol;
}

} // namespace Wasp
