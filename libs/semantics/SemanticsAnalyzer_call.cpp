#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "SemanticsAnalyzer.h"
#include "Symbol.h"
#include "SymbolScope.h"
#include "Type.h"

#include <cstddef>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

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
    TypeVector solid_types = visit(call.angular_nodes);

    return std::visit(
        overloaded{
            [&](Identifier& id)
            {
                return visit(call, id, solid_types, argument_types);
            },
            [&](MemberAccess& ma)
            {
                return visit(call, ma, solid_types, argument_types);
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
    if (soild_types.empty())
    {
        identifier.symbol = current_scope->lookup_required_and_resolve(identifier.name);

        Doctor::semantics().check(
            identifier.symbol->is<TypeOverloadsSymbol>(),
            "Symbol '" + identifier.name + "' is not an overloaded function"
        );

        bool should_capture = identifier.symbol->should_be_captured(current_scope->closure_depth);

        if (should_capture)
        {
            identifier.must_be_captured = true;
        }

        auto [function_symbol, raw_index] = resolve_function(identifier.symbol, argument_types);

        FunctionType_ptr function_type = function_symbol->get_type()->as<FunctionType_ptr>();
        call.overload_index = raw_index;

        return function_type->return_type;
    }

    std::string mangled_name = type_system->mangle_name(soild_types);
    mangled_name = identifier.name + "_" + mangled_name;

    Symbol_ptr symbol = current_scope->lookup(mangled_name);

    if (symbol)
    {
    }

    Doctor::semantics().fatal("Generic function calls are not yet supported");
}

Type_ptr SemanticsAnalyzer::visit(
    Call& call,
    MemberAccess& access,
    TypeVector& solid_types,
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

    auto [method_type, overload] = resolve_method(method_overload_type, argument_types);

    ma.member_index = owner_type->methods->get_index(method_name);
    call.overload_index = overload;

    return method_type->return_type;
}

std::tuple<Symbol_ptr, int> SemanticsAnalyzer::resolve_function(
    const Symbol_ptr symbol,
    const TypeVector& argument_types
) const
{
    Doctor::semantics().check(
        symbol->is<TypeOverloadsSymbol>(),
        "Symbol '" + symbol->name + "' is not an overloaded function"
    );

    const SymbolVector& candidates = symbol->as<TypeOverloadsSymbol>().overloads;

    struct Candidate
    {
        Symbol_ptr symbol;
        int index;
        FunctionType_ptr function_type;
    };

    std::vector<Candidate> viable;

    for (size_t i = 0; i < candidates.size(); ++i)
    {
        const Symbol_ptr& function_symbol_obj = candidates[i];
        const TypeSymbol& function_symbol = function_symbol_obj->as<TypeSymbol>();
        const FunctionType_ptr function_type = function_symbol.type->as<FunctionType_ptr>();

        // Skip template functions
        if (!function_type->template_type->empty())
        {
            continue;
        }

        // Skip if arity doesn't match
        if (function_type->parameter_types.size() != argument_types.size())
        {
            continue;
        }

        // Check if arguments are assignable to parameters
        bool found_any_unassignable = false;

        for (size_t j = 0; j < argument_types.size(); ++j)
        {
            found_any_unassignable = !type_system->assignable(
                current_scope,
                function_type->parameter_types[j],
                argument_types[j]
            );

            if (found_any_unassignable)
            {
                break;
            }
        }

        if (!found_any_unassignable)
        {
            viable.push_back({function_symbol_obj, static_cast<int>(i), function_type});
        }
    }

    Doctor::semantics().check(!viable.empty(), "No viable candidates for function " + symbol->name);

    // Only one candidate. Return it.
    if (viable.size() == 1)
    {
        return {viable[0].symbol, viable[0].index};
    }

    Doctor::semantics().fatal("Ambiguous call to '" + symbol->name + "'");
}

std::tuple<MethodType_ptr, int> SemanticsAnalyzer::resolve_method(
    const MethodOverloadType_ptr method_overload_type,
    const TypeVector& argument_types
) const
{
    struct Candidate
    {
        MethodType_ptr method_type;
        int index;
    };

    std::vector<Candidate> viable;

    for (size_t i = 0; i < method_overload_type->method_types.size(); ++i)
    {
        auto& method_type = method_overload_type->method_types[i];

        // Skip if arity doesn't match
        if (method_type->parameter_types.size() != argument_types.size())
        {
            continue;
        }

        // Check if arguments are assignable to parameters
        bool all_assignable = true;

        for (size_t j = 0; j < argument_types.size(); ++j)
        {
            bool is_assignable = type_system->assignable(
                current_scope,
                method_type->parameter_types[j],
                argument_types[j]
            );

            if (!is_assignable)
            {
                all_assignable = false;
                break;
            }
        }

        if (all_assignable)
        {
            viable.push_back({method_type, static_cast<int>(i)});
        }
    }

    Doctor::semantics().check(!viable.empty(), "No viable candidates for function call");

    // Only one candidate.
    // Return it.
    if (viable.size() == 1)
    {
        return {viable[0].method_type, viable[0].index};
    }

    Doctor::semantics().fatal("Ambiguous method call");
}

} // namespace Wasp
