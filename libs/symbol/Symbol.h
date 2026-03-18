#pragma once

#include "Objects.h"

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace Wasp {

struct Symbol;
using Symbol_ptr = std::shared_ptr<Symbol>;
using SymbolVector = std::vector<Symbol_ptr>;

struct VariableData {
    Object_ptr type;
    bool is_mutable;
};

struct FunctionData {
    Object_ptr type;
    bool is_native;

    // includes all overloads of this function, including the current one
    std::vector<Symbol_ptr> reachable_overloads;
};

struct ClassData {
    Object_ptr type;
};

struct EnumData {
    Object_ptr type;
};

struct ModuleData {
    Object_ptr type;
    std::unordered_map<std::string, Symbol_ptr> exports;
};

struct AliasData {
    Symbol_ptr target;
};

using SymbolPayload = std::
    variant<VariableData, FunctionData, ModuleData, ClassData, EnumData, AliasData>;

struct Symbol : public std::enable_shared_from_this<Symbol> {
    std::string name;

    int id = -1;
    int declaration_depth = 0;
    int lexical_depth = 0;

    SymbolPayload payload;

    Symbol(std::string name, int closure_depth, int lexical_depth, SymbolPayload payload)
        : name(std::move(name)), declaration_depth(closure_depth), lexical_depth(lexical_depth),
          payload(std::move(payload)) {}

    bool is_global() const { return lexical_depth == 0; }

    Object_ptr get_type();
    void set_type(Object_ptr new_type);

    // If it was declared in a shallower scope than we are currently in, it's an upvalue!
    // only takes care at file level not inter file level
    bool should_be_captured(int usage_depth) const { return declaration_depth < usage_depth; }

    Symbol_ptr resolve() {
        if (payload_is<AliasData>()) {
            // Recursively unwrap aliases until we hit the real symbol
            return get_payload_as<AliasData>().target->resolve();
        }

        // Safely return a shared_ptr to ourselves!
        return shared_from_this();
    }

    template <typename T> bool payload_is() const { return std::holds_alternative<T>(payload); }

    template <typename T> T& get_payload_as() { return std::get<T>(payload); }
};

struct SymbolFactory {

    static Symbol_ptr create_variable(
        std::string name,
        Object_ptr type,
        bool is_mutable = false,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_function(
        std::string name,
        Object_ptr type,
        bool is_native = false,
        int closure_depth = 0,
        int lexical_depth = 0
    );

    static Symbol_ptr create_class(
        std::string name, Object_ptr type = nullptr, int closure_depth = 0, int lexical_depth = 0
    );

    static Symbol_ptr create_enum(
        std::string name, Object_ptr type = nullptr, int closure_depth = 0, int lexical_depth = 0
    );

    static Symbol_ptr create_module(
        std::string name,
        Object_ptr type,
        std::unordered_map<std::string, Symbol_ptr> exports);

    static Symbol_ptr create_alias(std::string name, Symbol_ptr target);
};

} // namespace Wasp
