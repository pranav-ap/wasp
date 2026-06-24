#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "SemanticsAnalyzer.h"
#include "Symbol.h"
#include "SymbolScope.h"
#include "Type.h"

#include <string>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

Type_ptr SemanticsAnalyzer::visit(Call& call)
{
    TypeVector argument_types = visit(call.arguments);
    TypeVector generic_types = visit(call.angular_nodes);

    return std::visit(
        overloaded{
            [&](Identifier& id)
            {
                return visit(call, id, generic_types, argument_types);
            },
            [&](MemberAccess& ma)
            {
                return visit(call, ma, generic_types, argument_types);
            },
            [&](auto&) -> Type_ptr
            {
                Doctor::semantics().fatal("Invalid callable");
            }
        },
        call.callee->data
    );
}

Type_ptr SemanticsAnalyzer::visit(
    Call& call,
    Identifier& identifier,
    const TypeVector& soild_types,
    const TypeVector& argument_types
)
{
    identifier.symbol = current_scope->lookup_functions(identifier.name);

    bool should_capture = identifier.symbol->should_be_captured(current_scope->closure_depth);

    if (should_capture)
    {
        identifier.must_be_captured = true;
    }

    if (soild_types.empty())
    {
        auto [function_symbol, raw_index] = type_system->get_best_function(
            current_scope,
            identifier.symbol,
            argument_types
        );

        FunctionType_ptr function_type = function_symbol->get_type()->as<FunctionType_ptr>();

        call.overload_index = raw_index;

        return function_type->signature->return_type;
    }

    std::string mangled_name = type_system->mangle_name(soild_types);
    mangled_name = identifier.name + "_" + mangled_name;

    Symbol_ptr symbol = current_scope->lookup_functions(mangled_name);

    Doctor::semantics().fatal("Generic function calls are not yet supported");
}

Type_ptr SemanticsAnalyzer::visit(
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

                return resolve_method(call, access, argument_types, class_type);
            },

            [&](TraitType_ptr trait_type) -> Type_ptr
            {
                call.owner_kind = Call::OwnerKind::TRAIT;
                call.owner_name = trait_type->name;

                return resolve_method(call, access, argument_types, trait_type);
            },

            [&](PrimitiveType_ptr primitive_type) -> Type_ptr
            {
                call.owner_kind = Call::OwnerKind::PRIMITIVE;
                call.owner_name = primitive_type->name;

                return resolve_method(call, access, argument_types, primitive_type);
            },

            [&](auto&) -> Type_ptr
            {
                Doctor::semantics().fatal("Invalid member call LHS");
            }
        },
        left_type->data
    );
}

Type_ptr SemanticsAnalyzer::resolve_method(
    Call& call,
    MemberAccess& ma,
    const TypeVector& argument_types,
    OopsType_ptr owner_type
)
{
    std::string method_name = ma.member->as<Identifier>().name;

    MethodOverloadType_ptr method_overload_type = owner_type->methods->get_type(method_name);

    auto [method_type, overload] = type_system->get_best_method(
        current_scope,
        method_overload_type,
        argument_types
    );

    ma.member_index = owner_type->methods->get_index(method_name);
    call.overload_index = overload;

    return method_type->signature->return_type;
}

} // namespace Wasp
