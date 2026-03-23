#include "SymbolScope.h"
#include "Doctor.h"
#include "Workspace.h"

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

    return symbol;
}

Symbol_ptr SymbolScope::define_function(Symbol_ptr new_symbol)
{
    Doctor::get().assert(
        new_symbol->payload_is<FunctionData>(),
        WaspStage::Semantics,
        "Expected a function symbol");

    auto& new_payload = new_symbol->get_payload_as<FunctionData>();

    if (auto siblings = get_sibling_overloads(new_symbol->name); siblings.size() > 0)
    {
        for (auto sibling : siblings)
        {
            sibling->get_payload_as<FunctionData>().add_sibling_overload(new_symbol);
            new_payload.add_sibling_overload(sibling);
        }
    }

    if (auto parent = get_parent_overload(new_symbol->name))
    {
        new_payload.add_parent_overload(parent);

        auto parents = parent->get_payload_as<FunctionData>().get_overloads();

        for (auto parent : parents)
        {
            new_symbol->get_payload_as<FunctionData>().add_parent_overload(parent);
        }
    }

    int id = static_cast<int>(symbols.size());
    new_symbol->id = id;

    symbols.push_back(new_symbol);

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
    Symbol_ptr module_symbol = lookup(module_name);

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

    // Find any function with given name exported by module

    Symbol_ptr base_symbol = nullptr;

    for (const auto& exported_sym : module_data.mod->get_exports())
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
    Symbol_ptr base_symbol = lookup(name);

    if (!base_symbol)
    {
        return {};
    }

    return assemble_overload_family(
        base_symbol,
        "Symbol '" + name + "' is not a function and cannot have overloads");
}

SymbolVector SymbolScope::get_sibling_overloads(const std::string& name) const
{
    auto it = std::find_if(
        symbols.begin(),
        symbols.end(),
        [&](const Symbol_ptr& s) { return s->name == name && s->payload_is<FunctionData>(); });

    if (it != symbols.end())
    {
        Symbol_ptr anchor = *it;
        const auto& siblings = anchor->get_payload_as<FunctionData>().sibling_overloads;

        SymbolVector full_family;
        full_family.reserve(1 + siblings.size());

        // Add the anchor symbol itself
        full_family.push_back(anchor);

        // Append all of its siblings
        full_family.insert(full_family.end(), siblings.begin(), siblings.end());

        return full_family;
    }

    return {};
}

Symbol_ptr SymbolScope::get_parent_overload(const std::string& name) const
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
