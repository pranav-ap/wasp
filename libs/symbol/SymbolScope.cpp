#include "SymbolScope.h"
#include <algorithm>
#include <stdexcept>

#ifndef ASSERT
#include <cassert>
#define ASSERT(condition, message) assert((condition) && message)
#endif

namespace Wasp
{
	SymbolScope::SymbolScope(ScopeType type, SymbolScope_ptr enclosing)
		: type(type), enclosing_scope(std::move(enclosing)), depth(0)
	{
		if (enclosing_scope)
		{
			depth = enclosing_scope->depth + (type == ScopeType::FUNCTION ? 1 : 0);
		}
	}

	void SymbolScope::define(std::string_view name, Symbol_ptr symbol)
	{
		ASSERT(symbol != nullptr, "Cannot define a null symbol");
		auto [it, inserted] = symbols.emplace(std::string(name), std::move(symbol));

		ASSERT(inserted, "Variable name already exists in this scope!");
	}

	Symbol_ptr SymbolScope::lookup(std::string_view name) const
	{
		std::string search_name{name};
		const SymbolScope *current = this;

		while (current)
		{
			auto it = current->symbols.find(search_name);
			if (it != current->symbols.end())
			{
				return it->second;
			}

			current = current->enclosing_scope.get();
		}

		return nullptr;
	}

	bool SymbolScope::enclosed_in(ScopeType target_type) const
	{
		const SymbolScope *current = this;
		while (current)
		{
			if (current->type == target_type)
				return true;
			current = current->enclosing_scope.get();
		}
		return false;
	}

	bool SymbolScope::enclosed_in(const std::vector<ScopeType> &types) const
	{
		const SymbolScope *current = this;
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
}