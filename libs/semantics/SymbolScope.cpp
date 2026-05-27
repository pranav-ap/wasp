#include "SymbolScope.h"
#include "Doctor.h"
#include "Workspace.h"

#include <memory>
#include <optional>
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

Symbol_ptr SymbolScope::define(Symbol_ptr symbol)
{
    Doctor::get().fatal_if_nullptr(
        symbol,
        WaspStage::Semantics,
        "Cannot define a null symbol"
    );

    if (symbol->is<FunctionSymbol>())
    {
        return define_function(symbol);
    }

    if (symbol->is<OverloadsSymbol>())
    {
        return define_overloads(symbol);
    }

    Doctor::get().assert(
        !contains_in_current_scope(symbol->name),
        WaspStage::Semantics,
        symbol->name + " is already declared in this scope"
    );

    symbols[symbol->name] = symbol;
    return symbol;
}

Symbol_ptr SymbolScope::define_function(Symbol_ptr new_symbol)
{
    Doctor::get().assert(
        new_symbol->is<FunctionSymbol>(),
        WaspStage::Semantics,
        "Expected a function symbol"
    );

    if (contains_in_current_scope(new_symbol->name))
    {
        auto overload_group = symbols[new_symbol->name];
        Doctor::get().assert(
            overload_group->is<OverloadsSymbol>(),
            WaspStage::Semantics,
            new_symbol->name + " already declared and is not an overload set"
        );

        overload_group->as<OverloadsSymbol>().overloads.push_back(new_symbol);
        return overload_group;
    }

    auto overload_group = SymbolFactory::create_overloads(
        new_symbol->name,
        new_symbol->closure_depth,
        new_symbol->lexical_depth
    );

    overload_group->as<OverloadsSymbol>().overloads.push_back(new_symbol);
    symbols[new_symbol->name] = overload_group;

    merge_parent_overloads(overload_group);

    return overload_group;
}

Symbol_ptr SymbolScope::define_overloads(Symbol_ptr new_symbol)
{
    Doctor::get().assert(
        new_symbol->is<OverloadsSymbol>(),
        WaspStage::Semantics,
        "Expected an overloads symbol"
    );

    if (contains_in_current_scope(new_symbol->name))
    {
        auto existing = symbols[new_symbol->name];
        Doctor::get().assert(
            existing->is<OverloadsSymbol>(),
            WaspStage::Semantics,
            new_symbol->name + " is already declared and is not an overload set"
        );

        auto& existing_data = existing->as<OverloadsSymbol>();
        const auto& incoming = new_symbol->as<OverloadsSymbol>();

        existing_data.overloads.insert(
            existing_data.overloads.end(),
            incoming.overloads.begin(),
            incoming.overloads.end()
        );

        existing_data.parents.insert(
            existing_data.parents.end(),
            incoming.parents.begin(),
            incoming.parents.end()
        );

        return existing;
    }

    symbols[new_symbol->name] = new_symbol;
    merge_parent_overloads(new_symbol);

    return new_symbol;
}

void SymbolScope::merge_parent_overloads(Symbol_ptr new_overloads_symbol)
{
    if (!enclosing_scope)
    {
        return;
    }

    auto parent_symbol = enclosing_scope->lookup(new_overloads_symbol->name);
    if (!parent_symbol.has_value() ||
        !parent_symbol.value()->is<OverloadsSymbol>())
    {
        return;
    }

    const auto& parent_data = parent_symbol.value()->as<OverloadsSymbol>();
    auto& group_data = new_overloads_symbol->as<OverloadsSymbol>();

    group_data.parents.insert(
        group_data.parents.end(),
        parent_data.overloads.begin(),
        parent_data.overloads.end()
    );

    group_data.parents.insert(
        group_data.parents.end(),
        parent_data.parents.begin(),
        parent_data.parents.end()
    );
}

OptionalSymbol SymbolScope::lookup_local(const std::string& name) const
{
    auto it = symbols.find(name);

    if (it != symbols.end())
    {
        return it->second;
    }

    return std::nullopt;
}

OptionalSymbol SymbolScope::lookup(const std::string& name) const
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

    return std::nullopt;
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

} // namespace Wasp
