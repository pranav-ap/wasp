#pragma once

#include "Symbol.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace Wasp
{

struct CompilerScope;
using CompilerScope_ptr = std::shared_ptr<CompilerScope>;

struct CompilerScope
{
    CompilerScope_ptr parent;

    // Named variables: symbol ID → QBE variable name (stack slot)
    std::unordered_map<int, std::string> symbol_id_to_qbe_name;

    explicit CompilerScope(CompilerScope_ptr parent = nullptr) : parent(parent)
    {
    }

    void declare_variable(Symbol_ptr symbol, const std::string& qbe_name)
    {
        symbol_id_to_qbe_name[symbol->id] = qbe_name;
    }

    std::string lookup_variable(Symbol_ptr symbol) const
    {
        auto it = symbol_id_to_qbe_name.find(symbol->id);

        if (it != symbol_id_to_qbe_name.end())
        {
            return it->second;
        }

        if (parent)
        {
            return parent->lookup_variable(symbol);
        }

        return "";
    }

    bool contains_local(Symbol_ptr symbol) const
    {
        return symbol_id_to_qbe_name.find(symbol->id) != symbol_id_to_qbe_name.end();
    }
};

} // namespace Wasp
