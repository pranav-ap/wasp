#include "AST.h"
#include "Compiler.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
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
    auto symbol = expr.symbol; //->resolve();
    Doctor::get().fatal_if_nullptr(symbol, WaspStage::Compiler);

    if (symbol->is_native())
    {
        std::string mangled = mangle_name(
            symbol->name,
            "", // No class name for free functions
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

// ===========================================================================
// ASSIGNMENTS
// ===========================================================================

void Compiler::compile_member_assignment(MemberAccess& access, const Expression_ptr& value)
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

void Compiler::compile_identifier_assignment(Identifier& id, const Expression_ptr& rhs)
{
    visit(rhs);

    auto symbol = id.symbol;
    Doctor::get().fatal_if_nullptr(symbol, WaspStage::Compiler);

    if (id.must_be_captured)
    {
        int idx = resolve_upvalue(this, symbol);
        emit(OpCode::SET_UPVALUE, idx, symbol->name);
    }
    else
    {
        int stack_index = resolve_local(symbol->id);

        Doctor::get().assert(
            stack_index != -1,
            WaspStage::Compiler,
            "Attempted to assign to an unresolved local variable: " + symbol->name
        );

        emit(OpCode::SET_LOCAL, stack_index, symbol->name);
    }
}

// ===========================================================================
// CALL & CONSTRUCTOR
// ==========================================================================

void Compiler::visit(Call& expr)
{
    visit(expr.callable);

    int resolve_idx = expr.overload_index == -1 ? 0 : expr.overload_index;

    emit(OpCode::RESOLVE_FUNCTION, resolve_idx);

    int total_arguments = static_cast<int>(expr.arguments.size());

    if (expr.is_method_call && !expr.is_pure_method_call)
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

void Compiler::visit(Constructor& expr)
{
    Symbol_ptr class_symbol;

    if (expr.construtable->is<Identifier>())
    {
        class_symbol = expr.construtable->as<Identifier>().symbol;
    }
    else if (expr.construtable->is<MemberAccess>())
    {
        class_symbol = expr.construtable->as<MemberAccess>().right->as<Identifier>().symbol;
    }
    else if (expr.construtable->is<ConcreteTemplate>())
    {
        class_symbol = expr.construtable->as<ConcreteTemplate>().symbol;
    }
    else
    {
        Doctor::get().fatal(
            WaspStage::Compiler,
            "Construtable must be an identifier, member access, or template instantiation."
        );
    }

    auto class_type = class_symbol->get_type()->as<std::shared_ptr<ClassType>>();

    Doctor::get().assert(
        expr.values.size() == class_type->fields.size(),
        WaspStage::Compiler,
        "Compiler error: Argument count does not match instance field count."
    );

    for (const auto& arg : expr.values)
    {
        visit(arg);
    }

    visit(expr.construtable);
    emit(OpCode::INSTANTIATE, static_cast<int>(expr.values.size()));
}

void Compiler::visit(ConcreteTemplate& expr)
{
    visit(expr.target);
}

} // namespace Wasp
