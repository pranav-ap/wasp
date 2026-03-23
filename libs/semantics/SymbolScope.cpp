#include "SymbolScope.h"
#include "Doctor.h"
#include "Workspace.h"

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
        symbol->name + "' already declared in this scope");

    symbols[symbol->name].push_back(symbol);

    return symbol;
}

Symbol_ptr SymbolScope::define_function(Symbol_ptr new_symbol)
{
    Doctor::get().assert(
        new_symbol->payload_is<FunctionData>(),
        WaspStage::Semantics,
        "Expected a function symbol");

    auto& new_payload = new_symbol->get_payload_as<FunctionData>();

    auto& siblings = symbols[new_symbol->name];

    for (auto& sibling : siblings)
    {
        sibling->get_payload_as<FunctionData>().add_sibling_overload(new_symbol);
        new_payload.add_sibling_overload(sibling);
    }

    if (auto parent = get_parent_overload(new_symbol->name))
    {
        new_payload.add_parent_overload(parent);

        auto parents = parent->get_payload_as<FunctionData>().get_overloads();

        for (auto p : parents)
        {
            new_symbol->get_payload_as<FunctionData>().add_parent_overload(p);
        }
    }

    symbols[new_symbol->name].push_back(new_symbol);

    return new_symbol;
}

SymbolVector SymbolScope::lookup(const std::string& name) const
{
    const SymbolScope* current = this;

    while (current)
    {
        if (current->symbols.contains(name) && !current->symbols.at(name).empty())
        {
            return current->symbols.at(name);
        }

        current = current->enclosing_scope.get();
    }

    return {};
}

Symbol_ptr SymbolScope::lookup_solo(const std::string& name) const
{
    const SymbolScope* current = this;

    while (current)
    {
        if (current->symbols.contains(name) && !current->symbols.at(name).empty())
        {
            Doctor::get().assert(
                current->symbols.at(name).size() == 1,
                WaspStage::Semantics,
                "Expected only one symbol with name '" + name + "' in this scope, but found " +
                    std::to_string(current->symbols.at(name).size()));

            return current->symbols.at(name).front();
        }

        current = current->enclosing_scope.get();
    }

    return {};
}

SymbolVector SymbolScope::assemble_overload_family(
    Symbol_ptr base_symbol,
    const std::string& error_message) const
{
    base_symbol = base_symbol->resolve();

    Doctor::get().assert(
        base_symbol->payload_is<FunctionData>(),
        WaspStage::Semantics,
        error_message);

    auto& function_data = base_symbol->get_payload_as<FunctionData>();

    SymbolVector result;

    result.reserve(
        1 + function_data.parent_overloads.size() + function_data.sibling_overloads.size());

    result.push_back(base_symbol);

    result.insert(
        result.end(),
        function_data.parent_overloads.begin(),
        function_data.parent_overloads.end());

    result.insert(
        result.end(),
        function_data.sibling_overloads.begin(),
        function_data.sibling_overloads.end());

    return result;
}

SymbolVector SymbolScope::get_function_overloads_from_module(
    const std::string& module_name,
    const std::string& function_name) const
{
    Symbol_ptr module_symbol = lookup_solo(module_name);

    if (!module_symbol)
    {
        return {};
    }

    module_symbol = module_symbol->resolve();

    Doctor::get().assert(
        module_symbol->payload_is<ModuleData>(),
        WaspStage::Semantics,
        "Symbol '" + module_name + "' is not a module");

    const auto& module_data = module_symbol->get_payload_as<ModuleData>();

    Symbol_ptr base_symbol = nullptr;

    for (const auto& exported_sym : module_data.mod->get_flat_exports())
    {
        if (exported_sym->name == function_name)
        {
            base_symbol = exported_sym;
            break;
        }
    }

    if (!base_symbol)
    {
        return {};
    }

    return assemble_overload_family(
        base_symbol,
        "Symbol '" + function_name + "' in module '" + module_name +
            "' is not a function and cannot have overloads");
}

SymbolVector SymbolScope::get_function_overloads(const std::string& name) const
{
    SymbolVector matched_symbols = lookup(name);

    if (matched_symbols.empty())
    {
        return {};
    }

    return assemble_overload_family(
        matched_symbols.front(),
        "Symbol '" + name + "' is not a function and cannot have overloads");
}

Symbol_ptr SymbolScope::get_parent_overload(const std::string& name) const
{
    if (!enclosing_scope)
    {
        return nullptr;
    }

    SymbolVector parent_symbols = enclosing_scope->lookup(name);

    if (!parent_symbols.empty() && parent_symbols.front()->payload_is<FunctionData>())
    {
        return parent_symbols.front();
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
