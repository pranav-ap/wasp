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

void Compiler::visit(Constructor& expr)
{
    for (const auto& arg : expr.values)
    {
        visit(arg);
    }

    visit(expr.construtable);
    emit(OpCode::INSTANTIATE, static_cast<int>(expr.values.size()));
}
} // namespace Wasp
