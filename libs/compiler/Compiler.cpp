#include "Compiler.h"
#include "CFGraph.h"
#include "ConstantPool.h"
#include "Doctor.h"
#include "Expression.h"
#include "NativeRegistry.h"
#include "OpCode.h"
#include "Statement.h"

#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp {
Compiler::Compiler(ConstantPool_ptr pool, NativeRegistry_ptr native_registry)
    : pool(pool), current_block_id(InvalidBlockId), parent(nullptr), compiler_depth(0),
      native_registry(native_registry) {
    current_block_id = graph.create_block();
    graph.set_entry_block(current_block_id);
}

Compiler::Compiler(ConstantPool_ptr pool, Compiler* parent)
    : pool(std::move(pool)), parent(parent), compiler_depth(parent->compiler_depth + 1) {
    native_registry = parent->native_registry;
    current_block_id = graph.create_block();
    graph.set_entry_block(current_block_id);
}

CodeObject Compiler::run(const StatementVector& block, bool is_main) {
    if (is_main) {
        emit(OpCode::ENTER_WORKSPACE);
    }

    emit(OpCode::ENTER_MODULE);

    for (const auto& s : block) {
        visit(s);
    }

    BlockId exit = graph.create_block();
    graph.add_edge(current_block_id, exit);
    emit(OpCode::JUMP, static_cast<int>(exit));
    set_current_block(exit);

    emit(OpCode::EXIT_MODULE);

    if (is_main) {
        emit(OpCode::EXIT_WORKSPACE);
    }

    CodeObject final_code = flatten();
    final_code.local_names = std::move(this->debug_name_map);
    // final_code.name = "module";

    return final_code;
}

// ========================================================================
// Visitors
// ========================================================================

void Compiler::visit(std::vector<Statement_ptr>& statements) {
    for (const auto& stmt : statements)
        visit(stmt);
}

void Compiler::visit(const Statement_ptr statement) {
    Doctor::get().fatal_if_nullptr(statement, WaspStage::Compiler);

    std::visit(
        overloaded{
            [&](ExpressionStatement& stat) { visit(stat); },
            [&](VariableDefinition& stat) { visit(stat); },
            [&](IfBranch& stat) { visit(stat); },
            [&](ElseBranch& stat) { visit(stat); },
            [&](SimpleLoop& stat) { visit(stat); },
            [&](ForInLoop& stat) { visit(stat); },
            [&](Pass& stat) { visit(stat); },
            [&](LoopControl& stat) { visit(stat); },
            [&](FunctionDefinition& stat) { visit(stat); },
            [&](Return& stat) { visit(stat); },
            [&](SimpleImport& stat) { visit(stat); },
            [&](FromImport& stat) { visit(stat); },
            [](auto) { Doctor::get().fatal(WaspStage::Compiler, "Unknown Statement"); }
        },
        statement->data
    );
}

void Compiler::visit(ExpressionStatement& statement) {
    visit(statement.expression);
    emit(OpCode::POP);
}

} // namespace Wasp
