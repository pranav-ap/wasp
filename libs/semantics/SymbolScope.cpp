#include "SymbolScope.h"
#include "AST.h"
#include "Doctor.h"
#include "Symbol.h"
#include "SymbolFactory.h"
#include "Type.h"

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

    Doctor::semantics().check(
        symbol->is<VariableSymbol>(),
        "Expected variable symbol for '" + name + "'"
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

    Doctor::semantics().check(
        !contains_in_current_scope(symbol->name),
        symbol->name + " is already declared in this scope"
    );

    symbols[symbol->name] = symbol;
}

void SymbolScope::define(TemplateType_ptr template_type)
{
    if (template_type->empty())
    {
        return;
    }

    auto ordered_generics = template_type->get_ordered_generics();

    for (const auto& [name, generic_type] : ordered_generics)
    {
        auto symbol = SymbolFactory::create_type(name, generic_type);
        this->define(symbol);
    }
}

Symbol_ptr SymbolScope::overload_function(Symbol_ptr new_symbol)
{
    // Inspect new symbol

    Doctor::semantics().fatal_if_nullptr(new_symbol, "Cannot define a null symbol");

    Doctor::semantics().check(new_symbol->is<TypeSymbol>(), "Only Function can be overloaded");

    Type_ptr new_symbol_type = new_symbol->get_type();

    Doctor::semantics().check(
        new_symbol_type->is<FunctionType_ptr>(),
        "Expected FunctionType for symbol: " + new_symbol->name
    );

    FunctionType_ptr new_function_type = new_symbol_type->as<FunctionType_ptr>();

    // Inspect existing overload symbol

    Symbol_ptr overload_symbol = this->lookup(new_symbol->name);

    if (!overload_symbol)
    {
        overload_symbol = SymbolFactory::create_type_overloads(
            new_symbol->name,
            make_shared_type<FunctionOverloadType>(new_symbol->name)
        );
    }

    Doctor::semantics().check(
        overload_symbol->is<TypeOverloadsSymbol>(),
        "Expected TypeOverloadsSymbol for symbol: " + overload_symbol->name
    );

    TypeOverloadsSymbol& s = overload_symbol->as<TypeOverloadsSymbol>();
    s.overloads.push_back(new_symbol);
    Type_ptr overload_type = s.type;

    Doctor::semantics().check(
        overload_type->is<FunctionOverloadType_ptr>(),
        "Expected FunctionOverloadType for symbol: " + new_symbol->name
    );

    FunctionOverloadType_ptr overload_function_type = overload_type->as<FunctionOverloadType_ptr>();
    overload_function_type->add(new_function_type);

    symbols[new_symbol->name] = overload_symbol;

    return overload_symbol;
}

Symbol_ptr SymbolScope::overload_method(Symbol_ptr symbol)
{
    Doctor::semantics().fatal_if_nullptr(symbol, "Cannot define a null symbol");

    Doctor::semantics().check(symbol->is<TypeSymbol>(), "Only Method can be overloaded");

    Type_ptr symbol_type = symbol->get_type();

    Doctor::semantics().check(
        symbol_type->is<MethodType_ptr>(),
        "Expected MethodType for symbol: " + symbol->name
    );

    MethodType_ptr method_type = symbol_type->as<MethodType_ptr>();

    Symbol_ptr overload_symbol = this->lookup(symbol->name);

    if (!overload_symbol)
    {
        overload_symbol = SymbolFactory::create_type_overloads(
            symbol->name,
            make_shared_type<MethodOverloadType>(symbol->name)
        );
    }

    Doctor::semantics().check(
        overload_symbol->is<TypeOverloadsSymbol>(),
        "Expected TypeOverloadsSymbol for symbol: " + overload_symbol->name
    );

    TypeOverloadsSymbol& s = overload_symbol->as<TypeOverloadsSymbol>();

    s.overloads.push_back(symbol);
    Type_ptr overload_type = s.type;

    Doctor::semantics().check(
        overload_type->is<MethodOverloadType_ptr>(),
        "Expected MethodOverloadType for symbol: " + symbol->name
    );

    MethodOverloadType_ptr overload_method_type = overload_type->as<MethodOverloadType_ptr>();
    overload_method_type->add(method_type);

    symbols[symbol->name] = overload_symbol;

    return overload_symbol;
}

} // namespace Wasp
