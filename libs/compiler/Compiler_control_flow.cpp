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
    leave_scope("true branch");
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
    enter_scope(); // Condition Scope

    visit(statement.test);

    bool has_alternative = statement.alternative.has_value();

    BlockId true_block = graph.create_block();
    BlockId false_block = graph.create_block();
    BlockId end_block = graph.create_block();

    emit(OpCode::JUMP_IF_FALSE, static_cast<int>(false_block));
    emit(OpCode::JUMP, static_cast<int>(true_block));
    graph.add_edge(current_block_id, true_block);
    graph.add_edge(current_block_id, false_block);

    // --- True Branch ---
    set_current_block(true_block);
    enter_scope(); // Body scope
    visit(statement.body);
    leave_scope(); // Pop Body scope

    emit(OpCode::JUMP, static_cast<int>(end_block));
    graph.add_edge(true_block, end_block);

    // --- False Branch ---
    set_current_block(false_block);
    if (has_alternative)
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
    graph.add_edge(false_block, end_block);

    // --- Converge ---
    set_current_block(end_block);

    leave_scope();
}

void Compiler::visit(ElseBranch& statement)
{
    enter_scope();
    visit(statement.body);
    leave_scope();
}

// ============================================================================
// Control Flow: Loops
// ============================================================================

void Compiler::visit(ForInLoop& statement)
{
    visit(statement.iterable_expression);
    emit(OpCode::GET_ITER);

    BlockId header = graph.create_block();
    BlockId body = graph.create_block();
    BlockId end = graph.create_block();

    emit(OpCode::JUMP, static_cast<int>(header));
    graph.add_edge(current_block_id, header);

    // --- Header ---
    set_current_block(header);
    enter_scope();

    emit(OpCode::LOOP_ITER, static_cast<int>(end));
    emit(OpCode::JUMP, static_cast<int>(body));

    graph.add_edge(header, body);
    graph.add_edge(header, end);

    loop_tracking_stack
        .emplace_back(header, body, end, current_lexical_scope_depth, current_lexical_scope_depth);

    // --- Body ---
    set_current_block(body);

    if (statement.lhs->is<Identifier>())
    {
        auto symbol = statement.lhs->as<Identifier>().symbol;
        if (symbol)
        {
            locals.push_back(symbol);
        }
    }
    else
    {
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

void Compiler::visit(Pass& statement)
{
}

} // namespace Wasp
