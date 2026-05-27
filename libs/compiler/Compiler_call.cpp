#include "AST.h"
#include "Compiler.h"
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

void Compiler::visit(ClassMethodCall& expr)
{
    // Push the instance onto the stack
    visit(expr.instance);

    int resolve_idx = expr.overload_index == -1 ? 0 : expr.overload_index;

    // Get the class method by its index
    emit(OpCode::GET_CLASS_METHOD, expr.method_index, resolve_idx);

    // Visit all arguments
    for (const auto& arg : expr.arguments)
    {
        visit(arg);
    }

    // Total arguments includes the instance (self) + explicit arguments
    int total_arguments = static_cast<int>(expr.arguments.size()) + 1;
    emit(OpCode::CALL, total_arguments);
}

void Compiler::visit(TraitMethodCall& expr)
{
    // Push the instance onto the stack
    visit(expr.instance);

    int resolve_idx = expr.overload_index == -1 ? 0 : expr.overload_index;

    // Get the trait method using trait_type_id and method_index
    emit(OpCode::GET_TRAIT_METHOD, expr.method_index, resolve_idx);

    // Visit all arguments
    for (const auto& arg : expr.arguments)
    {
        visit(arg);
    }

    // Total arguments includes the instance (self) + explicit arguments
    int total_arguments = static_cast<int>(expr.arguments.size()) + 1;
    emit(OpCode::CALL, total_arguments);
}

} // namespace Wasp
