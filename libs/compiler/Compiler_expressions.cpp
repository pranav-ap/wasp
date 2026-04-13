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
    auto symbol = expr.symbol;
    Doctor::get().fatal_if_nullptr(symbol, WaspStage::Compiler);

    if (symbol->is_native())
    {
        auto native_registry_id = workspace->native_registry->get_native_index(symbol->name);
        emit(OpCode::GET_NATIVE, native_registry_id, symbol->name);
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

void Compiler::visit(MemberAccess& expr)
{
    visit(expr.left);
    emit(OpCode::GET_MEMBER, expr.member_index);
}

// ===========================================================================
// ASSIGNMENTS
// ===========================================================================

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

void Compiler::compile_member_assignment(MemberAccess& mac, const Expression_ptr& rhs)
{
    // Evaluate the object first (Stack: [obj])
    visit(mac.left);

    // Evaluate the value second (Stack: [obj, val])
    visit(rhs);

    Doctor::get().assert(
        mac.right->is<Identifier>(),
        WaspStage::Compiler,
        "Right side of member assignment must be an Identifier"
    );

    emit(OpCode::SET_MEMBER, mac.member_index, mac.right->as<Identifier>().name);
}

// ===========================================================================
// CALL
// ==========================================================================

void Compiler::visit(Call& expr)
{
    if (expr.is_constructor_call)
    {
        compile_constructor_call(expr);
    }
    else
    {
        compile_function_call(expr);
    }
}

void Compiler::compile_constructor_call(Call& expr)
{
    visit(expr.callable);

    auto symbol = expr.callable->as<Identifier>().symbol;
    auto class_type = symbol->get_type()->as<std::shared_ptr<ClassType>>();

    int arg_index = 0;
    int total_size = 0;

    for (const std::string& member_name : class_type->declaration_order)
    {
        // Skip shared members ('our' variables and 'our' methods)
        if (class_type->shared_members.contains(member_name))
            continue;

        auto type_obj = class_type->members.at(member_name);

        if (type_obj->is<std::shared_ptr<MyMethodType>>())
        {
            std::string mangled_name = class_type->name + "::" + member_name;

            int method_physical_index = -1;
            for (int i = static_cast<int>(locals.size()) - 1; i >= 0; --i)
            {
                if (locals[i]->name == mangled_name)
                {
                    method_physical_index = i;
                    break;
                }
            }

            Doctor::get().assert(
                method_physical_index != -1,
                WaspStage::Compiler,
                "Compiler error: Could not find method " + mangled_name + " in locals."
            );

            emit(OpCode::GET_LOCAL, method_physical_index, "method " + mangled_name);
        }
        else
        {
            // It's an instance field! Evaluate the next provided constructor argument.
            Doctor::get().assert(
                arg_index < expr.arguments.size(),
                WaspStage::Compiler,
                "Compiler error: Not enough arguments provided for constructor."
            );

            visit(expr.arguments[arg_index]);
            arg_index++;
        }

        total_size++;
    }

    emit(OpCode::INSTANTIATE, total_size);
}

void Compiler::compile_function_call(Call& expr)
{
    visit(expr.callable);

    if (expr.overload_index != -1)
    {
        emit(OpCode::RESOLVE_FUNCTION, expr.overload_index);
    }

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
