#include "Compiler.h"
#include "AST.h"
#include "CFGraph.h"
#include "OpCode.h"
#include "Statement.h"
#include "Workspace.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void Compiler::visit(FunctionDefinition& def)
{
    int physical_index = resolve_local(def.group_symbol->id);

    if (physical_index == -1)
    {
        physical_index = get_or_add_local_index(def.group_symbol);

        emit(OpCode::BUILD_OVERLOAD_GROUP, 0);
        emit(OpCode::SET_LOCAL, physical_index);
    }

    if (!def.template_params.empty())
    {
        return;
    }

    if (def.symbol->is_native())
    {
        std::string mangled = mangle_name(def.name, "", def.symbol->module_path);
        int registry_id = workspace->native_registry->get_native_index(mangled);
        emit(OpCode::GET_NATIVE, registry_id, mangled);
    }
    else
    {
        compile_function_closure(def.name, def.parameter_symbols, def.body, nullptr);
    }

    std::string label = (def.is_pure ? "pure fun " : "fun ") + def.name;
    emit(OpCode::STORE_FUNCTION_OVERLOAD, physical_index, label);
}

void Compiler::compile_function_closure(
    const std::string& name,
    const std::vector<Symbol_ptr>& parameters,
    StatementVector body,
    Symbol_ptr context_symbol
)
{
    Compiler func_compiler(this);

    func_compiler.enter_scope();

    if (context_symbol)
    {
        func_compiler.stack.push_back(context_symbol);
    }

    for (const auto& param_symbol : parameters)
    {
        func_compiler.stack.push_back(param_symbol);
    }

    func_compiler.visit(body);
    func_compiler.leave_scope();

    func_compiler.emit(OpCode::LOAD_NONE);
    func_compiler.emit(OpCode::RETURN);

    CodeObject code = func_compiler.flatten();

    int const_id = workspace->pool->allocate_function_definition(
        std::move(code),
        name
    );

    emit(OpCode::LOAD_CONSTANT, const_id, "fun " + name);
    emit(OpCode::BUILD_FUNCTION, static_cast<int>(func_compiler.upvalues.size()));

    for (const auto& uv : func_compiler.upvalues)
    {
        emit_raw_byte(uv.is_local_to_parent ? std::byte{1} : std::byte{0});
        emit_raw_byte(static_cast<std::byte>(uv.index));
    }
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
