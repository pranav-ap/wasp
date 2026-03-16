#include "CFGraph.h"
#include "Compiler.h"
#include "Doctor.h"
#include "Expression.h"
#include "OpCode.h"
#include "Statement.h"
#include "Token.h"


#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp {

// -----------------------------------------------------------------------
// Control Flow
// -----------------------------------------------------------------------

void Compiler::visit(IfTernaryBranch& expr) {
    enter_scope();
    visit(expr.test);

    BlockId true_block = graph.create_block();
    BlockId false_block = graph.create_block();
    BlockId end_block = graph.create_block();

    graph.add_edge(current_block_id, true_block);
    graph.add_edge(current_block_id, false_block);

    emit(OpCode::JUMP_IF_FALSE, static_cast<int>(false_block));
    emit(OpCode::JUMP, static_cast<int>(true_block));

    // --- True Branch ---
    set_current_block(true_block);
    visit(expr.true_expression);
    leave_scope();

    emit(OpCode::JUMP, static_cast<int>(end_block));
    graph.add_edge(true_block, end_block);

    // --- False Branch ---
    set_current_block(false_block);
    leave_scope();

    if (expr.alternative) {
        visit(expr.alternative);
    } else {
        emit(OpCode::LOAD_NONE);
    }

    emit(OpCode::JUMP, static_cast<int>(end_block));
    graph.add_edge(false_block, end_block);

    // --- Converge ---
    set_current_block(end_block);
}

void Compiler::visit(ElseTernaryBranch& expr) {
    enter_scope();
    visit(expr.expression);
    leave_scope();
}

void Compiler::visit(IfBranch& statement) {
    enter_scope();

    visit(statement.test);

    bool has_alternative = statement.alternative.has_value();

    BlockId true_block = graph.create_block();
    BlockId end_block = graph.create_block();
    BlockId false_block = has_alternative ? graph.create_block() : end_block;

    // If no alternative, we need a separate block just to POP the condition scope
    // before ending.
    BlockId cleanup_block = has_alternative ? false_block : graph.create_block();

    graph.add_edge(current_block_id, true_block);
    graph.add_edge(current_block_id, cleanup_block);

    emit(OpCode::JUMP_IF_FALSE, static_cast<int>(cleanup_block));
    emit(OpCode::JUMP, static_cast<int>(true_block));

    // --- True Branch ---
    set_current_block(true_block);
    visit(statement.body);
    leave_scope();

    emit(OpCode::JUMP, static_cast<int>(end_block));
    graph.add_edge(true_block, end_block);

    // --- False Branch / Exit Trampoline ---
    set_current_block(cleanup_block);
    leave_scope();

    if (has_alternative) {
        auto& alt_variant = statement.alternative.value()->data;

        if (std::holds_alternative<IfBranch>(alt_variant)) {
            visit(std::get<IfBranch>(alt_variant));
        } else if (std::holds_alternative<ElseBranch>(alt_variant)) {
            visit(std::get<ElseBranch>(alt_variant));
        }

        emit(OpCode::JUMP, static_cast<int>(end_block));
        graph.add_edge(cleanup_block, end_block);
    } else {
        emit(OpCode::JUMP, static_cast<int>(end_block));
        graph.add_edge(cleanup_block, end_block);
    }

    set_current_block(end_block);
}

void Compiler::visit(ElseBranch& statement) {
    enter_scope();
    visit(statement.body);
    leave_scope();
}

// ============================================================================
// Control Flow: Loops
// ============================================================================

