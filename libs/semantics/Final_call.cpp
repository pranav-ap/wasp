#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Final.h"
#include "Symbol.h"
#include "SymbolScope.h"
#include "Type.h"
#include "TypeSystem.h"

#include <string>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

namespace
{

// ============================================================================
// Call Handlers
// ============================================================================

Type_ptr resolve_standard_overload(
    Call& call,
    Symbol_ptr functions_symbol,
    const TypeVector& generic_types,
    const TypeVector& argument_types,
    SymbolScope_ptr scope,
    TypeSystem_ptr type_system
)
{
    auto [function_symbol, raw_index] = type_system
                                            ->get_best_function_symbol(
                                                scope,
                                                functions_symbol,
                                                generic_types,
                                                argument_types
                                            );

    auto signature = function_symbol->get_type()->as<Signature_ptr>();

    call.overload_index = raw_index;

    return signature->return_type;
}

Type_ptr handle_identifier_call(
    Call& call,
    Identifier& identifier,
    const TypeVector& generic_types,
    const TypeVector& argument_types,
    SymbolScope_ptr scope,
    TypeSystem_ptr type_system
)
{
    identifier.symbol = scope->lookup_functions(identifier.name);

    if (identifier.symbol->should_be_captured(scope->closure_depth))
    {
        identifier.must_be_captured = true;
    }

    return resolve_standard_overload(
        call,
        identifier.symbol,
        generic_types,
        argument_types,
        scope,
        type_system
    );
}
} // namespace

Type_ptr Final::visit(Call& call)
{
    TypeVector argument_types = visit(call.arguments);
    TypeVector generic_types = visit(call.angular_nodes);

    return std::visit(
        overloaded{
            [&](Identifier& id)
            {
                return handle_identifier_call(
                    call,
                    id,
                    generic_types,
                    argument_types,
                    current_scope,
                    type_system
                );
            },
            // [&](MemberAccess& ma)
            // {
            //     return handle_member_call(call, ma, argument_types);
            // },
            [&](auto&) -> Type_ptr
            {
                Doctor::semantics().fatal("Invalid callable");
            }
        },
        call.callee->data
    );
}

} // namespace Wasp
