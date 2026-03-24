#include "CFGraph.h"
#include "Compiler.h"
#include "OpCode.h"
#include "Statement.h"

#include <cstddef>
#include <map>
#include <optional>
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

// -----------------------------------------------------------------------
// Function Definition
// ------------------------------------------------------------------------

void Compiler::visit(FunctionDefinition& function_definition)
{
    Compiler func_compiler(this);

    func_compiler.enter_scope();

    std::vector<int> func_compiler_parameter_symbol_ids;
    for (const auto& param_symbol : function_definition.parameter_symbols)
    {
        func_compiler_parameter_symbol_ids.push_back(param_symbol->id);
        func_compiler.symbol_id_to_name_map[param_symbol->id] = param_symbol->name;
    }

    func_compiler.visit(function_definition.body);
    func_compiler.leave_scope();

    func_compiler.emit(OpCode::LOAD_NONE);
    func_compiler.emit(OpCode::RETURN);

    CodeObject code = func_compiler.flatten();

    // Store the code in the constant pool
    int const_id = workspace->pool->allocate_function_definition(
        std::move(code),
        std::move(func_compiler_parameter_symbol_ids),
        function_definition.name,
        std::move(func_compiler.symbol_id_to_name_map),
        std::move(func_compiler.upvalue_index_to_name_map));

    emit(OpCode::LOAD_CONST, const_id);

    // Make the function & capture upvalues

    int upvalue_count = static_cast<int>(func_compiler.upvalues.size());
    emit(OpCode::MAKE_FUNCTION, upvalue_count);

    for (const auto& uv : func_compiler.upvalues)
    {
        if (uv.is_local_to_parent)
        {
            emit_raw_byte(std::byte{1});
            emit_raw_byte(static_cast<std::byte>(uv.symbol_id));
        }
        else
        {
            emit_raw_byte(std::byte{0});
            emit_raw_byte(static_cast<std::byte>(uv.upvalue_index_in_parent));
        }
    }

    symbol_id_to_name_map[function_definition.symbol->id] = function_definition.name;
    emit(OpCode::DEFINE_LOCAL, function_definition.symbol->id);
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
