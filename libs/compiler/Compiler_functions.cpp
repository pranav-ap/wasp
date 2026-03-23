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

template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp {

// -----------------------------------------------------------------------
// Function Definition
// ------------------------------------------------------------------------

void Compiler::visit(FunctionDefinition& function_definition) {
    Compiler func_compiler(this);

    func_compiler.enter_scope();
    func_compiler.visit(function_definition.body);
    func_compiler.leave_scope();

    func_compiler.emit(OpCode::LOAD_NONE);
    func_compiler.emit(OpCode::RETURN);

    CodeObject code = func_compiler.flatten();

    int const_id = pool->allocate_function_definition(
        std::move(code),
        function_definition.name,
        func_compiler.id_to_name_map,
        func_compiler.id_to_upvalue_name_map);

    emit(OpCode::LOAD_CONST, const_id);

    int upvalue_count = static_cast<int>(func_compiler.upvalues.size());
    emit(OpCode::MAKE_FUNCTION, upvalue_count);

    for (const auto& uv : func_compiler.upvalues) {
        emit_raw_byte(uv.is_local ? std::byte{1} : std::byte{0});
        emit_raw_byte(static_cast<std::byte>(uv.index));
    }

    id_to_name_map[function_definition.symbol->id] = function_definition.name;
    emit(OpCode::DEFINE_LOCAL, function_definition.symbol->id);
}

void Compiler::visit(Return& statement) {
    if (statement.expression.has_value())
        visit(statement.expression.value());
    else
        emit(OpCode::LOAD_NONE);

    emit(OpCode::RETURN);
}

} // namespace Wasp
