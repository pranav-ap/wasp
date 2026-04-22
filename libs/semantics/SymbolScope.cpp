#include "SymbolScope.h"
#include "Doctor.h"
#include "Workspace.h"

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
        closure_depth = enclosing_scope->closure_depth;
        lexical_depth = enclosing_scope->lexical_depth;

        if (type == ScopeType::FUNCTION)
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
    Doctor::get().fatal_if_nullptr(symbol, WaspStage::Semantics, "Cannot define a null symbol");

    if (symbol->payload_is<FunctionData>())
    {
        return define_function(symbol);
    }
    else if (symbol->payload_is<MethodData>())
    {
        return define_method(symbol);
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
        new_symbol->payload_is<FunctionData>(),
        WaspStage::Semantics,
        "Expected a function symbol"
    );

    if (contains_in_current_scope(new_symbol->name))
    {
        Symbol_ptr overload_group = symbols[new_symbol->name];

        Doctor::get().assert(
            overload_group->payload_is<FunctionOverloadsData>(),
            WaspStage::Semantics,
            new_symbol->name + " already declared in this scope and is not an overload set"
        );

        overload_group->get_payload_as<FunctionOverloadsData>().siblings.push_back(new_symbol);
        return overload_group;
    }

    auto overload_group = SymbolFactory::create_function_overloads(
        new_symbol->name,
        new_symbol->closure_depth,
        new_symbol->lexical_depth
    );

    auto& overload_group_data = overload_group->get_payload_as<FunctionOverloadsData>();
    overload_group_data.siblings.push_back(new_symbol);

    symbols[new_symbol->name] = overload_group;

    if (enclosing_scope)
    {
        Symbol_ptr existing_parent = enclosing_scope->lookup(new_symbol->name);

        if (existing_parent && existing_parent->payload_is<FunctionOverloadsData>())
        {
            const auto& parent_data = existing_parent->get_payload_as<FunctionOverloadsData>();

            overload_group_data.parents.insert(
                overload_group_data.parents.end(),
                parent_data.siblings.begin(),
                parent_data.siblings.end()
            );

            overload_group_data.parents.insert(
                overload_group_data.parents.end(),
                parent_data.parents.begin(),
                parent_data.parents.end()
            );
        }
    }

    return overload_group;
}

Symbol_ptr SymbolScope::define_method(Symbol_ptr new_symbol)
{
    Doctor::get().assert(
        new_symbol->payload_is<MethodData>(),
        WaspStage::Semantics,
        "Expected a method symbol"
    );

    if (contains_in_current_scope(new_symbol->name))
    {
        Symbol_ptr existing_local = symbols[new_symbol->name];

        Doctor::get().assert(
            existing_local->payload_is<MethodOverloadsData>(),
            WaspStage::Semantics,
            new_symbol->name + " already declared in this scope and is not an overload set"
        );

        existing_local->get_payload_as<MethodOverloadsData>().overloads.push_back(new_symbol);
        return new_symbol;
    }

    auto overload_group = SymbolFactory::create_method_overloads(
        new_symbol->name,
        new_symbol->closure_depth,
        new_symbol->lexical_depth
    );

    auto& set_data = overload_group->get_payload_as<MethodOverloadsData>();
    set_data.overloads.push_back(new_symbol);

    symbols[new_symbol->name] = overload_group;

    return new_symbol;
}

Symbol_ptr SymbolScope::lookup(const std::string& name) const
{
    const SymbolScope* current = this;

    while (current)
    {
        if (current->symbols.contains(name))
        {
            return current->symbols.at(name)->resolve();
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

} // namespace Wasp
