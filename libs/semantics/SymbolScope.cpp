#include "SymbolScope.h"
#include "Doctor.h"
#include "Workspace.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Wasp
{

SymbolScope::SymbolScope(ScopeType type, SymbolScope_ptr enclosing)
    : type(type), enclosing_scope(std::move(enclosing)), closure_depth(0), lexical_depth(0)
{
    if (enclosing_scope)
    {
        closure_depth = enclosing_scope->closure_depth + (type == ScopeType::FUNCTION ? 1 : 0);
        lexical_depth = enclosing_scope->lexical_depth + 1;
    }
}

Symbol_ptr SymbolScope::define(Symbol_ptr symbol)
{
    Doctor::get().fatal_if_nullptr(symbol, WaspStage::Semantics, "Cannot define a null symbol");

    if (symbol->payload_is<LocalFunctionData>())
    {
        return define_function(symbol);
    }

    Doctor::get().assert(
        !contains_in_current_scope(symbol->name),
        WaspStage::Semantics,
        "'" + symbol->name + "' already declared in this scope");

    symbols[symbol->name] = symbol;

    return symbol;
}

Symbol_ptr SymbolScope::define_function(Symbol_ptr new_symbol)
{
    Doctor::get().assert(
        new_symbol->payload_is<LocalFunctionData>(),
        WaspStage::Semantics,
        "Expected a function symbol"
    );

    // Overload Group already exists locally

    if (contains_in_current_scope(new_symbol->name))
    {
        Symbol_ptr existing_local = symbols[new_symbol->name];

        Doctor::get().assert(
            existing_local->payload_is<OverloadGroupData>(),
            WaspStage::Semantics,
            new_symbol->name + " already declared in this scope and is not an overload set");

        existing_local->get_payload_as<OverloadGroupData>().siblings.push_back(new_symbol);
        return new_symbol;
    }

    // Create a new Overload Group

    auto new_overload_set = SymbolFactory::create_overload_set(
        new_symbol->name,
        closure_depth,
        lexical_depth);

    auto& set_data = new_overload_set->get_payload_as<OverloadGroupData>();
    set_data.siblings.push_back(new_symbol);

    symbols[new_symbol->name] = new_overload_set;

    // Inherit from parent scope

    if (enclosing_scope)
    {
        Symbol_ptr existing_parent = enclosing_scope->lookup(new_symbol->name);

        if (existing_parent && existing_parent->payload_is<OverloadGroupData>())
        {
            const auto& parent_data = existing_parent->get_payload_as<OverloadGroupData>();

            // Pull in the parent's siblings and the parent's parents into our local parents vector

            set_data.parents.insert(
                set_data.parents.end(),
                parent_data.siblings.begin(),
                parent_data.siblings.end());

            set_data.parents.insert(
                set_data.parents.end(),
                parent_data.parents.begin(),
                parent_data.parents.end());
        }
    }

    return new_symbol;
}

Symbol_ptr SymbolScope::lookup(const std::string& name) const
{
    const SymbolScope* current = this;

    while (current)
    {
        if (current->symbols.contains(name))
        {
            return current->symbols.at(name);
        }

        current = current->enclosing_scope.get();
    }

    return nullptr;
}

bool SymbolScope::contains_in_current_scope(const std::string& name) const
{
    return symbols.contains(name);
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
                return true;
        }
        current = current->enclosing_scope.get();
    }
    return false;
}

ScopeType SymbolScope::get_type() const { return type; }

SymbolScope_ptr SymbolScope::get_enclosing() const { return enclosing_scope; }

int SymbolScope::get_closure_depth() const { return closure_depth; }

int SymbolScope::get_lexical_depth() const { return lexical_depth; }

int SymbolScope::get_function_distance(int target_closure_depth) const
{
    return this->closure_depth - target_closure_depth;
}

SymbolVector SymbolScope::get_all_symbols() const
{
    SymbolVector result;
    result.reserve(symbols.size());

    for (const auto& [name, symbol] : symbols)
    {
        result.push_back(symbol);
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const Symbol_ptr& a, const Symbol_ptr& b)
        {
            return a->id < b->id;
        }
    );

    return result;
}
} // namespace Wasp
