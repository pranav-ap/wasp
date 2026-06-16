#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Type.h"
#include "TypeChecker.h"

#include <string>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

Type_ptr TypeChecker::visit(Expression_ptr expression)
{
    return std::visit(
        [&](auto& node) -> Type_ptr
        {
            if constexpr (requires { visit(node); })
            {
                return visit(node);
            }
            else
            {
                Doctor::semantics().fatal(
                    "Unsupported expression type for type inference"
                );
            }
        },
        expression->data
    );
}

TypeVector TypeChecker::visit(std::vector<Expression_ptr>& expressions)
{
    TypeVector types;

    for (auto& expr : expressions)
    {
        types.push_back(visit(expr));
    }

    return types;
}

Type_ptr TypeChecker::visit(TernaryExpression& expr)
{
    Type_ptr test_type = visit(expr.test);

    Doctor::semantics().assert(
        type_system->is_boolean_type(test_type),
        "Test expression must be of boolean type, got: " +
            test_type->to_string()
    );

    Type_ptr then_type = visit(expr.then_expr);
    Type_ptr else_type = visit(expr.else_expr);

    Type_ptr result = type_system->unify(
        current_scope,
        {then_type, else_type}
    );

    return result;
}

Type_ptr TypeChecker::visit(IntegerLiteral&)
{
    return make_shared_type<IntType>();
}

Type_ptr TypeChecker::visit(FloatLiteral&)
{
    return make_shared_type<FloatType>();
}

Type_ptr TypeChecker::visit(StringLiteral&)
{
    return make_shared_type<StringType>();
}

Type_ptr TypeChecker::visit(BooleanLiteral&)
{
    return make_shared_type<BooleanType>();
}

Type_ptr TypeChecker::visit(NoneLiteral&)
{
    return make_shared_type<NoneType>();
}

Type_ptr TypeChecker::visit(ListLiteral& expr)
{
    TypeVector element_types = visit(expr.expressions);

    Type_ptr unified_element_type = type_system->unify(
        current_scope,
        element_types
    );

    return make_shared_type<ListType>(unified_element_type);
}

Type_ptr TypeChecker::visit(TupleLiteral& expr)
{
    TypeVector element_types = visit(expr.expressions);
    return make_shared_type<TupleType>(element_types);
}

Type_ptr TypeChecker::visit(SetLiteral& expr)
{
    TypeVector element_types = visit(expr.expressions);

    for (auto& type : element_types)
    {
        Doctor::semantics().assert(
            type_system->is_key_type(type),
            "Invalid set element type: " + type->to_string()
        );
    }

    Type_ptr unified_element_type = type_system->unify(
        current_scope,
        element_types
    );

    return make_shared_type<SetType>(unified_element_type);
}

Type_ptr TypeChecker::visit(MapLiteral& expr)
{
    TypeVector key_types, val_types;

    for (const auto& [k_expr, v_expr] : expr.pairs)
    {
        Type_ptr k_type = visit(k_expr);
        Type_ptr v_type = visit(v_expr);

        Doctor::semantics().assert(
            type_system->is_key_type(k_type),
            "Invalid map key type: " + k_type->to_string()
        );

        key_types.push_back(k_type);
        val_types.push_back(v_type);
    }

    Type_ptr unified_key_type = type_system->unify(
        current_scope,
        key_types
    );
    Type_ptr unified_val_type = type_system->unify(
        current_scope,
        val_types
    );

    return make_shared_type<MapType>(unified_key_type, unified_val_type);
}

} // namespace Wasp
