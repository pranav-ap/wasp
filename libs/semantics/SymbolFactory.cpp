#include "SymbolFactory.h"
#include "AST.h"
#include "Symbol.h"
#include "Type.h"

#include <memory>
#include <string>
#include <utility>

namespace Wasp
{

int SymbolFactory::symbol_id_counter = 0;

void SymbolFactory::reset_counter()
{
    symbol_id_counter = 0;
}

int SymbolFactory::get_current_id()
{
    return symbol_id_counter;
}

Symbol_ptr SymbolFactory::create_symbol(
    const std::string& name,
    SymbolVariant&& payload,
    int closure_depth,
    int lexical_depth
)
{
    return std::make_shared<Symbol>(
        symbol_id_counter++,
        name,
        closure_depth,
        lexical_depth,
        std::move(payload)
    );
}

Symbol_ptr SymbolFactory::create_symbol(SymbolVariant&& payload)
{
    return std::make_shared<Symbol>(
        symbol_id_counter++,
        "",
        0,
        0,
        std::move(payload)
    );
}

Symbol_ptr SymbolFactory::create_variable(
    const std::string& name,
    Type_ptr type,
    bool is_mutable,
    int closure_depth,
    int lexical_depth
)
{
    return create_symbol(
        name,
        VariableSymbol{type, is_mutable},
        closure_depth,
        lexical_depth
    );
}

Symbol_ptr SymbolFactory::create_function(
    const std::string& name,
    Type_ptr type,
    int closure_depth,
    int lexical_depth
)
{
    return create_symbol(
        name,
        FunctionSymbol{type, false, false, false},
        closure_depth,
        lexical_depth
    );
}

Symbol_ptr SymbolFactory::create_type(
    const std::string& name,
    Type_ptr type,
    int closure_depth,
    int lexical_depth
)
{
    return create_symbol(name, TypeSymbol{type}, closure_depth, lexical_depth);
}

Symbol_ptr SymbolFactory::create_type_alias(
    const std::string& name,
    Type_ptr type,
    int closure_depth,
    int lexical_depth
)
{
    return create_symbol(
        name,
        TypeAliasSymbol{type},
        closure_depth,
        lexical_depth
    );
}

Symbol_ptr SymbolFactory::create_symbol_alias(
    const std::string& name,
    Symbol_ptr target,
    int closure_depth,
    int lexical_depth
)
{
    return create_symbol(
        name,
        SymbolAliasSymbol{target},
        closure_depth,
        lexical_depth
    );
}

} // namespace Wasp
