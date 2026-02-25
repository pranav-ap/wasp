#include "SymbolScope.h"
#include <algorithm>
#include <stdexcept>

#ifndef ASSERT
#include <cassert>
#define ASSERT(condition, message) assert((condition) && message)
#endif

namespace Wasp
{

	SymbolScope::SymbolScope(ScopeType type, SymbolScope_ptr enclosing_scope)
		: type(type), enclosing_scope(std::move(enclosing_scope))
	{
	}

	void SymbolScope::define(std::string_view name, Symbol_ptr symbol)
	{
		ASSERT(symbol != nullptr, "Cannot define a null symbol");

		auto [it, inserted] = symbols.try_emplace(std::string(name), std::move(symbol));
		ASSERT(inserted, "Name already exists in scope!");
	}

	Symbol_ptr SymbolScope::lookup(std::string_view name) const
	{
		const SymbolScope *current = this;
		std::string search_name{name};

		while (current != nullptr)
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
		while (current != nullptr)
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
		while (current != nullptr)
		{
			if (std::any_of(types.begin(), types.end(),
							[current](ScopeType t)
							{ return current->type == t; }))
			{
				return true;
			}
			current = current->enclosing_scope.get();
		}
		return false;
	}

}