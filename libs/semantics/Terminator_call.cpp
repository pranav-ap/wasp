#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Symbol.h"
#include "SymbolScope.h"
#include "Terminator.h"
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

Type_ptr resolve_function(
    Call& call,
    Symbol_ptr functions_symbol,
    const TypeVector& generic_types,
    const TypeVector& argument_types,
    SymbolScope_ptr scope,
    TypeSystem_ptr type_system
)
{
    auto [function_symbol, raw_index] = type_system->get_best_function(
        scope,
        functions_symbol,
        generic_types,
        argument_types
    );

    auto function_type = function_symbol->get_type()->as<FunctionType_ptr>();

    call.overload_index = raw_index;

    return function_type->signature->return_type;
}

Type_ptr resolve_method(
    Call& call,
    MemberAccess& ma,
    const TypeVector& generic_types,
    const TypeVector& argument_types,
    OopsType_ptr owner_type,
    SymbolScope_ptr scope,
    TypeSystem_ptr type_system
)
{
    auto method_name = ma.member->as<Identifier>().name;
    auto method_overload_type = owner_type->methods->get_type(method_name);

    auto [method_type, overload] = type_system->get_best_method(
        scope,
        method_overload_type,
        argument_types
    );

    ma.member_index = owner_type->methods->get_index(method_name);
    call.overload_index = overload;

    return method_type->signature->return_type;
}

} // namespace

Type_ptr Terminator::visit(Call& call)
{
    TypeVector argument_types = visit(call.arguments);
    TypeVector generic_types = visit(call.angular_nodes);

    return std::visit(
        overloaded{
            [&](Identifier& id)
            {
                return handle_call(
                    call,
                    id,
                    generic_types,
                    argument_types
                );
            },
            [&](MemberAccess& ma)
            {
                return handle_call(
                    call,
                    ma,
                    generic_types,
                    argument_types
                );
            },
            [&](auto&) -> Type_ptr
            {
                Doctor::semantics().fatal("Invalid callable");
            }
        },
        call.callee->data
    );
}

Type_ptr Terminator::handle_call(
    Call& call,
    Identifier& identifier,
    const TypeVector& generic_types,
    const TypeVector& argument_types
)
{
    identifier.symbol = current_scope->lookup_functions(identifier.name);

    if (identifier.symbol->should_be_captured(
            current_scope->closure_depth
        ))
    {
        identifier.must_be_captured = true;
    }

    return resolve_function(
        call,
        identifier.symbol,
        generic_types,
        argument_types,
        current_scope,
        type_system
    );
}

Type_ptr Terminator::handle_call(
    Call& call,
    MemberAccess& access,
    TypeVector& generic_types,
    TypeVector& argument_types
)
{
    Type_ptr left_type = visit(access.owner);
    left_type = left_type->unwrap_alias();

    return std::visit(
        overloaded{
            [&](ClassType_ptr class_type) -> Type_ptr
            {
                call.owner_kind = Call::OwnerKind::CLASS;
                call.owner_name = class_type->name;

                return resolve_method(
                    call,
                    access,
                    generic_types,
                    argument_types,
                    class_type,
                    current_scope,
                    type_system
                );
            },

            [&](TraitType_ptr trait_type) -> Type_ptr
            {
                call.owner_kind = Call::OwnerKind::TRAIT;
                call.owner_name = trait_type->name;

                return resolve_method(
                    call,
                    access,
                    generic_types,
                    argument_types,
                    trait_type,
                    current_scope,
                    type_system
                );
            },

            [&](PrimitiveType_ptr primitive_type) -> Type_ptr
            {
                call.owner_kind = Call::OwnerKind::PRIMITIVE;
                call.owner_name = primitive_type->name;

                return resolve_method(
                    call,
                    access,
                    generic_types,
                    argument_types,
                    primitive_type,
                    current_scope,
                    type_system
                );
            },

            [&](auto&) -> Type_ptr
            {
                Doctor::semantics().fatal("Invalid member call LHS");
            }
        },
        left_type->data
    );
}

} // namespace Wasp
