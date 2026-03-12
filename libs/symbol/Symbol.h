#pragma once

#include "Objects.h"

#include <memory>
#include <string>
#include <utility>

namespace Wasp {

struct Symbol {
    std::string name;
    Object_ptr type;

    int id = -1;
    int closure_depth = 0;
    int lexical_depth = 0;

    bool is_mutable = true;
    bool is_native = false;
    bool is_captured = false;

    Symbol(
        std::string name,
        Object_ptr type,
        bool is_mutable,
        bool is_builtin,
        bool is_captured,
        int closure_depth,
        int lexical_depth
    )
        : name(std::move(name)), type(std::move(type)), closure_depth(closure_depth),
          lexical_depth(lexical_depth), is_mutable(is_mutable), is_native(is_builtin),
          is_captured(is_captured) {}

    bool is_global() const { return lexical_depth == 0; }

    // If we are writing to a variable from an outer scope, mark it captured.
    bool should_be_captured(int current_closure_depth) const {
        return closure_depth > 0 && closure_depth < current_closure_depth;
    }
};

using Symbol_ptr = std::shared_ptr<Symbol>;

} // namespace Wasp