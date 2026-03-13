#pragma once

#include "Objects.h"

#include <memory>
#include <string>
#include <utility>
#include <variant>

namespace Wasp {

struct VariableData {
    Object_ptr type;

    bool is_mutable;
    bool is_captured;
};

struct FunctionData {
    Object_ptr type;

    bool is_native;
};

struct ClassData {
    Object_ptr type;
};

struct EnumData {
    Object_ptr type;
};

struct ModuleData {};

using SymbolPayload = std::variant<VariableData, FunctionData, ModuleData, ClassData, EnumData>;

struct Symbol {
    std::string name;

    int id = -1;
    int closure_depth = 0;
    int lexical_depth = 0;

    SymbolPayload payload;

    Symbol(std::string name, int closure_depth, int lexical_depth, SymbolPayload payload)
        : name(std::move(name)), closure_depth(closure_depth), lexical_depth(lexical_depth),
          payload(std::move(payload)) {}

    bool is_global() const { return lexical_depth == 0; }

    Object_ptr get_type();
    void set_type(Object_ptr new_type);

    // If we are writing to a variable declared in an outer scope, mark it captured.
    bool should_be_captured(int current_closure_depth) const {
        return closure_depth > 0 && closure_depth < current_closure_depth;
    }

    void capture_if_required(int current_closure_depth) {
        if (is<VariableData>()) {
            auto& var_data = as<VariableData>();
            if (!var_data.is_captured && should_be_captured(current_closure_depth)) {
                var_data.is_captured = true;
            }
        }
    }

    template <typename T> bool is() const { return std::holds_alternative<T>(payload); }

    template <typename T> T& as() { return std::get<T>(payload); }
};

using Symbol_ptr = std::shared_ptr<Symbol>;

struct SymbolFactory {

    static Symbol_ptr create_variable(
        std::string name,
        Object_ptr type,
        bool is_mutable,
        bool is_captured = false,
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

    static Symbol_ptr create_module(std::string name);
};

} // namespace Wasp
