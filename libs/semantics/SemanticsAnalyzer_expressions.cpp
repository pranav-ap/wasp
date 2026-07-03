#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "SemanticsAnalyzer.h"
#include "Type.h"
#include "TypeSystem.h"

#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

Type_ptr SemanticsAnalyzer::visit(Expression_ptr expression)
{
    return std::visit(
        [&](auto& node) -> Type_ptr
        {
            if constexpr (requires { visit(node); })
            {
                return visit(node);
            }

            Doctor::semantics().fatal(
                "Unsupported expression type for type inference"
            );
        },
        expression->data
    );
}

TypeVector SemanticsAnalyzer::visit(ExpressionVector& expressions)
{
    TypeVector types;

    for (auto& expr : expressions)
    {
        types.push_back(visit(expr));
    }

    return types;
}

Type_ptr SemanticsAnalyzer::visit(TernaryExpression& expr)
{
    Type_ptr test_type = visit(expr.test);

    if (test_type->is<PrimitiveType_ptr>())
    {
        PrimitiveType_ptr primitive_type = test_type->as<PrimitiveType_ptr>();

        Doctor::semantics().check(
            primitive_type->name == "bool",
            "Test expression must be of boolean type, got: " + test_type->to_string()
        );
    }
    else
    {
        Doctor::semantics().check(
            TypeSystem::is_boolean_type(test_type),
            "Test expression must be of boolean type, got: " + test_type->to_string()
        );
    }

    Type_ptr then_type = visit(expr.then_expr);
    Type_ptr else_type = visit(expr.else_expr);

    Type_ptr result = TypeSystem::unify(current_scope, {then_type, else_type});

    return result;
}

Type_ptr SemanticsAnalyzer::visit(IntegerLiteral&)
{
    return make_shared_type<IntType>();
}

Type_ptr SemanticsAnalyzer::visit(FloatLiteral&)
{
    return make_shared_type<FloatType>();
}

Type_ptr SemanticsAnalyzer::visit(StringLiteral&)
{
    return make_shared_type<StringType>();
}

Type_ptr SemanticsAnalyzer::visit(BooleanLiteral&)
{
    return make_shared_type<BooleanType>();
}

Type_ptr SemanticsAnalyzer::visit(NoneLiteral&)
{
    return make_shared_type<NoneType>();
}

Type_ptr SemanticsAnalyzer::visit(InterpolatedString& expr)
{
    for (auto& part : expr.parts)
    {
        Type_ptr part_type = visit(part);

        Doctor::semantics().check(
            TypeSystem::implements_trait(part_type, "Printable"),
            "Interpolated string parts must be Printable. Got : " + part_type->to_string()
        );
    }

    return make_shared_type<StringType>();
}

Type_ptr SemanticsAnalyzer::visit(ListLiteral& expr)
{
    TypeVector element_types = visit(expr.expressions);

    Type_ptr unified_element_type = TypeSystem::unify(current_scope, element_types);

    return make_shared_type<ListType>(unified_element_type);
}

Type_ptr SemanticsAnalyzer::visit(TupleLiteral& expr)
{
    TypeVector element_types = visit(expr.expressions);
    return make_shared_type<TupleType>(element_types);
}

Type_ptr SemanticsAnalyzer::visit(SetLiteral& expr)
{
    TypeVector element_types = visit(expr.expressions);

    for (auto& type : element_types)
    {
        Doctor::semantics().check(
            TypeSystem::is_key_type(type),
            "Invalid set element type: " + type->to_string()
        );
    }

    Type_ptr unified_element_type = TypeSystem::unify(current_scope, element_types);

    return make_shared_type<SetType>(unified_element_type);
}

Type_ptr SemanticsAnalyzer::visit(MapLiteral& expr)
{
    TypeVector key_types, val_types;

    for (const auto& [k_expr, v_expr] : expr.pairs)
    {
        Type_ptr k_type = visit(k_expr);
        Type_ptr v_type = visit(v_expr);

        Doctor::semantics().check(
            TypeSystem::is_key_type(k_type),
            "Invalid map key type: " + k_type->to_string()
        );

        key_types.push_back(k_type);
        val_types.push_back(v_type);
    }

    Type_ptr unified_key_type = TypeSystem::unify(current_scope, key_types);
    Type_ptr unified_val_type = TypeSystem::unify(current_scope, val_types);

    return make_shared_type<MapType>(unified_key_type, unified_val_type);
}

} // namespace Wasp
