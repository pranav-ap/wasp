#include "Compiler.h"
#include "Objects.h"
#include "OpCode.h"
#include "Statement.h"
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void Compiler::compile_abstract_function(AbstractFunctionDefinition& function_definition)
{
    Compiler func_compiler(this);

    func_compiler.enter_scope();

    // The Semantic Analyzer already injected 'my' and 'our' in here for methods!
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

    // Push the FunctionBlueprintObject onto the stack
    emit(OpCode::LOAD_CONST, const_id, "fun " + function_definition.name);

    // Transform it into a FunctionRuntimeObject & capture upvalues
    int upvalue_count = static_cast<int>(func_compiler.upvalues.size());
    emit(OpCode::MAKE_FUNCTION, upvalue_count);

    // Emit closure routing bytes for the VM
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

// -------------------------------------------------------------------------
// Route all three AST nodes to the exact same compiler logic
// -------------------------------------------------------------------------

void Compiler::visit(LocalFunctionDefinition& function_definition)
{
    compile_abstract_function(function_definition);
}

void Compiler::visit(MyMethodDefinition& method_definition)
{
    compile_abstract_function(method_definition);
}

void Compiler::visit(OurMethodDefinition& method_definition)
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

void Compiler::visit(ClassDefinition& class_definition)
{
    // 1. Compile the methods body bytecode first!
    for (auto& stmt : class_definition.members)
    {
        std::visit(
            overloaded{
                [&](MyMethodDefinition& method)
                {
                    visit(method);
                },
                [&](OurMethodDefinition& method)
                {
                    visit(method);
                },
                [&](auto&)
                {
                    // Fields are handled dynamically at instantiation,
                    // no bytecode body to compile here.
                }
            },
            stmt->data
        );
    }

    // 2. Load and store the Class Blueprint
    Object_ptr class_blueprint = class_definition.symbol->get_type();

    int const_id = workspace->pool->allocate(class_blueprint);
    emit(OpCode::LOAD_CONST, const_id, "class " + class_definition.name);

    int physical_index = resolve_local(class_definition.symbol->id);

    if (physical_index == -1)
    {
        physical_index = static_cast<int>(locals.size());
        locals.push_back(class_definition.symbol);
    }

    emit(OpCode::SET_LOCAL, physical_index, "class " + class_definition.name);
}

} // namespace Wasp
