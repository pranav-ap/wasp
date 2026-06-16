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
                Doctor::get().fatal(
                    WaspStage::Semantics,
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
    Doctor::get().fatal(WaspStage::Semantics, "Not supported yet");
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
    Doctor::get().fatal(WaspStage::Semantics, "Not supported yet");
}

Type_ptr TypeChecker::visit(TupleLiteral& expr)
{
    Doctor::get().fatal(WaspStage::Semantics, "Not supported yet");
}

Type_ptr TypeChecker::visit(MapLiteral& expr)
{
    Doctor::get().fatal(WaspStage::Semantics, "Not supported yet");
}

Type_ptr TypeChecker::visit(SetLiteral& expr)
{
    Doctor::get().fatal(WaspStage::Semantics, "Not supported yet");
}

} // namespace Wasp
