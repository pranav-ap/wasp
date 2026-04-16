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

void Compiler::visit(MemberAccess& access)
{
    visit(access.left);
    emit(OpCode::GET_MEMBER, access.member_index);
}

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
    Symbol_ptr class_symbol;

    if (expr.callable->is<Identifier>())
    {
        class_symbol = expr.callable->as<Identifier>().symbol;
    }
    else if (expr.callable->is<MemberAccess>())
    {
        class_symbol = expr.callable->as<MemberAccess>().right->as<Identifier>().symbol;
    }
    else
    {
        Doctor::get().fatal(
            WaspStage::Compiler,
            "Callable must be an identifier or member access."
        );
    }

    auto class_type = class_symbol->get_type()->as<std::shared_ptr<ClassType>>();

    Doctor::get().assert(
        expr.arguments.size() == class_type->fields.size(),
        WaspStage::Compiler,
        "Compiler error: Argument count does not match instance field count."
    );

    for (const auto& arg : expr.arguments)
    {
        visit(arg);
    }

    for (const std::string& method_name : class_type->methods)
    {
        std::string mangled_name = class_type->name + "::" + method_name;
        int method_physical_index = resolve_local(mangled_name);

        if (method_physical_index != -1)
        {
            emit(OpCode::GET_LOCAL, method_physical_index, "method " + mangled_name);
        }
        else
        {
            auto& mac = expr.callable->as<MemberAccess>();

            visit(mac.left);

            auto module_symbol = mac.left->as<Identifier>().symbol->resolve();
            auto module_type = module_symbol->get_type()->as<std::shared_ptr<ModuleType>>();

            if (!module_type->contains_member(mangled_name))
            {
                Object_ptr method_type = class_type->get_member(method_name);
                module_type->set_member(mangled_name, method_type);
            }

            int mod_index = module_type->get_member_index(mangled_name);
            emit(OpCode::GET_MEMBER, mod_index, "imported method " + mangled_name);
        }
    }

    int total_size = static_cast<int>(class_type->fields.size() + class_type->methods.size());
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
