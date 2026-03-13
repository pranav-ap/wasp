#pragma once

#include "Symbol.h"
#include "Objects.h"

#include <memory>
#include <string>
#include <utility>
#include <variant>

template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp {

Object_ptr Symbol::get_type() {
    return std::visit(
        overloaded{
            [](const VariableData& d) { return d.type; },
            [](const FunctionData& d) { return d.type; },
            [](const ClassData& d) { return d.type; },
            [](const EnumData& d) { return d.type; },
            [](const ModuleData& d) -> Object_ptr { return nullptr; }
        },
        payload
    );
}

void Symbol::set_type(Object_ptr new_type) {
    std::visit(
        overloaded{
            [&](VariableData& d) { d.type = std::move(new_type); },
            [&](FunctionData& d) { d.type = std::move(new_type); },
            [&](ClassData& d) { d.type = std::move(new_type); },
            [&](EnumData& d) { d.type = std::move(new_type); },
            [](ModuleData& d) { /* Modules don't have a type */ }
        },
        payload
    );
}

Symbol_ptr SymbolFactory::create_variable(
    std::string name,
    Object_ptr type,
    bool is_mutable,
    bool is_captured,
    int closure_depth,
    int lexical_depth
) {
    return std::make_shared<Symbol>(
        std::move(name), closure_depth, lexical_depth, VariableData{type, is_mutable, is_captured}
    );
}

Symbol_ptr SymbolFactory::create_function(
    std::string name, Object_ptr type, bool is_native, int closure_depth, int lexical_depth
) {
    return std::make_shared<Symbol>(
        std::move(name), closure_depth, lexical_depth, FunctionData{type, is_native}
    );
}

Symbol_ptr SymbolFactory::create_class(
    std::string name, Object_ptr type, int closure_depth, int lexical_depth
) {
    return std::make_shared<Symbol>(std::move(name), closure_depth, lexical_depth, ClassData{type});
}

Symbol_ptr SymbolFactory::create_enum(
    std::string name, Object_ptr type, int closure_depth, int lexical_depth
) {
    return std::make_shared<Symbol>(std::move(name), closure_depth, lexical_depth, EnumData{type});
}

Symbol_ptr SymbolFactory::create_module(std::string name) {
    return std::make_shared<Symbol>(std::move(name), 0, 0, ModuleData{});
}

} // namespace Wasp