void Compiler::visit(ForInLoop& statement) {
    visit(statement.iterable_expression);

    BlockId header = graph.create_block();
    BlockId body = graph.create_block();
    BlockId end = graph.create_block();

    emit(OpCode::JUMP, static_cast<int>(header));
    graph.add_edge(current_block_id, header);

    // --- Header ---
    set_current_block(header);

    emit(OpCode::LOOP_ITER, static_cast<int>(end));
    emit(OpCode::JUMP, static_cast<int>(body));

    graph.add_edge(header, body);
    graph.add_edge(header, end);

    loop_tracking_stack.emplace_back(
        header, body, end, current_lexical_scope_depth, current_lexical_scope_depth
    );

    // --- Body ---
    set_current_block(body);
    enter_scope();

    if (statement.lhs->is<Identifier>()) {
        auto symbol = statement.lhs->as<Identifier>().symbol;
        if (symbol) {
            debug_name_map[symbol->id] = symbol->name;
            emit(OpCode::DEFINE_LOCAL, symbol->id);
        }
    } else {
        Doctor::get().fatal(WaspStage::Compiler, "For-in loop LHS must be a simple Identifier");
    }

    visit(statement.body);
    leave_scope();

    // Loop Back
    emit(OpCode::JUMP, static_cast<int>(header));
    graph.add_edge(body, header);

    // End
    loop_tracking_stack.pop_back();
    set_current_block(end);
    // Clean up the iterator object
    emit(OpCode::POP);
}

void Compiler::visit(SimpleLoop& statement) {
    BlockId header = graph.create_block();
    BlockId body = graph.create_block();
    BlockId cleanup_block = graph.create_block();
    BlockId end = graph.create_block();

    emit(OpCode::JUMP, static_cast<int>(header));

    graph.add_edge(current_block_id, header);
    loop_tracking_stack.emplace_back(
        header, body, end, current_lexical_scope_depth, current_lexical_scope_depth + 1
    );

    // --- Header ---
    set_current_block(header);
    enter_scope();
    visit(statement.condition);

    if (statement.style == TokenType::UNTIL || statement.style == TokenType::UNLESS) {
        emit(OpCode::NOT);
    }

    graph.add_edge(header, body);
    graph.add_edge(header, cleanup_block);
    emit(OpCode::JUMP_IF_FALSE, static_cast<int>(cleanup_block));
    emit(OpCode::JUMP, static_cast<int>(body));

    // --- Body ---
    set_current_block(body);
    enter_scope();
    visit(statement.body);
    leave_scope();

    // Next Iteration
    leave_scope();
    emit(OpCode::JUMP, static_cast<int>(header));
    graph.add_edge(body, header);

    // Cleanup
    set_current_block(cleanup_block);
    emit(OpCode::POP_SCOPE);

    emit(OpCode::JUMP, static_cast<int>(end));
    graph.add_edge(cleanup_block, end);

    loop_tracking_stack.pop_back();
    set_current_block(end);
}

void Compiler::visit(LoopControl& statement) {
    Doctor::get().assert(
        !loop_tracking_stack.empty(), WaspStage::Compiler, "Loop control outside loop"
    );

    auto [header, body, end, entry_depth, body_depth] = loop_tracking_stack.back();

    int target_depth;

    if (statement.type == TokenType::REDO) {
        // Unwind to the exact state before the body block's internal scope is
        // pushed
        target_depth = body_depth;
    } else {
        // Break and Continue unwind completely to the loop's entry state
        target_depth = entry_depth;
    }

    int scopes_to_pop = current_lexical_scope_depth - target_depth;
    Doctor::get().assert(
        scopes_to_pop >= 0,
        WaspStage::Compiler,
        "Compiler Error: Current lexical scope depth is less than loop entry depth!"
    );

    for (int i = 0; i < scopes_to_pop; ++i) {
        emit(OpCode::POP_SCOPE);
    }

    if (statement.type == TokenType::BREAK) {
        emit(OpCode::JUMP, static_cast<int>(end));
        graph.add_edge(current_block_id, end);
    } else if (statement.type == TokenType::CONTINUE) {
        emit(OpCode::JUMP, static_cast<int>(header));
        graph.add_edge(current_block_id, header);
    } else if (statement.type == TokenType::REDO) {
        emit(OpCode::JUMP, static_cast<int>(body));
        graph.add_edge(current_block_id, body);
    }
}

void Compiler::visit(Pass& statement) {}

} // namespace Wasp
