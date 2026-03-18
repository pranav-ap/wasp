#include "SymbolScope.h"
#include "Doctor.h"
#include "Symbol.h"

#include <string>
#include <utility>
#include <vector>

namespace Wasp {
SymbolScope::SymbolScope(ScopeType type, SymbolScope_ptr enclosing)
    : type(type), enclosing_scope(std::move(enclosing)), closure_depth(0), lexical_depth(0) {
    if (enclosing_scope) {
        closure_depth = enclosing_scope->closure_depth + (type == ScopeType::FUNCTION ? 1 : 0);
        lexical_depth = enclosing_scope->lexical_depth + 1;
    }
}

int SymbolScope::get_symbols_count() const {
    int count = symbols.size();

    // Sum up all overloaded functions
    for (const auto& [name, overloads] : function_symbols) {
        count += overloads.size();
    }

    return count;
}

Symbol_ptr SymbolScope::define(Symbol_ptr symbol) {
    Doctor::get().fatal_if_nullptr(symbol, WaspStage::Semantics);

    symbol->id = get_symbols_count();

    if (symbol->payload_is<FunctionData>()) {
        Doctor::get().assert(
            symbols.find(symbol->name) == symbols.end(),
            WaspStage::Semantics,
            "Name collision: '" + symbol->name + "' is already declared as a variable."
        );

        function_symbols[symbol->name].push_back(symbol);
        return symbol;
    }

    Doctor::get().assert(
        function_symbols.find(symbol->name) == function_symbols.end(),
        WaspStage::Semantics,
        "Name collision: '" + symbol->name + "' is already declared as a function."
    );

    Doctor::get().assert(
        symbols.find(symbol->name) == symbols.end(),
        WaspStage::Semantics,
        "Variable '" + symbol->name + "' already declared in this scope."
    );

    symbols[symbol->name] = symbol;

    return symbol;
}

Symbol_ptr SymbolScope::lookup(std::string name) const {
    const SymbolScope* current = this;

    while (current) {
        auto it = current->symbols.find(name);
        if (it != current->symbols.end()) {
            return it->second;
        }

        // check if it's a function
        // just send the first one you find, the caller will handle the overloads

        auto func_it = current->function_symbols.find(name);
        if (func_it != current->function_symbols.end() && !func_it->second.empty()) {
            return func_it->second.front();
        }

        current = current->enclosing_scope.get();
    }

    return nullptr;
}

std::vector<Symbol_ptr> SymbolScope::collect_function_symbols(const std::string& name) const {
    std::vector<Symbol_ptr> result;
    const SymbolScope* current = this;

    while (current) {
        auto it = current->function_symbols.find(name);

        if (it != current->function_symbols.end()) {
            // Append all overloads found in this specific scope
            result.insert(result.end(), it->second.begin(), it->second.end());
        }

        current = current->enclosing_scope.get();
    }

    return result;
}

bool SymbolScope::enclosed_in(ScopeType target_type) const {
    const SymbolScope* current = this;
    while (current) {
        if (current->type == target_type) {
            return true;
        }

        current = current->enclosing_scope.get();
    }
    return false;
}

bool SymbolScope::enclosed_in(const std::vector<ScopeType>& types) const {
    const SymbolScope* current = this;
    while (current) {
        for (auto t : types) {
            if (current->type == t)
                return true;
        }
        current = current->enclosing_scope.get();
    }
    return false;
}
} // namespace Wasp
