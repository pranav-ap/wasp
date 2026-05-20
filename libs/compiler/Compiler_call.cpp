#include "Compiler.h"
#include "AST.h"
#include "Expression.h"
#include "OpCode.h"


template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void Compiler::visit(FunctionCall& expr)
{
    visit(expr.callable);

    int resolve_idx = expr.overload_index == -1 ? 0 : expr.overload_index;
    emit(OpCode::GET_FUNCTION, resolve_idx);

    for (const auto& arg : expr.arguments)
    {
        visit(arg);
    }

    int total_arguments = static_cast<int>(expr.arguments.size());
    emit(OpCode::CALL, total_arguments);
}

void Compiler::visit(MethodCall& expr)
{
    visit(expr.instance);

    int resolve_idx = expr.overload_index == -1 ? 0 : expr.overload_index;

    if (expr.is_trait_dispatch)
    {
        emit(OpCode::GET_TRAIT_METHOD, expr.method_index, resolve_idx);
    }
    else
    {
        emit(OpCode::GET_CLASS_METHOD, expr.method_index, resolve_idx);
    }

    for (const auto& arg : expr.arguments)
    {
        visit(arg);
    }

    int total_arguments = static_cast<int>(expr.arguments.size());
    emit(OpCode::CALL, total_arguments + 1);
}

} // namespace Wasp
