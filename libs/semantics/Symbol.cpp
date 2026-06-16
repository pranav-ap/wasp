#include "Symbol.h"
#include "AST.h"
#include "Doctor.h"
#include "Type.h"

#include <string>
#include <utility>

namespace Wasp
{

// ============================================================================
// Constructor
// ============================================================================

Symbol::Symbol(
    int id,
    std::string name,
    int closure_depth,
    int lexical_depth,
    SymbolVariant payload
)
    : name(std::move(name)), id(id), closure_depth(closure_depth),
      lexical_depth(lexical_depth), payload(std::move(payload))
{
}

// ============================================================================
// Methods
// ============================================================================

Type_ptr Symbol::get_type() const
{
    if (is<VariableSymbol>())
    {
        return as<VariableSymbol>().type;
    }

    if (is<FunctionSymbol>())
    {
        return as<FunctionSymbol>().type;
    }

    if (is<TypeSymbol>())
    {
        return as<TypeSymbol>().type;
    }

    if (is<TypeAliasSymbol>())
    {
        return as<TypeAliasSymbol>().type;
    }

    if (is<SymbolAliasSymbol>())
    {
        return as<SymbolAliasSymbol>().target->get_type();
    }

    Doctor::semantics().fatal(
        "Symbol does not have a type attribute : " + name
    );
}

void Symbol::set_type(Type_ptr new_type)
{
    if (is<VariableSymbol>())
    {
        as<VariableSymbol>().type = new_type;
    }
    else if (is<FunctionSymbol>())
    {
        as<FunctionSymbol>().type = new_type;
    }
    else if (is<TypeSymbol>())
    {
        as<TypeSymbol>().type = new_type;
    }
    else if (is<TypeAliasSymbol>())
    {
        as<TypeAliasSymbol>().type = new_type;
    }
    else if (is<SymbolAliasSymbol>())
    {
        as<SymbolAliasSymbol>().target->set_type(new_type);
    }
    else
    {
        Doctor::semantics().fatal("Cannot set type for symbol: " + name);
    }
}

Symbol_ptr Symbol::resolve()
{
    if (is<SymbolAliasSymbol>())
    {
        return as<SymbolAliasSymbol>().target;
    }

    return shared_from_this();
}

bool Symbol::is_global() const
{
    return lexical_depth == 0;
}

bool Symbol::is_exportable() const
{
    return is_global();
}

bool Symbol::should_be_captured(int usage_depth) const
{
    return usage_depth > closure_depth;
}

std::string Symbol::to_string() const
{
    return name + " (id=" + std::to_string(id) + ")";
}

// ============================================================================
// Payload Functions
// ============================================================================

void FunctionOverloadsSymbol::add_overload(Symbol_ptr function_symbol)
{
    Doctor::semantics().assert(
        function_symbol->is<FunctionSymbol>(),
        "Only FunctionSymbol can be added as an overload"
    );

    overloads.push_back(function_symbol);
}

} // namespace Wasp
