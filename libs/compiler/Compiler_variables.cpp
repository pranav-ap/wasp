#include "AST.h"
#include "Compiler.h"
#include "Doctor.h"
#include "Expression.h"
#include "OpCode.h"
#include "Workspace.h"

#include <memory>
#include <string>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void Compiler::visit(Assignment& expr)
{
    if (expr.lhs->is<Identifier>())
    {
        visit(expr.rhs);

        auto& id = expr.lhs->as<Identifier>();
        auto symbol = id.symbol;
        Doctor::get().fatal_if_nullptr(symbol, WaspStage::Compiler);

        if (id.must_be_captured)
        {
            int upval_index = resolve_upvalue(this, symbol);
            emit(OpCode::SET_UPVALUE, upval_index, symbol->name);
            emit(OpCode::GET_UPVALUE, upval_index, symbol->name);
        }
        else
        {
            int physical_index = get_or_add_local_index(symbol);
            emit(OpCode::SET_LOCAL, physical_index, symbol->name);
            emit(OpCode::GET_LOCAL, physical_index, symbol->name);
        }
    }
    else if (expr.lhs->is<MemberAccess>())
    {
        auto& mac = expr.lhs->as<MemberAccess>();
        compile_member_assignment(mac, expr.rhs);
    }
    else
    {
        Doctor::get().fatal(
            WaspStage::Compiler,
            "Left-hand side of assignment must be an Identifier or MemberAccess"
        );
    }
}

void Compiler::compile_member_assignment(
    MemberAccess& access,
    const Expression_ptr& value
)
{
    visit(access.left);
    visit(value);

    Doctor::get().assert(
        access.right->is<Identifier>(),
        WaspStage::Compiler,
        "Right side of member assignment must be an Identifier"
    );

    auto target_name = access.right->as<Identifier>().name;
    emit(OpCode::SET_MEMBER, access.member_index, target_name);
}

} // namespace Wasp
