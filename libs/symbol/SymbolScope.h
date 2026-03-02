#pragma once

#include "Symbol.h"
#include <unordered_map>
#include <string>
#include <string_view>
#include <memory>
#include <vector>

namespace Wasp
{
	enum class ScopeType
	{
		NONE,
		MODULE,
		EXPRESSION,
		LOOP,
		BRANCH,
		FUNCTION
	};

	class SymbolScope;
	using SymbolScope_ptr = std::shared_ptr<SymbolScope>;

	class SymbolScope
	{
	private:
		ScopeType type;
		SymbolScope_ptr enclosing_scope;
		int depth;

		std::unordered_map<std::string, Symbol_ptr> symbols;

	public:
		SymbolScope(ScopeType type, SymbolScope_ptr enclosing_scope = nullptr);

		SymbolScope(const SymbolScope &) = delete;
		SymbolScope &operator=(const SymbolScope &) = delete;

		void define(std::string_view name, Symbol_ptr symbol);

		Symbol_ptr lookup(std::string_view name) const;

		bool enclosed_in(ScopeType target_type) const;
		bool enclosed_in(const std::vector<ScopeType> &types) const;

		ScopeType get_type() const { return type; }
		SymbolScope_ptr get_enclosing() const { return enclosing_scope; }

		int get_depth() const { return depth; }
	};
}