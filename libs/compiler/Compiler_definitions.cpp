#include "AST.h"
#include "CFGraph.h"
#include "Compiler.h"
#include "Doctor.h"
#include "Objects.h"
#include "OpCode.h"
#include "Statement.h"
#include "Workspace.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Wasp
{

void Compiler::emit_closure_upvalues(const std::vector<Upvalue>& upvalues)
{
    emit(OpCode::MAKE_FUNCTION, static_cast<int>(upvalues.size()));

    for (const auto& uv : upvalues)
    {
        emit_raw_byte(uv.is_local_to_parent ? std::byte{1} : std::byte{0});
        emit_raw_byte(static_cast<std::byte>(uv.index));
    }
}

void Compiler::compile_function_closure(
    const std::string& name,
    const std::vector<Symbol_ptr>& parameters,
    StatementVector body
)
{
    Compiler func_compiler(this);

    func_compiler.enter_scope();

    for (const auto& param_symbol : parameters)
    {
        func_compiler.stack.push_back(param_symbol);
    }

    func_compiler.visit(body);
    func_compiler.leave_scope();

    func_compiler.emit(OpCode::LOAD_NONE);
    func_compiler.emit(OpCode::RETURN);

    CodeObject code = func_compiler.flatten();

    int const_id = workspace->pool->allocate_function_definition(std::move(code), name);
    emit(OpCode::LOAD_CONST, const_id, "fun " + name);

    emit_closure_upvalues(func_compiler.upvalues);
}

void Compiler::compile_method_group(
    ClassDefinition& class_definition,
    const std::string& original_name
)
{
    int overload_count = 0;
    std::string mangled_name = class_definition.name + "::" + original_name;

    for (auto& stmt : class_definition.members)
    {
        if (std::holds_alternative<MethodDefinition>(stmt->data))
        {
            auto& method = std::get<MethodDefinition>(stmt->data);

            if (method.name == mangled_name)
            {
                compile_function_closure(method.name, method.parameter_symbols, method.body);
                overload_count++;
            }
        }
    }

    emit(OpCode::BUILD_OVERLOAD_GROUP, overload_count, "overloads for " + original_name);
}

void Compiler::visit(ClassDefinition& class_definition)
{
    auto class_type = class_definition.symbol->get_type()->as<std::shared_ptr<ClassType>>();
    int unique_method_count = 0;

    for (const std::string& original_name : class_type->methods)
    {
        compile_method_group(class_definition, original_name);
        unique_method_count++;
    }

    emit(OpCode::BUILD_CLASS, unique_method_count, "build class " + class_definition.name);

    int physical_index = get_or_add_local_index(class_definition.symbol);
    emit(OpCode::SET_LOCAL, physical_index, "class " + class_definition.name);
}

void Compiler::visit(FunctionDefinition& function_definition)
{
    compile_function_closure(
        function_definition.name,
        function_definition.parameter_symbols,
        function_definition.body
    );

    int physical_index = get_or_add_local_index(function_definition.group_symbol);
    emit(OpCode::STORE_FUNCTION_OVERLOAD, physical_index, "fun " + function_definition.name);
}

void Compiler::visit(MethodDefinition& method_definition)
{
    Doctor::get().fatal(
        WaspStage::Compiler,
        "Method definitions should not be visited directly during compilation. They should be "
        "compiled as part of their containing class."
    );
}

void Compiler::visit(Return& statement)
{
    if (statement.expression.has_value())
    {
        visit(statement.expression.value());
    }
    else
    {
        emit(OpCode::LOAD_NONE);
    }

    emit(OpCode::RETURN);
}

} // namespace Wasp
