#include "CFGraph.h"
#include "Compiler.h"
#include "ConstantPool.h"
#include "Expression.h"
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

void Compiler::visit(FunctionDefinition& statement) {
    Compiler func_compiler(pool, this);

    func_compiler.enter_scope();
    func_compiler.visit(statement.body);
    func_compiler.leave_scope();

    func_compiler.emit(OpCode::LOAD_NONE);
    func_compiler.emit(OpCode::RETURN);

    CodeObject func_code = func_compiler.flatten();
    func_code.name = statement.name;
    func_code.local_names = std::move(func_compiler.debug_name_map);

    int const_id = pool->allocate_function_definition(std::move(func_code));
    emit(OpCode::LOAD_CONST, const_id);

    int upvalue_count = static_cast<int>(func_compiler.upvalues.size());
    emit(OpCode::MAKE_FUNCTION, upvalue_count);

    for (const auto& uv : func_compiler.upvalues) {
        emit_raw_byte(uv.is_local ? std::byte{1} : std::byte{0});
        emit_raw_byte(static_cast<std::byte>(uv.index));
    }

    debug_name_map[statement.symbol->id] = statement.name;
    emit(OpCode::DEFINE_LOCAL, statement.symbol->id);
}

void Compiler::visit(Return& statement) {
    if (statement.expression.has_value())
        visit(statement.expression.value());
    else
        emit(OpCode::LOAD_NONE);

    emit(OpCode::RETURN);
}

void Compiler::visit(Call& expr) {
    visit(expr.callable);

    for (const auto& arg : expr.arguments)
        visit(arg);

    emit(OpCode::CALL, static_cast<int>(expr.arguments.size()));
}

} // namespace Wasp
