#include "CFGraph.h"
#include "Compiler.h"
#include "Doctor.h"
#include "Expression.h"
#include "OpCode.h"
#include "Statement.h"
#include "Token.h"

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

// -----------------------------------------------------------------------
// Ternary Branch
// -----------------------------------------------------------------------

void Compiler::visit(IfTernaryBranch& expr)
{
    BlockId test_block = graph.create_block();
    BlockId true_block = graph.create_block();
    BlockId false_block = graph.create_block();
    BlockId end_block = graph.create_block();

    graph.add_edge(current_block_id, test_block);
    graph.add_edge(test_block, true_block);
    graph.add_edge(test_block, false_block);
    graph.add_edge(true_block, end_block);
    graph.add_edge(false_block, end_block);

    set_current_block(test_block);
    enter_scope("test");
    visit(expr.test);

    emit(OpCode::JUMP_IF_FALSE, static_cast<int>(false_block));

    set_current_block(true_block);
    enter_scope("true branch");
    visit(expr.true_expression);
    leave_scope_keep_tos("true branch keep TOS");
    leave_scope_keep_tos("test");

    emit(OpCode::JUMP, static_cast<int>(end_block));

    set_current_block(false_block);
    leave_scope("test");
    enter_scope("false branch");

    if (expr.alternative)
    {
        visit(expr.alternative);
    }
    else
    {
        emit(OpCode::LOAD_NONE);
    }

    leave_scope_keep_tos("false branch keep TOS");
    emit(OpCode::JUMP, static_cast<int>(end_block));

    set_current_block(end_block);
}

void Compiler::visit(ElseTernaryBranch& expr)
{
    visit(expr.expression);
}

// -----------------------------------------------------------------------
// Conditional Branch
// -----------------------------------------------------------------------

void Compiler::visit(IfBranch& statement)
{
    BlockId test_block = graph.create_block();
    BlockId true_block = graph.create_block();
    BlockId false_block = graph.create_block();
    BlockId end_block = graph.create_block();

    graph.add_edge(current_block_id, test_block);
    graph.add_edge(test_block, true_block);
    graph.add_edge(test_block, false_block);
    graph.add_edge(true_block, end_block);
    graph.add_edge(false_block, end_block);

    set_current_block(test_block);
    enter_scope("test");
    visit(statement.test);

    emit(OpCode::JUMP_IF_FALSE, static_cast<int>(false_block));

    set_current_block(true_block);
    enter_scope("true branch");
    visit(statement.body);
    leave_scope("true branch");
    leave_scope("test");

    emit(OpCode::JUMP, static_cast<int>(end_block));

    set_current_block(false_block);

    if (statement.alternative.has_value())
    {
        auto& alt_variant = statement.alternative.value()->data;

        if (std::holds_alternative<IfBranch>(alt_variant))
        {
            visit(std::get<IfBranch>(alt_variant));
        }
        else if (std::holds_alternative<ElseBranch>(alt_variant))
        {
            visit(std::get<ElseBranch>(alt_variant));
        }
    }

    emit(OpCode::JUMP, static_cast<int>(end_block));

    set_current_block(end_block);
}

void Compiler::visit(ElseBranch& statement)
{
    enter_scope("else branch");
    visit(statement.body);
    leave_scope("else branch");
}

// ============================================================================
// Loops
// ============================================================================

void Compiler::visit(ForInLoop& statement)
{
    BlockId header = graph.create_block();
    BlockId body = graph.create_block();
    BlockId end = graph.create_block();

    graph.add_edge(current_block_id, header);
    graph.add_edge(header, body);
    graph.add_edge(header, end);
    graph.add_edge(body, header);

    visit(statement.iterable_expression);
    emit(OpCode::GET_ITER);

    set_current_block(header);
    enter_scope("For Loop Header and Body");

    emit(OpCode::LOOP_ITER, static_cast<int>(end));

    Doctor::get().assert(
        statement.lhs->is<Identifier>(),
        WaspStage::Compiler,
        "For-in loop variable must be an identifier."
    );

    auto symbol = statement.lhs->as<Identifier>().symbol;
    Doctor::get().fatal_if_nullptr(symbol, WaspStage::Compiler);
    locals.push_back(symbol);

    emit(OpCode::JUMP, static_cast<int>(body));

    loop_tracking_stack
        .emplace_back(header, body, end, current_lexical_scope_depth, current_lexical_scope_depth);

    set_current_block(body);

    visit(statement.body);
    leave_scope("For Loop Header and Body");
    emit(OpCode::JUMP, static_cast<int>(header));

    loop_tracking_stack.pop_back();

    set_current_block(end);

    // Clean up the iterator object
    emit(OpCode::POP, "remove iterator object");
}

void Compiler::visit(SimpleLoop& statement)
{
    BlockId header = graph.create_block();
    BlockId body = graph.create_block();
    BlockId cleanup_block = graph.create_block();
    BlockId end = graph.create_block();

    emit(OpCode::JUMP, static_cast<int>(header));
    graph.add_edge(current_block_id, header);

    loop_tracking_stack.emplace_back(
        header,
        body,
        end,
        current_lexical_scope_depth,
        current_lexical_scope_depth + 1
    );

    // --- Header ---
    set_current_block(header);
    enter_scope(); // Condition Scope
    visit(statement.condition);

    if (statement.style == TokenType::UNTIL || statement.style == TokenType::UNLESS)
    {
        emit(OpCode::NOT);
    }

    emit(OpCode::JUMP_IF_FALSE, static_cast<int>(cleanup_block));
    emit(OpCode::JUMP, static_cast<int>(body));
    graph.add_edge(header, body);
    graph.add_edge(header, cleanup_block);

    // --- Body ---
    set_current_block(body);
    enter_scope(); // Body scope
    visit(statement.body);
    leave_scope(); // Pop Body scope

    // --- Next Iteration ---
    emit_local_cleanups(current_lexical_scope_depth - 1);

    emit(OpCode::JUMP, static_cast<int>(header));
    graph.add_edge(body, header);

    // --- Cleanup (False path) ---
    set_current_block(cleanup_block);

    leave_scope();

    emit(OpCode::JUMP, static_cast<int>(end));
    graph.add_edge(cleanup_block, end);

    loop_tracking_stack.pop_back();
    set_current_block(end);
}

void Compiler::visit(LoopControl& statement)
{
    Doctor::get()
        .assert(!loop_tracking_stack.empty(), WaspStage::Compiler, "Loop control outside loop");

    auto [header, body, end, entry_depth, body_depth] = loop_tracking_stack.back();

    int target_depth = (statement.type == TokenType::REDO) ? body_depth : entry_depth;

    emit_local_cleanups(target_depth);

    if (statement.type == TokenType::BREAK)
    {
        emit(OpCode::JUMP, static_cast<int>(end));
        graph.add_edge(current_block_id, end);
    }
    else if (statement.type == TokenType::CONTINUE)
    {
        emit(OpCode::JUMP, static_cast<int>(header));
        graph.add_edge(current_block_id, header);
    }
    else if (statement.type == TokenType::REDO)
    {
        emit(OpCode::JUMP, static_cast<int>(body));
        graph.add_edge(current_block_id, body);
    }
}

// ============================================================================
// Other
// ============================================================================

void Compiler::visit(Pass& statement)
{
}

} // namespace Wasp
