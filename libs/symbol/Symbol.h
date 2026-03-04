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

		bool is_public;
		bool is_mutable;
		bool is_captured;

		int closure_depth;
		int lexical_depth;

		Object_ptr type;

		Symbol(std::string name, Object_ptr type, bool is_public, bool is_mutable, int closure_depth, int lexical_depth)
			: id(0), name(name), type(type), is_public(is_public), is_mutable(is_mutable), closure_depth(closure_depth), lexical_depth(lexical_depth), is_captured(false) {};

		bool is_global() const { return lexical_depth == 0; }

		// If we are writing to a variable from an outer scope, mark it captured.
		bool should_be_captured(int current_closure_depth) const
		{
			return closure_depth > 0 && closure_depth < current_closure_depth;
		}
	};

	using Symbol_ptr = std::shared_ptr<Symbol>;
}