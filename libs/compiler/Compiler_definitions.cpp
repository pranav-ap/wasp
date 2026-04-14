#include "CFGraph.h"
#include "Compiler.h"
#include "Objects.h"
#include "OpCode.h"
#include "Statement.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{
// ============================================================================
// CLASS
// ============================================================================

void Compiler::compile_class_members(ClassDefinition& class_definition)
{
    for (auto& stmt : class_definition.members)
    {
        std::visit(
            overloaded{
                [&](MethodDefinition& method)
                {
                    visit(method);
                },
                [&](auto&)
                {
                }
            },
            stmt->data
        );
    }
}

void Compiler::visit(ClassDefinition& class_definition)
{
    compile_class_members(class_definition);

    auto class_type_obj = class_definition.symbol->get_type();
    auto class_type = class_type_obj->as<std::shared_ptr<ClassType>>();

    int arity = static_cast<int>(class_type->fields.size() + class_type->methods.size());
    Object_ptr runtime_blueprint = make_object(
        std::make_shared<ClassObject>(class_type->name, arity)
    );

    int const_id = workspace->pool->allocate(runtime_blueprint);
    emit(OpCode::LOAD_CONST, const_id, "class " + class_definition.name);

    int physical_index = resolve_local(class_definition.symbol->id);

    if (physical_index == -1)
    {
        physical_index = static_cast<int>(locals.size());
        locals.push_back(class_definition.symbol);
    }

    emit(OpCode::SET_LOCAL, physical_index, "class " + class_definition.name);
}

// ==========================================================================
// FUNCTIONS & METHODS
// ==========================================================================

void Compiler::compile_abstract_function(AbstractFunctionDefinition& function_definition)
{
    Compiler func_compiler(this);

    func_compiler.enter_scope();

    for (const auto& param_symbol : function_definition.parameter_symbols)
    {
        func_compiler.locals.push_back(param_symbol);
    }

    func_compiler.visit(function_definition.body);
    func_compiler.leave_scope();

    func_compiler.emit(OpCode::LOAD_NONE);
    func_compiler.emit(OpCode::RETURN);

    CodeObject code = func_compiler.flatten();

    int const_id = workspace->pool->allocate_function_definition(
        std::move(code),
        function_definition.name
    );

    emit(OpCode::LOAD_CONST, const_id, "fun " + function_definition.name);

    int upvalue_count = static_cast<int>(func_compiler.upvalues.size());
    emit(OpCode::MAKE_FUNCTION, upvalue_count);

    for (const auto& uv : func_compiler.upvalues)
    {
        if (uv.is_local_to_parent)
        {
            emit_raw_byte(std::byte{1});
        }
        else
        {
            emit_raw_byte(std::byte{0});
        }

        emit_raw_byte(static_cast<std::byte>(uv.index));
    }

    int physical_index = resolve_local(function_definition.group_symbol->id);

    if (physical_index == -1)
    {
        physical_index = static_cast<int>(locals.size());
        locals.push_back(function_definition.group_symbol);
    }

    emit(OpCode::OVERLOAD_FUNCTION, physical_index, "fun " + function_definition.name);
}

void Compiler::visit(FunctionDefinition& function_definition)
{
    compile_abstract_function(function_definition);
}

void Compiler::visit(MethodDefinition& method_definition)
{
    compile_abstract_function(method_definition);
}

void Compiler::visit(Return& statement)
{
    if (statement.expression.has_value())
        visit(statement.expression.value());
    else
        emit(OpCode::LOAD_NONE);

    emit(OpCode::RETURN);
}

} // namespace Wasp
