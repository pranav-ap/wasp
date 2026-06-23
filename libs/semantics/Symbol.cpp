#include "Symbol.h"
#include "AST.h"
#include "Doctor.h"
#include "Type.h"

#include <string>
#include <type_traits>
#include <utility>
#include <variant>

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
    return std::visit(
        [this](auto&& arg) -> Type_ptr
        {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, SymbolAliasSymbol>)
            {
                return arg.target->get_type();
            }
            else if constexpr (requires { arg.type; })
            {
                return arg.type;
            }
            else
            {
                Doctor::semantics().fatal(
                    "Symbol '" + name + "' does not have a type attribute"
                );
            }
        },
        payload
    );
}

void Symbol::set_type(Type_ptr new_type)
{
    std::visit(
        [this, new_type](auto&& arg)
        {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, SymbolAliasSymbol>)
            {
                arg.target->set_type(new_type);
            }
            else if constexpr (requires { arg.type; })
            {
                arg.type = new_type;
            }
            else
            {
                Doctor::semantics().fatal("Cannot set type for symbol: " + name);
            }
        },
        payload
    );
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
    Doctor::semantics().check(
        function_symbol->is<FunctionSymbol>(),
        "Only FunctionSymbol can be added as an overload"
    );

    overloads.push_back(function_symbol);

    FunctionOverloadType_ptr overload_type = type->as<FunctionOverloadType_ptr>();
    overload_type->function_types.push_back(
        function_symbol->get_type()->as<FunctionType_ptr>()
    );
}

void MethodOverloadsSymbol::add_overload(Symbol_ptr method_symbol)
{
    Doctor::semantics().check(
        method_symbol->is<MethodSymbol>(),
        "Only FunctionSymbol can be added as an overload"
    );

    overloads.push_back(method_symbol);

    MethodOverloadType_ptr overload_type = type->as<MethodOverloadType_ptr>();

    overload_type->method_types.push_back(
        method_symbol->get_type()->as<MethodType_ptr>()
    );
}

} // namespace Wasp
