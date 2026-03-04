#include "Compiler.h"
#include "InstructionPrinter.h"
#include <stdexcept>
#include <utility>
#include <memory>
#include <variant>
#include <optional>

#ifndef ASSERT
#include <cassert>
#define ASSERT(condition, message) assert((condition) && message)
#endif

#define FATAL(message) throw std::runtime_error(message)
#define NULL_CHECK(x) ASSERT(x != nullptr, "Oh shit! A nullptr")

template <class... Ts>
struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{
    // ------------------------------------------------------------------------
    // Constructors
    // ------------------------------------------------------------------------

    Compiler::Compiler()
        : constant_pool(std::make_shared<ConstantPool>()),
          current_block_id(InvalidBlockId),
          parent(nullptr),
          compiler_depth(0)
    {
        current_block_id = graph.create_block();
        graph.set_entry_block(current_block_id);
    }

    Compiler::Compiler(ConstantPool_ptr pool, Compiler *parent)
        : constant_pool(std::move(pool)),
          parent(parent),
          compiler_depth(parent->compiler_depth + 1)
    {
        current_block_id = graph.create_block();
        graph.set_entry_block(current_block_id);
    }

    // ------------------------------------------------------------------------
    // Scope
    // ------------------------------------------------------------------------

    void Compiler::enter_scope()
    {
        emit(OpCode::PUSH_SCOPE);
        current_lexical_scope_depth++;
    }

    void Compiler::leave_scope()
    {
        emit(OpCode::POP_SCOPE);
        current_lexical_scope_depth--;
    }

    // ------------------------------------------------------------------------
    // Closure Support
    // ------------------------------------------------------------------------

    int Compiler::add_upvalue(int index, bool is_local)
    {
        for (int i = 0; i < static_cast<int>(upvalues.size()); i++)
        {
            if (upvalues[i].index == index && upvalues[i].is_local == is_local)
            {
                return i;
            }
        }

        upvalues.push_back({index, is_local});
        return static_cast<int>(upvalues.size()) - 1;
    }

    int Compiler::resolve_upvalue(Compiler *current_compiler, Symbol_ptr symbol)
    {
        if (current_compiler->parent == nullptr)
        {
            FATAL("Compiler Error: Reached global scope while resolving upvalue.");
        }

        if (symbol->closure_depth == current_compiler->parent->compiler_depth)
        {
            return current_compiler->add_upvalue(symbol->id, true);
        }

        int upvalue_index_in_parent = resolve_upvalue(current_compiler->parent, symbol);
        return current_compiler->add_upvalue(upvalue_index_in_parent, false);
    }

    // -----------------------------------------------------------------------
    // Emit
    // ----------------------------------------------------------------------

    void Compiler::emit(OpCode opcode) { graph.get_block(current_block_id).get_code().emit(opcode); }

    void Compiler::emit(OpCode opcode, int operand) { graph.get_block(current_block_id).get_code().emit(opcode, operand); }

    void Compiler::emit(OpCode opcode, int operand_1, int operand_2) { graph.get_block(current_block_id).get_code().emit(opcode, operand_1, operand_2); }

    void Compiler::emit_raw_byte(std::byte b)
    {
        ByteVector bv = {b};
        graph.get_block(current_block_id).get_code().push(bv);
    }

    // ========================================================================
    // Statement Visitors
    // ========================================================================

    void Compiler::visit(std::vector<Statement_ptr> &statements)
    {
        for (const auto &stmt : statements)
            visit(stmt);
    }

    void Compiler::visit(const Statement_ptr statement)
    {
        NULL_CHECK(statement);
        std::visit(overloaded{[&](ExpressionStatement &stat)
                              { visit(stat); },
                              [&](VariableDefinition &stat)
                              { visit(stat); },
                              [&](IfBranch &stat)
                              { visit(stat); },
                              [&](ElseBranch &stat)
                              { visit(stat); },
                              [&](SimpleLoop &stat)
                              { visit(stat); },
                              [&](ForInLoop &stat)
                              { visit(stat); },
                              [&](Pass &stat)
                              { visit(stat); },
                              [&](LoopControl &stat)
                              { visit(stat); },
                              [&](FunctionDefinition &stat)
                              { visit(stat); },
                              [&](Return &stat)
                              { visit(stat); },
                              [](auto)
                              { FATAL("Unknown Statement"); }},
                   statement->data);
    }

    void Compiler::visit(ExpressionStatement &statement)
    {
        visit(statement.expression);
        emit(OpCode::POP);
    }

    // -----------------------------------------------------------------------
    // Variable Definition
    // -----------------------------------------------------------------------

    void Compiler::visit(VariableDefinition &statement)
    {
        compile_variable_definition(statement.expression, false);
    }

    void Compiler::visit(VariableDefinitionExpression &expr)
    {
        compile_variable_definition(expr.assignment, true);
    }

    void Compiler::compile_variable_definition(const Expression_ptr &assignment, bool as_expression)
    {
        NULL_CHECK(assignment);

        Expression_ptr lhs = nullptr;
        Expression_ptr rhs = nullptr;

        if (assignment->is<UntypedAssignment>())
        {
            const auto &assign = assignment->as<UntypedAssignment>();
            lhs = assign.lhs_expression;
            rhs = assign.rhs_expression;
        }
        else if (assignment->is<TypedAssignment>())
        {
            const auto &assign = assignment->as<TypedAssignment>();
            lhs = assign.lhs_expression;
            rhs = assign.rhs_expression;
        }
        else
        {
            FATAL("Invalid definition assignment type.");
        }

        visit(rhs);

        if (as_expression)
        {
            emit(OpCode::DUP);
        }

        if (!lhs->is<Identifier>())
        {
            FATAL("Left-hand side of definition must be an Identifier.");
        }

        auto symbol = lhs->as<Identifier>().resolution;

        if (!symbol)
        {
            FATAL("Compiler Error: Unresolved definition");
        }

        debug_name_map[symbol->id] = symbol->name;

        emit(OpCode::DEFINE_LOCAL, symbol->id);
    }

    // -----------------------------------------------------------------------
    // Control Flow
    // -----------------------------------------------------------------------

    void Compiler::visit(IfTernaryBranch &expr)
    {
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

        if (expr.alternative)
        {
            visit(expr.alternative);
        }
        else
        {
            emit(OpCode::LOAD_NONE);
        }

        emit(OpCode::JUMP, static_cast<int>(end_block));
        graph.add_edge(false_block, end_block);

        // --- Converge ---
        set_current_block(end_block);
    }

    void Compiler::visit(ElseTernaryBranch &expr)
    {
        enter_scope();
        visit(expr.expression);
        leave_scope();
    }

    void Compiler::visit(IfBranch &statement)
    {
        enter_scope();

        visit(statement.test);

        bool has_alternative = statement.alternative.has_value();

        BlockId true_block = graph.create_block();
        BlockId end_block = graph.create_block();
        BlockId false_block = has_alternative ? graph.create_block() : end_block;

        // If no alternative, we need a separate block just to POP the condition scope before ending.
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

        if (has_alternative)
        {
            auto &alt_variant = statement.alternative.value()->data;

            if (std::holds_alternative<IfBranch>(alt_variant))
            {
                visit(std::get<IfBranch>(alt_variant));
            }
            else if (std::holds_alternative<ElseBranch>(alt_variant))
            {
                visit(std::get<ElseBranch>(alt_variant));
            }

            emit(OpCode::JUMP, static_cast<int>(end_block));
            graph.add_edge(cleanup_block, end_block);
        }
        else
        {
            emit(OpCode::JUMP, static_cast<int>(end_block));
            graph.add_edge(cleanup_block, end_block);
        }

        set_current_block(end_block);
    }

    void Compiler::visit(ElseBranch &statement)
    {
        enter_scope();
        visit(statement.body);
        leave_scope();
    }

    // ============================================================================
    // Control Flow: Loops
    // ============================================================================

    void Compiler::visit(ForInLoop &statement)
    {
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

        loop_tracking_stack.emplace_back(header, body, end, current_lexical_scope_depth, current_lexical_scope_depth);

        // --- Body ---
        set_current_block(body);
        enter_scope();

        if (statement.lhs->is<Identifier>())
        {
            auto symbol = statement.lhs->as<Identifier>().resolution;
            if (symbol)
            {
                debug_name_map[symbol->id] = symbol->name;
                emit(OpCode::DEFINE_LOCAL, symbol->id);
            }
        }
        else
        {
            FATAL("For-in loop LHS must be a simple Identifier.");
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

    void Compiler::visit(SimpleLoop &statement)
    {
        BlockId header = graph.create_block();
        BlockId body = graph.create_block();
        BlockId cleanup_block = graph.create_block();
        BlockId end = graph.create_block();

        emit(OpCode::JUMP, static_cast<int>(header));

        graph.add_edge(current_block_id, header);
        loop_tracking_stack.emplace_back(header, body, end, current_lexical_scope_depth, current_lexical_scope_depth + 1);

        // --- Header ---
        set_current_block(header);
        enter_scope();
        visit(statement.condition);

        if (statement.style == TokenType::UNTIL || statement.style == TokenType::UNLESS)
        {
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

    void Compiler::visit(LoopControl &statement)
    {
        if (loop_tracking_stack.empty())
        {
            FATAL("Loop control outside loop");
        }

        auto [header, body, end, entry_depth, body_depth] = loop_tracking_stack.back();

        int target_depth;

        if (statement.type == TokenType::REDO)
        {
            // Unwind to the exact state before the body block's internal scope is pushed
            target_depth = body_depth;
        }
        else
        {
            // Break and Continue unwind completely to the loop's entry state
            target_depth = entry_depth;
        }

        int scopes_to_pop = current_lexical_scope_depth - target_depth;
        ASSERT(scopes_to_pop >= 0, "Compiler Error: Current lexical scope depth is less than loop entry depth!");

        for (int i = 0; i < scopes_to_pop; ++i)
        {
            emit(OpCode::POP_SCOPE);
        }

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

    void Compiler::visit(Pass &statement) {}

    // -----------------------------------------------------------------------
    // Function Definition
    // ------------------------------------------------------------------------

    void Compiler::visit(FunctionDefinition &statement)
    {
        Compiler func_compiler(constant_pool, this);

        func_compiler.enter_scope();
        func_compiler.visit(statement.body);
        func_compiler.leave_scope();

        func_compiler.emit(OpCode::LOAD_NONE);
        func_compiler.emit(OpCode::RETURN);

        CodeObject func_code = func_compiler.flatten();
        func_code.name = statement.name;
        func_code.local_names = std::move(func_compiler.debug_name_map);

        int const_id = constant_pool->allocate_function_definition(std::move(func_code));
        emit(OpCode::LOAD_CONST, const_id);

        int upvalue_count = static_cast<int>(func_compiler.upvalues.size());
        emit(OpCode::MAKE_FUNCTION, upvalue_count);

        for (const auto &uv : func_compiler.upvalues)
        {
            emit_raw_byte(uv.is_local ? std::byte{1} : std::byte{0});
            emit_raw_byte(static_cast<std::byte>(uv.index));
        }

        if (statement.resolution)
        {
            debug_name_map[statement.resolution->id] = statement.name;
            emit(OpCode::DEFINE_LOCAL, statement.resolution->id);
        }
    }

    void Compiler::visit(Return &statement)
    {
        if (statement.expression.has_value())
            visit(statement.expression.value());
        else
            emit(OpCode::LOAD_NONE);

        emit(OpCode::RETURN);
    }

    // -----------------------------------------------------------------------
    // Expressions
    // -----------------------------------------------------------------------

    void Compiler::visit(std::vector<Expression_ptr> &expressions)
    {
        for (const auto &expr : expressions)
        {
            visit(expr);
        }
    }

    void Compiler::visit(const Expression_ptr expr)
    {
        NULL_CHECK(expr);
        std::visit(overloaded{[&](int val)
                              { visit(val); },
                              [&](double val)
                              { visit(val); },
                              [&](std::string val)
                              { visit(val); },
                              [&](bool val)
                              { visit(val); },
                              [&](Identifier &id)
                              { visit(id); },
                              [&](UntypedAssignment &a)
                              { visit(a); },
                              [&](TypedAssignment &a)
                              { visit(a); },
                              [&](Prefix &p)
                              { visit(p); },
                              [&](Infix &i)
                              { visit(i); },
                              [&](Call &c)
                              { visit(c); },
                              [&](ListLiteral &l)
                              { visit(l); },
                              [&](TupleLiteral &t)
                              { visit(t); },
                              [&](MapLiteral &m)
                              { visit(m); },
                              [&](SetLiteral &s)
                              { visit(s); },
                              [&](RangeLiteral &r)
                              { visit(r); },
                              [&](VariableDefinitionExpression &v)
                              { visit(v); },
                              [&](IfTernaryBranch &i)
                              { visit(i); },
                              [&](ElseTernaryBranch &e)
                              { visit(e); },
                              [&](auto &) { /* Fallback */ }},
                   expr->data);
    }

    void Compiler::visit(int expr) { emit(OpCode::LOAD_CONST, constant_pool->allocate(expr)); }

    void Compiler::visit(double expr) { emit(OpCode::LOAD_CONST, constant_pool->allocate(expr)); }

    void Compiler::visit(std::string expr) { emit(OpCode::LOAD_CONST, constant_pool->allocate(expr)); }

    void Compiler::visit(bool expr) { emit(expr ? OpCode::LOAD_TRUE : OpCode::LOAD_FALSE); }

    void Compiler::visit(Identifier &expr)
    {
        auto symbol = expr.resolution;

        if (!symbol)
        {
            FATAL("Unresolved identifier");
        }

        if (symbol->lexical_depth == 0)
        {
            emit(OpCode::GET_GLOBAL, symbol->id);
        }
        else if (symbol->is_captured)
        {
            int upval_index = resolve_upvalue(this, symbol);
            emit(OpCode::GET_UPVALUE, upval_index);
        }
        else
        {
            emit(OpCode::GET_LOCAL, symbol->id);
        }
    }

    void Compiler::compile_assignment(const Expression_ptr &lhs, const Expression_ptr &rhs)
    {
        visit(rhs);

        if (!lhs->is<Identifier>())
            FATAL("Only ID assignment supported");

        auto symbol = lhs->as<Identifier>().resolution;
        if (!symbol)
            FATAL("Unresolved assignment");

        if (symbol->is_captured)
        {
            int idx = resolve_upvalue(this, symbol);
            emit(OpCode::SET_UPVALUE, idx);
        }
        else if (symbol->closure_depth == 0)
        {
            emit(OpCode::SET_GLOBAL, symbol->id);
        }
        else
        {
            emit(OpCode::SET_LOCAL, symbol->id);
        }
    }

    void Compiler::visit(UntypedAssignment &expr)
    {
        compile_assignment(expr.lhs_expression, expr.rhs_expression);
    }

    void Compiler::visit(TypedAssignment &expr)
    {
        // do nothing
    }

    void Compiler::visit(Prefix &expr)
    {
        visit(expr.operand);

        switch (expr.op.type)
        {
        case TokenType::MINUS:
            emit(OpCode::NEGATE);
            break;
        case TokenType::NOT:
            emit(OpCode::NOT);
            break;
        default:
            break;
        }
    }

    void Compiler::visit(Infix &expr)
    {
        visit(expr.left);
        visit(expr.right);

        switch (expr.op.type)
        {
        case TokenType::PLUS:
            emit(OpCode::ADD);
            break;
        case TokenType::MINUS:
            emit(OpCode::SUB);
            break;
        case TokenType::STAR:
            emit(OpCode::MUL);
            break;
        case TokenType::DIVISION:
            emit(OpCode::DIV);
            break;
        case TokenType::EQUAL_EQUAL:
            emit(OpCode::EQ);
            break;
        default:
            break;
        }
    }

    void Compiler::visit(Call &expr)
    {
        visit(expr.callee);
        for (const auto &arg : expr.arguments)
            visit(arg);

        emit(OpCode::CALL, (int)expr.arguments.size());
    }

    void Compiler::visit(ListLiteral &expr)
    {
        visit(expr.expressions);
        emit(OpCode::BUILD_LIST, (int)expr.expressions.size());
    }

    void Compiler::visit(TupleLiteral &expr)
    {
        visit(expr.expressions);
        emit(OpCode::BUILD_TUPLE, (int)expr.expressions.size());
    }

    void Compiler::visit(SetLiteral &expr)
    {
        visit(expr.expressions);
        emit(OpCode::BUILD_SET, (int)expr.expressions.size());
    }

    void Compiler::visit(MapLiteral &expr)
    {
        for (auto &[k, v] : expr.pairs)
        {
            visit(k);
            visit(v);
        }

        emit(OpCode::BUILD_MAP, (int)expr.pairs.size());
    }

    void Compiler::visit(RangeLiteral &expr)
    {
        if (expr.start)
            visit(expr.start);
        else
            emit(OpCode::LOAD_NONE);
        if (expr.end)
            visit(expr.end);
        else
            emit(OpCode::LOAD_NONE);
        if (expr.step)
            visit(expr.step);
        else
            emit(OpCode::LOAD_NONE);

        emit(OpCode::BUILD_RANGE, expr.is_inclusive ? 1 : 0);
    }

    void Compiler::visit(DotLiteral &expr) {}

    void Compiler::visit(TypePattern &expr) {}

    void Compiler::visit(Postfix &expr) {}

    // -----------------------------------------------------------------------
    // Utils & Run
    // -----------------------------------------------------------------------

    std::map<BlockId, size_t> Compiler::calculate_block_offsets() const
    {
        std::map<BlockId, size_t> offsets;
        size_t current = 0;
        for (const auto &block : graph.get_all_blocks())
        {
            offsets[block.get_id()] = current;
            current += block.get_code().length();
        }
        return offsets;
    }

    void Compiler::resolve_jumps_in_block(ByteVector &bytes, const std::map<BlockId, size_t> &offsets) const
    {
        size_t ip = 0;
        while (ip < bytes.size())
        {
            OpCode op = static_cast<OpCode>(bytes[ip]);
            if (op == OpCode::MAKE_FUNCTION)
            {
                ip += 2 + (static_cast<int>(bytes[ip + 1]) * 2);
                continue;
            }
            if (op == OpCode::JUMP || op == OpCode::JUMP_IF_FALSE || op == OpCode::LOOP_ITER)
            {
                BlockId target = static_cast<BlockId>(static_cast<uint8_t>(bytes[ip + 1]) | (static_cast<uint8_t>(bytes[ip + 2]) << 8));
                size_t off = offsets.at(target);
                bytes[ip + 1] = static_cast<std::byte>(off & 0xFF);
                bytes[ip + 2] = static_cast<std::byte>((off >> 8) & 0xFF);
            }
            ip += 1 + get_opcode_arity(op);
        }
    }

    CodeObject Compiler::flatten()
    {
        CodeObject final_bytecode;
        const auto &all_blocks = graph.get_all_blocks();

        std::map<BlockId, size_t> block_offsets = calculate_block_offsets();

        for (const auto &block : all_blocks)
        {
            const auto &block_code = block.get_code();

            if (block_code.length() > 0)
            {
                ByteVector temp_bytes(block_code.data(), block_code.data() + block_code.length());
                resolve_jumps_in_block(temp_bytes, block_offsets);
                final_bytecode.push(temp_bytes);
            }
        }

        return final_bytecode;
    }

    std::tuple<ConstantPool_ptr, CodeObject> Compiler::run(const Module &module)
    {
        emit(OpCode::ENTER_MODULE);

        for (const auto &stmt : module.statements)
        {
            visit(stmt);
        }

        BlockId exit = graph.create_block();
        graph.add_edge(current_block_id, exit);
        emit(OpCode::JUMP, static_cast<int>(exit));
        set_current_block(exit);

        emit(OpCode::EXIT_MODULE);

        return {constant_pool, flatten()};
    }
}
