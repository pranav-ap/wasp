#include "Compiler.h"
#include "AST.h"
#include "Expression.h"
#include "OpCode.h"

#include <string>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void Compiler::visit(IntegerLiteral& expr)
{
    emit(OpCode::LOAD_CONSTANT, workspace->pool->allocate(expr.value), std::to_string(expr.value));
}

void Compiler::visit(FloatLiteral& expr)
{
    emit(OpCode::LOAD_CONSTANT, workspace->pool->allocate(expr.value), std::to_string(expr.value));
}

void Compiler::visit(StringLiteral& expr)
{
    emit(OpCode::LOAD_CONSTANT, workspace->pool->allocate(expr.value), expr.value);
}

void Compiler::visit(BooleanLiteral& expr)
{
    emit(expr.value ? OpCode::LOAD_TRUE : OpCode::LOAD_FALSE);
}

void Compiler::visit(ListLiteral& expr)
{
    visit(expr.expressions);
    emit(OpCode::BUILD_LIST, static_cast<int>(expr.expressions.size()));
}

void Compiler::visit(TupleLiteral& expr)
{
    visit(expr.expressions);
    emit(OpCode::BUILD_TUPLE, static_cast<int>(expr.expressions.size()));
}

void Compiler::visit(SetLiteral& expr)
{
    visit(expr.expressions);
    emit(OpCode::BUILD_SET, static_cast<int>(expr.expressions.size()));
}

void Compiler::visit(MapLiteral& expr)
{
    for (auto& [k, v] : expr.pairs)
    {
        visit(k);
        visit(v);
    }
    emit(OpCode::BUILD_MAP, static_cast<int>(expr.pairs.size()));
}

} // namespace Wasp
