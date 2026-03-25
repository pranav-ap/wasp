#include "CFGraph.h"
#include "Compiler.h"
#include "Objects.h"
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
        StringVector{},
        function_definition.name
    );

    // Push the StaticFunctionObject onto the stack
    emit(OpCode::LOAD_CONST, const_id);

    // Transform it into a RuntimeFunctionObject & capture upvalues
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

    emit(OpCode::OVERLOAD_FUNCTION, function_definition.group_symbol->id);
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
