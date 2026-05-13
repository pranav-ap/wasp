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

void Compiler::visit(Call& expr)
{
    visit(expr.callable);

    int resolve_idx = expr.overload_index == -1 ? 0 : expr.overload_index;
    emit(OpCode::RESOLVE_FUNCTION, resolve_idx);

    int total_arguments = static_cast<int>(expr.arguments.size());

    if (expr.is_method_call)
    {
        auto& mac = expr.callable->as<MemberAccess>();
        visit(mac.left);
        total_arguments++;
    }

    for (const auto& arg : expr.arguments)
    {
        visit(arg);
    }

    emit(OpCode::CALL, total_arguments);
}

} // namespace Wasp
