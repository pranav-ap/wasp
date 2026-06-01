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
    visit(access.left);

    if (access.is_module_access)
    {
        emit(OpCode::GET_IMPORTED_MEMBER, access.member_index);
    }

    else if (access.is_field)
    {
        emit(OpCode::GET_FIELD, access.member_index);
    }
}

void Compiler::visit(EnumMember& enum_member)
{
    int const_id = workspace->pool->allocate(enum_member.enum_member_value);
    emit(OpCode::LOAD_CONSTANT, const_id);
}

void Compiler::visit(TemplateAngular& expr)
{
    visit(expr.target);
}

void Compiler::visit(Box& node)
{
    // Push class instance onto stack
    visit(node.expr);
    emit(OpCode::BOX, node.trait_type_id);
}

} // namespace Wasp
