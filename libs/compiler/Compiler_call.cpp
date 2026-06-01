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

void Compiler::visit(Call& expr)
{
    if (expr.callable->is<MemberAccess>())
    {
        auto& member_access = expr.callable->as<MemberAccess>();

        // Push the instance (left side) onto the stack
        visit(member_access.left);

        int overload_index = expr.overload_index;

        if (member_access.is_trait_dispatch)
        {
            emit(
                OpCode::GET_TRAIT_METHOD,
                member_access.member_index,
                overload_index
            );
        }
        else
        {
            emit(
                OpCode::GET_CLASS_METHOD,
                member_access.member_index,
                overload_index
            );
        }

        for (const auto& arg : expr.arguments)
        {
            visit(arg);
        }

        // Total arguments includes the instance (self) + explicit arguments
        int total_arguments = static_cast<int>(expr.arguments.size()) + 1;
        emit(OpCode::CALL, total_arguments);
    }
    else
    {
        // Regular function call
        visit(expr.callable);

        int overload_index = expr.overload_index;
        emit(OpCode::GET_FUNCTION, overload_index);

        for (const auto& arg : expr.arguments)
        {
            visit(arg);
        }

        int total_arguments = static_cast<int>(expr.arguments.size());
        emit(OpCode::CALL, total_arguments);
    }
}

} // namespace Wasp
