#include "SymbolScope.h"
#include "Symbol.h"
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifndef ASSERT
#include <cassert>
#define ASSERT(condition, message) assert((condition) && message)
#endif

namespace Wasp
{
	SymbolScope::SymbolScope(ScopeType type, SymbolScope_ptr enclosing)
		: type(type), enclosing_scope(std::move(enclosing)), closure_depth(0), lexical_depth(0)
	{
		if (enclosing_scope)
		{
			closure_depth = enclosing_scope->closure_depth + (type == ScopeType::FUNCTION ? 1 : 0);
			lexical_depth = enclosing_scope->lexical_depth + 1;
		}
	}

	Symbol_ptr SymbolScope::define(Symbol_ptr symbol)
	{
		ASSERT(symbol != nullptr, "Cannot define a null symbol");

		if (symbols.find(std::string(symbol->name)) != symbols.end())
		{
			throw std::runtime_error("Variable '" + std::string(symbol->name) + "' already declared in this scope.");
		}

		int id = static_cast<int>(symbols.size());
		symbol->id = id;

		symbols.emplace(std::string(symbol->name), symbol);
		return symbol;
	}

	Symbol_ptr SymbolScope::lookup(std::string name) const
	{
		const SymbolScope *current = this;

		while (current)
		{
			auto it = current->symbols.find(name);
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