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

		int depth;

		Object_ptr type;

		Symbol(int id, std::string name, Object_ptr type, bool is_public, bool is_mutable, int depth)
			: id(id), name(name), type(type), is_public(is_public), is_mutable(is_mutable), depth(depth), is_captured(false) {};
	};

	using Symbol_ptr = std::shared_ptr<Symbol>;
}