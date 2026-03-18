#include "SymbolScope.h"
#include "Doctor.h"
#include "Symbol.h"

#include <algorithm>
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

    if (symbol->payload_is<FunctionData>())
    {
        return define_function(symbol);
    }

    Doctor::get().assert(
        !contains_in_current_scope(symbol->name),
        WaspStage::Semantics,
        std::string(symbol->name) + "' already declared in this scope");

    int id = static_cast<int>(symbols.size());
    symbol->id = id;

    symbols.push_back(symbol);
    id_to_name_map[id] = symbol->name;

    return symbol;
}

Symbol_ptr SymbolScope::define_function(Symbol_ptr new_symbol)
{
    Doctor::get().assert(
        new_symbol->payload_is<FunctionData>(),
        WaspStage::Semantics,
        "Expected a function symbol");

    auto& new_payload = new_symbol->get_payload_as<FunctionData>();

    if (auto old_anchor = find_any_function_overload_in_current_scope(new_symbol->name))
    {
        auto& anchor_payload = old_anchor->get_payload_as<FunctionData>();

        // exchange overload lists

        for (auto& sibling : anchor_payload.reachable_overloads)
        {
            sibling->get_payload_as<FunctionData>().reachable_overloads.push_back(new_symbol);
        }

        new_payload.reachable_overloads = anchor_payload.reachable_overloads;
    }
    else if (
        auto existing_parent_anchor = find_any_function_overload_in_any_parent_scope(
            new_symbol->name))
    {
        auto& existing_parent_payload = existing_parent_anchor->get_payload_as<FunctionData>();
        new_payload.reachable_overloads = existing_parent_payload.reachable_overloads;
    }

    int id = static_cast<int>(symbols.size());
    new_symbol->id = id;

    symbols.push_back(new_symbol);
    id_to_name_map[id] = new_symbol->name;

    return new_symbol;
}

Symbol_ptr SymbolScope::lookup(const std::string& name) const
{
    const SymbolScope* current = this;

    while (current)
    {
        auto it = std::find_if(
            current->symbols.begin(),
            current->symbols.end(),
            [&](const Symbol_ptr& s) { return s->name == name; });

        if (it != current->symbols.end())
        {
            return *it;
        }

        current = current->enclosing_scope.get();
    }

    return nullptr;
}

SymbolVector SymbolScope::get_function_overloads(const std::string& name) const
{
    Symbol_ptr base_symbol = lookup(name);

    if (base_symbol && base_symbol->payload_is<FunctionData>())
    {
        return base_symbol->get_payload_as<FunctionData>().reachable_overloads;
    }

    return {};
}

Symbol_ptr SymbolScope::find_any_function_overload_in_current_scope(const std::string& name) const
{
    auto it = std::find_if(
        symbols.begin(),
        symbols.end(),
        [&](const Symbol_ptr& s) { return s->name == name && s->payload_is<FunctionData>(); });

    if (it != symbols.end())
    {
        return *it;
    }

    return nullptr;
}

Symbol_ptr SymbolScope::find_any_function_overload_in_any_parent_scope(
    const std::string& name) const
{
    if (!enclosing_scope)
    {
        return nullptr;
    }

    Symbol_ptr sym = enclosing_scope->lookup(name);

    if (sym && sym->payload_is<FunctionData>())
    {
        return sym;
    }

    return nullptr;
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
} // namespace Wasp
