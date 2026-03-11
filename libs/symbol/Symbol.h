#pragma once

#include "Objects.h"
#include <memory>
#include <string>

namespace Wasp
{
	struct Symbol
	{
		int id;
		std::string name;

        Object_ptr type;

        bool is_public;
        bool is_mutable;
        bool is_builtin;
        bool is_captured;

        int closure_depth;
        int lexical_depth;

        Symbol(
            std::string name,
            Object_ptr type,
            bool is_public = true,
            bool is_mutable = true,
            bool is_builtin = false,
            bool is_captured = false,
            int closure_depth = 0,
            int lexical_depth = 0
        )
            : id(-1), name(name), type(type), is_public(is_public), is_mutable(is_mutable),
              is_builtin(is_builtin), is_captured(is_captured), closure_depth(closure_depth),
              lexical_depth(lexical_depth) {};

        bool is_global() const { return lexical_depth == 0; }

        // If we are writing to a variable from an outer scope, mark it captured.
        bool should_be_captured(int current_closure_depth) const {
            return closure_depth > 0 && closure_depth < current_closure_depth;
        }
    };

    using Symbol_ptr = std::shared_ptr<Symbol>;
}