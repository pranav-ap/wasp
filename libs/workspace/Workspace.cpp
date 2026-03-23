#include "Workspace.h"
#include "Doctor.h"
#include "Objects.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

Object_ptr Symbol::get_type()
{
    return std::visit(
        overloaded{
            [](const VariableData& d) { return d.type; },
            [](const FunctionData& d) { return d.type; },
            [](const ClassData& d) { return d.type; },
            [](const EnumData& d) { return d.type; },
            [](const ModuleData& d) -> Object_ptr { return d.mod->type; },
            [](const AliasData& d) { return d.target->get_type(); }},
        payload);
}

void Symbol::set_type(Object_ptr new_type)
{
    std::visit(
        overloaded{
            [&](VariableData& d) { d.type = std::move(new_type); },
            [&](FunctionData& d) { d.type = std::move(new_type); },
            [&](ClassData& d) { d.type = std::move(new_type); },
            [&](EnumData& d) { d.type = std::move(new_type); },
            [&](AliasData& d) { d.target->set_type(std::move(new_type)); },
            [](auto) -> void
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "You are not allowed to set type for this object");
            }},
        payload);
}

Symbol_ptr Symbol::resolve()
{
    if (payload_is<AliasData>())
    {
        // Recursively unwrap aliases until we hit the real symbol
        return get_payload_as<AliasData>().target->resolve();
    }

    // Safely return a shared_ptr to ourselves!
    return shared_from_this();
}

Object_ptr FunctionData::get_return_type() const
{
    Doctor::get().assert(
        type->is<FunctionType>(),
        WaspStage::Semantics,
        "FunctionData's type payload is not a FunctionType");

    const auto& signature = type->as<FunctionType>();

    if (signature.return_type.has_value())
    {
        return signature.return_type.value();
    }

    return make_object(NoneType());
}

SymbolVector FunctionData::get_overloads() const
{
    SymbolVector all_overloads;
    all_overloads.reserve(sibling_overloads.size() + parent_overloads.size() + 1);

    all_overloads.insert(all_overloads.end(), sibling_overloads.begin(), sibling_overloads.end());

    all_overloads.insert(all_overloads.end(), parent_overloads.begin(), parent_overloads.end());

    return all_overloads;
}

void FunctionData::add_sibling_overload(Symbol_ptr sibling)
{
    auto it = std::find_if(
        sibling_overloads.begin(),
        sibling_overloads.end(),
        [&](const Symbol_ptr& s) { return s->id == sibling->id; });

    if (it == sibling_overloads.end())
    {
        sibling_overloads.push_back(sibling);
    }
}

void FunctionData::add_parent_overload(Symbol_ptr parent)
{
    auto it = std::find_if(
        parent_overloads.begin(),
        parent_overloads.end(),
        [&](const Symbol_ptr& p) { return p->id == parent->id; });

    if (it == parent_overloads.end())
    {
        parent_overloads.push_back(parent);
    }
}

Symbol_ptr SymbolFactory::create_variable(
    std::string name,
    Object_ptr type,
    bool is_mutable,
    int closure_depth,
    int lexical_depth)
{
    return std::make_shared<Symbol>(
        symbol_id_counter++,
        std::move(name),
        closure_depth,
        lexical_depth,
        VariableData{std::move(type), is_mutable});
}

Symbol_ptr SymbolFactory::create_function(
    std::string name,
    Object_ptr type,
    bool is_native,
    int closure_depth,
    int lexical_depth)
{
    auto symbol = std::make_shared<Symbol>(
        symbol_id_counter++,
        std::move(name),
        closure_depth,
        lexical_depth,
        FunctionData{std::move(type), is_native, {}, {}});

    return symbol;
}

Symbol_ptr SymbolFactory::create_class(
    std::string name,
    Object_ptr type,
    int closure_depth,
    int lexical_depth)
{
    return std::make_shared<Symbol>(
        symbol_id_counter++,
        std::move(name),
        closure_depth,
        lexical_depth,
        ClassData{std::move(type)});
}

Symbol_ptr SymbolFactory::create_enum(
    std::string name,
    Object_ptr type,
    int closure_depth,
    int lexical_depth)
{
    return std::make_shared<Symbol>(
        symbol_id_counter++,
        std::move(name),
        closure_depth,
        lexical_depth,
        EnumData{std::move(type)});
}

Symbol_ptr SymbolFactory::create_module(std::string name, Module_ptr mod)
{
    return std::make_shared<Symbol>(
        symbol_id_counter++,
        std::move(name),
        0,
        0,
        ModuleData{std::move(mod)});
}

Symbol_ptr SymbolFactory::create_alias(std::string name, Symbol_ptr target)
{
    return std::make_shared<Symbol>(
        symbol_id_counter++,
        std::move(name),
        0,
        0,
        AliasData{std::move(target)});
}
} // namespace Wasp
