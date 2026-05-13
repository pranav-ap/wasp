#include "Compiler.h"
#include "AST.h"
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

void Compiler::visit(Identifier& expr)
{
    auto symbol = expr.symbol;
    Doctor::get().fatal_if_nullptr(symbol, WaspStage::Compiler);

    if (symbol->is_native())
    {
        std::string mangled = mangle_name(
            symbol->name,
            "",
            symbol->module_path
        );
        auto id = workspace->native_registry->get_native_index(mangled);
        emit(OpCode::GET_NATIVE, id, mangled);
    }
    else if (expr.must_be_captured)
    {
        int upval_index = resolve_upvalue(this, symbol);
        emit(OpCode::GET_UPVALUE, upval_index, symbol->name);
    }
    else
    {
        int stack_index = resolve_local(symbol->id);

        Doctor::get().assert(
            stack_index != -1,
            WaspStage::Compiler,
            "Attempted to read an unresolved local variable: " + symbol->name
        );

        emit(OpCode::GET_LOCAL, stack_index, symbol->name);
    }
}

void Compiler::visit(MemberAccess& access)
{
    if (access.is_enum_value)
    {
        int const_id = workspace->pool->allocate(access.member_index);
        emit(OpCode::LOAD_CONST, const_id);
        return;
    }

    visit(access.left);
    emit(OpCode::GET_MEMBER, access.member_index);
}

void Compiler::visit(TemplateAngular& expr)
{
    visit(expr.target);
}

} // namespace Wasp
