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
    Compiler::Compiler()
        : constant_pool(std::make_shared<ConstantPool>()),
          current_block_id(InvalidBlockId),
          current_scope(nullptr),
          next_symbol_id(0)
    {
    }

    Compiler::Compiler(ConstantPool_ptr pool, SymbolScope_ptr enclosing_scope)
        : constant_pool(std::move(pool)),
          current_scope(std::make_shared<SymbolScope>(ScopeType::FUNCTION, enclosing_scope)),
          next_symbol_id(0)
    {
        current_block_id = graph.create_block();
        graph.set_entry_block(current_block_id);
    }

    // -----------------------------------------------------------------------
    // Scoping and Symbol Resolution
    // -----------------------------------------------------------------------

    void Compiler::enter_scope(ScopeType type, bool emit_opcodes)
    {
        current_scope = std::make_shared<SymbolScope>(type, current_scope);

        if (emit_opcodes)
        {
            emit(OpCode::PUSH_SCOPE);
        }
    }

    void Compiler::leave_scope(bool emit_opcodes)
    {
        if (current_scope)
        {
            current_scope = current_scope->get_enclosing();

            if (emit_opcodes)
            {
                emit(OpCode::POP_SCOPE);
            }
        }
    }

    // -----------------------------------------------------------------------
    // Emit
    // ----------------------------------------------------------------------

    void Compiler::emit(OpCode opcode)
    {
        graph.get_block(current_block_id).get_code().emit(opcode);
    }

    void Compiler::emit(OpCode opcode, int operand)
    {
        graph.get_block(current_block_id).get_code().emit(opcode, operand);
    }

    void Compiler::emit(OpCode opcode, int operand_1, int operand_2)
    {
        graph.get_block(current_block_id).get_code().emit(opcode, operand_1, operand_2);
    }

    // ========================================================================
    // Statement Visitors
    // ========================================================================

    void Compiler::visit(std::vector<Statement_ptr> &statements)
    {
        for (const auto &stmt : statements)
        {
            visit(stmt);
        }
    }

    void Compiler::visit(const Statement_ptr statement)
    {
        NULL_CHECK(statement);

        std::visit(overloaded{[&](ExpressionStatement &stat)
                              { visit(stat); },
                              [&](VariableDefinition &var_def)
                              { visit(var_def); },

                              [&](IfBranch &if_branch)
                              { visit(if_branch); },
                              [&](ElseBranch &else_branch)
                              { visit(else_branch); },

                              [&](SimpleLoop &loop_stmt)
                              { visit(loop_stmt); },
                              [&](ForInLoop &for_in_stmt)
                              { visit(for_in_stmt); },

                              [&](Pass &pass_stmt)
                              { visit(pass_stmt); },
                              [&](LoopControl &break_stmt)
                              { visit(break_stmt); },

                              [&](FunctionDefinition &func_def)
                              { visit(func_def); },
                              [&](Return &return_stmt)
                              { visit(return_stmt); },

                              [](auto)
                              { FATAL("Never Seen this Statement before!"); }},
                   statement->data);
    }

    void Compiler::visit(ExpressionStatement &statement)
    {
        visit(statement.expression);
        emit(OpCode::POP);
    }

    // ------------------------------------------------------------------------
    // Variable Definition
    // -----------------------------------------------------------------------

    void Compiler::visit(VariableDefinition &statement)
    {
        compile_variable_definition(statement.expression, statement.is_mutable);
    }

    void Compiler::visit(VariableDefinitionExpression &expr)
    {
        compile_variable_definition(expr.assignment, expr.is_mutable, true);
    }

    //-----------------------------------------------------------------------
    // Control Flow
    //-----------------------------------------------------------------------

    void Compiler::visit(IfBranch &statement)
    {
        visit(statement.test);

        bool has_alternative = statement.alternative.has_value();

        BlockId true_block_id = graph.create_block();
        BlockId end_block_id = graph.create_block();
        BlockId false_block_id = has_alternative ? graph.create_block() : end_block_id;

        graph.add_edge(current_block_id, true_block_id);
        graph.add_edge(current_block_id, false_block_id);
        graph.add_edge(current_block_id, end_block_id);

        emit(OpCode::JUMP_IF_FALSE, static_cast<int>(false_block_id));
        emit(OpCode::JUMP, static_cast<int>(true_block_id));

        // ========================================================================
        // Compile the True Branch
        // ========================================================================
        set_current_block(true_block_id);
        enter_scope(ScopeType::BRANCH, true);
        visit(statement.body);
        leave_scope(true);

        emit(OpCode::JUMP, static_cast<int>(end_block_id));
        graph.add_edge(current_block_id, end_block_id);

        // ========================================================================
        // Compile the False Branch (Elif or Else)
        // ========================================================================
        if (has_alternative)
        {
            set_current_block(false_block_id);

            auto &alt_variant = statement.alternative.value()->data;

            if (std::holds_alternative<IfBranch>(alt_variant))
            {
                visit(std::get<IfBranch>(alt_variant));
            }
            else if (std::holds_alternative<ElseBranch>(alt_variant))
            {
                visit(std::get<ElseBranch>(alt_variant));
            }

            // Once the alternative finishes, jump to the end block
            emit(OpCode::JUMP, static_cast<int>(end_block_id));
            graph.add_edge(current_block_id, end_block_id);
        }

        set_current_block(end_block_id);
    }

    void Compiler::visit(ElseBranch &statement)
    {
        enter_scope(ScopeType::BRANCH, true);
        visit(statement.body);
        leave_scope(true);
    }

    void Compiler::visit(SimpleLoop &statement)
    {
        BlockId header_block_id = graph.create_block();
        BlockId body_block_id = graph.create_block();
        BlockId end_block_id = graph.create_block();

        emit(OpCode::JUMP, static_cast<int>(header_block_id));
        graph.add_edge(current_block_id, header_block_id);

        loop_tracking_stack.push_back({header_block_id, body_block_id, end_block_id});

        // ========================================================================
        // Compile the Header
        // ========================================================================
        set_current_block(header_block_id);
        visit(statement.condition);

        if (statement.style == TokenType::UNTIL || statement.style == TokenType::UNLESS)
        {
            emit(OpCode::NOT);
        }

        graph.add_edge(header_block_id, body_block_id);
        graph.add_edge(header_block_id, end_block_id);

        // If condition is false, jump to the end. Otherwise, jump into the body.
        emit(OpCode::JUMP_IF_FALSE, static_cast<int>(end_block_id));
        emit(OpCode::JUMP, static_cast<int>(body_block_id));

        // ========================================================================
        // Compile the Body
        // ========================================================================
        set_current_block(body_block_id);
        enter_scope(ScopeType::LOOP, true);
        visit(statement.body);
        leave_scope(true);

        emit(OpCode::JUMP, static_cast<int>(header_block_id));
        graph.add_edge(current_block_id, header_block_id);

        loop_tracking_stack.pop_back();

        set_current_block(end_block_id);
    }

    void Compiler::visit(ForInLoop &statement)
    {
        // Place IteratorObject on TOS
        visit(statement.iterable_expression);

        BlockId header_block_id = graph.create_block();
        BlockId body_block_id = graph.create_block();
        BlockId end_block_id = graph.create_block();

        emit(OpCode::JUMP, static_cast<int>(header_block_id));
        graph.add_edge(current_block_id, header_block_id);

        // ========================================================================
        // Compile the Header
        // ========================================================================
        set_current_block(header_block_id);

        // LOOP_ITER checks the iterator on the stack.
        // If it's exhausted, it jumps to end_block_id.
        // If it has a next item, it pushes that item onto the stack and falls through.
        emit(OpCode::LOOP_ITER, static_cast<int>(end_block_id));
        emit(OpCode::JUMP, static_cast<int>(body_block_id));

        graph.add_edge(header_block_id, body_block_id);
        graph.add_edge(header_block_id, end_block_id);

        loop_tracking_stack.push_back({header_block_id, body_block_id, end_block_id});

        // ========================================================================
        // Compile the Body
        // ========================================================================
        set_current_block(body_block_id);
        enter_scope(ScopeType::LOOP, true);

        if (statement.lhs->is<Identifier>())
        {
            std::string var_name = statement.lhs->as<Identifier>().name;

            int symbol_id = next_symbol_id++;
            auto symbol = std::make_shared<Symbol>(symbol_id, var_name, nullptr, false, true);

            current_scope->define(var_name, symbol);
            debug_name_map[symbol_id] = var_name;
            emit(OpCode::DEFINE_LOCAL, symbol_id);
        }
        else
        {
            FATAL("Compiler Error: For-in loop LHS must be a simple Identifier.");
        }

        visit(statement.body);

        leave_scope(true);

        emit(OpCode::JUMP, static_cast<int>(header_block_id));
        graph.add_edge(current_block_id, header_block_id);

        loop_tracking_stack.pop_back();

        // ========================================================================
        // Exit the loop
        // ========================================================================
        set_current_block(end_block_id);
        // Pop the IterableObject
        emit(OpCode::POP);
    }

    void Compiler::visit(LoopControl &statement)
    {
        if (!statement.label.empty())
        {
            FATAL("Compiler Error: Labeled break, continue, and redo statements are not supported yet.");
        }

        if (loop_tracking_stack.empty())
        {
            FATAL("Compiler Error: Loop control statement not within a loop.");
        }

        int scopes_to_pop = 0;
        SymbolScope_ptr temp_scope = current_scope;

        while (temp_scope && temp_scope->get_type() != ScopeType::LOOP)
        {
            scopes_to_pop++;
            temp_scope = temp_scope->get_enclosing();
        }

        scopes_to_pop++;

        for (int i = 0; i < scopes_to_pop; i++)
        {
            emit(OpCode::POP_SCOPE);
        }

        auto [header_id, body_id, end_id] = loop_tracking_stack.back();

        if (statement.type == TokenType::BREAK)
        {
            emit(OpCode::JUMP, static_cast<int>(end_id));
            graph.add_edge(current_block_id, end_id);
        }
        else if (statement.type == TokenType::CONTINUE)
        {
            emit(OpCode::JUMP, static_cast<int>(header_id));
            graph.add_edge(current_block_id, header_id);
        }
        else if (statement.type == TokenType::REDO)
        {
            // Redo skips the header and jumps straight back to the body
            emit(OpCode::JUMP, static_cast<int>(body_id));
            graph.add_edge(current_block_id, body_id);
        }
        else
        {
            FATAL("Compiler Error: Unknown LoopControl statement type.");
        }
    }

    void Compiler::visit(Pass &statement)
    {
        // do nothing
    }

    // -----------------------------------------------------------------------
    // Function Definition
    // ------------------------------------------------------------------------

    void Compiler::visit(FunctionDefinition &statement)
    {
        Compiler func_compiler(constant_pool, current_scope);

        // Define parameters as local variables (IDs 0, 1, 2...)
        for (const auto &[param_name, param_type] : statement.parameters)
        {
            int symbol_id = func_compiler.next_symbol_id++;
            auto symbol = std::make_shared<Symbol>(symbol_id, param_name, nullptr, false, true);

            func_compiler.current_scope->define(param_name, symbol);
            func_compiler.debug_name_map[symbol_id] = param_name;
        }

        func_compiler.visit(statement.body);

        // Place a default return at the end of the block
        func_compiler.emit(OpCode::LOAD_NONE);
        func_compiler.emit(OpCode::RETURN);

        CodeObject func_code = func_compiler.flatten();
        func_code.local_names = std::move(func_compiler.debug_name_map);

        int const_id = constant_pool->allocate_function_definition(
            std::move(func_code),
            std::move(func_compiler.debug_name_map));

        // ========================================================================
        // Back to the Main Compiler
        // ========================================================================

        emit(OpCode::LOAD_CONST, const_id);
        emit(OpCode::MAKE_FUNCTION);

        int symbol_id = next_symbol_id++;
        auto symbol = std::make_shared<Symbol>(symbol_id, statement.name, nullptr, false, true);

        current_scope->define(statement.name, symbol);
        debug_name_map[symbol_id] = statement.name;

        emit(OpCode::DEFINE_LOCAL, symbol_id);
    }

    void Compiler::visit(Return &statement)
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

    // ========================================================================
    // Expression Visitors
    // ========================================================================

    void Compiler::visit(const Expression_ptr expr)
    {
        NULL_CHECK(expr);

        std::visit(overloaded{[&](std::monostate) { /* Do nothing */ },

                              [&](int val)
                              { visit(val); },
                              [&](double val)
                              { visit(val); },
                              [&](std::string val)
                              { visit(val); },
                              [&](bool val)
                              { visit(val); },

                              [&](Identifier &id)
                              { visit(id); },
                              [&](DotLiteral &dot)
                              { visit(dot); },

                              [&](Prefix &prefix)
                              { visit(prefix); },
                              [&](Infix &infix)
                              { visit(infix); },
                              [&](Postfix &postfix)
                              { visit(postfix); },

                              [&](ListLiteral &list)
                              { visit(list); },
                              [&](TupleLiteral &tuple)
                              { visit(tuple); },
                              [&](MapLiteral &map)
                              { visit(map); },
                              [&](SetLiteral &set)
                              { visit(set); },
                              [&](RangeLiteral &range)
                              { visit(range); },

                              [&](VariableDefinitionExpression &var_def)
                              { visit(var_def); },
                              [&](UntypedAssignment &untyped_assign)
                              { visit(untyped_assign); },
                              [&](TypedAssignment &typed_assign)
                              { visit(typed_assign); },
                              [&](TypePattern &type_pattern)
                              { visit(type_pattern); },

                              [&](IfTernaryBranch &if_branch)
                              { visit(if_branch); },
                              [&](ElseTernaryBranch &else_branch)
                              { visit(else_branch); },

                              [&](Call &call)
                              { visit(call); },

                              [&](auto &)
                              { FATAL("Unhandled Expression type in variant dispatch!"); }},
                   expr->data);
    }

    void Compiler::visit(std::vector<Expression_ptr> &expressions)
    {
        for (const auto &expr : expressions)
        {
            visit(expr);
        }
    }

    // -----------------------------------------------------------------------
    // Primitives
    // -----------------------------------------------------------------------

    void Compiler::visit(int expr)
    {
        int id = constant_pool->allocate(expr);
        emit(OpCode::LOAD_CONST, id);
    }

    void Compiler::visit(double expr)
    {
        int id = constant_pool->allocate(expr);
        emit(OpCode::LOAD_CONST, id);
    }

    void Compiler::visit(std::string expr)
    {
        int id = constant_pool->allocate(expr);
        emit(OpCode::LOAD_CONST, id);
    }

    void Compiler::visit(bool expr)
    {
        if (expr)
        {
            emit(OpCode::LOAD_TRUE);
        }
        else
        {
            emit(OpCode::LOAD_FALSE);
        }
    }

    void Compiler::visit(Identifier &expr)
    {
        auto symbol = current_scope->lookup(expr.name);

        if (!symbol)
        {
            FATAL("Compiler Error: Attempted to load undefined variable '" + expr.name + "'");
        }

        emit(OpCode::GET_LOCAL, symbol->id);
    }

    void Compiler::visit(Prefix &expr)
    {
        visit(expr.operand);

        // emit appropriate opcode based on expr.op
        switch (expr.op.type)
        {
        case TokenType::PLUS:
            // do nothing
            break;
        case TokenType::MINUS:
            emit(OpCode::NEGATE);
            break;
        case TokenType::NOT:
            emit(OpCode::NOT);
            break;
        default:
            FATAL("Unsupported prefix operator: " + expr.op.value);
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
        case TokenType::REMINDER:
            emit(OpCode::MOD);
            break;
        case TokenType::EQUAL_EQUAL:
            emit(OpCode::EQ);
            break;
        case TokenType::BANG_EQUAL:
            emit(OpCode::NE);
            break;
        case TokenType::LESSER_THAN:
            emit(OpCode::LT);
            break;
        case TokenType::LESSER_THAN_EQUAL:
            emit(OpCode::LE);
            break;
        case TokenType::GREATER_THAN:
            emit(OpCode::GT);
            break;
        case TokenType::GREATER_THAN_EQUAL:
            emit(OpCode::GE);
            break;
        case TokenType::AND:
            emit(OpCode::LOGICAL_AND);
            break;
        case TokenType::OR:
            emit(OpCode::LOGICAL_OR);
            break;
        default:
            FATAL("Unsupported infix operator: " + expr.op.value);
        }
    }

    void Compiler::visit(Postfix &expr)
    {
        // no prefix ops available
    }

    void Compiler::visit(ListLiteral &expr)
    {
        visit(expr.expressions);
        emit(OpCode::BUILD_LIST, static_cast<int>(expr.expressions.size()));
    }

    void Compiler::visit(TupleLiteral &expr)
    {
        visit(expr.expressions);
        emit(OpCode::BUILD_TUPLE, static_cast<int>(expr.expressions.size()));
    }

    void Compiler::visit(MapLiteral &expr)
    {
        for (const auto &[key, value] : expr.pairs)
        {
            visit(key);
            visit(value);
        }

        emit(OpCode::BUILD_MAP, static_cast<int>(expr.pairs.size()));
    }

    void Compiler::visit(SetLiteral &expr)
    {
        visit(expr.expressions);
        emit(OpCode::BUILD_SET, static_cast<int>(expr.expressions.size()));
    }

    void Compiler::visit(RangeLiteral &expr)
    {
        if (expr.start)
        {
            visit(expr.start);
        }
        else
        {
            emit(OpCode::LOAD_NONE);
        }

        if (expr.end)
        {
            visit(expr.end);
        }
        else
        {
            emit(OpCode::LOAD_NONE);
        }

        if (expr.step)
        {
            visit(expr.step);
        }
        else
        {
            emit(OpCode::LOAD_NONE);
        }

        emit(OpCode::BUILD_RANGE, expr.is_inclusive ? 1 : 0);
    }

    void Compiler::visit(UntypedAssignment &expr)
    {
        compile_assignment(expr.lhs_expression, expr.rhs_expression);
    }

    void Compiler::visit(TypedAssignment &expr)
    {
        compile_assignment(expr.lhs_expression, expr.rhs_expression);
    }

    void Compiler::visit(TypePattern &expr)
    {
        // nothing to do
    }

    void Compiler::visit(IfTernaryBranch &expr)
    {
        // Evaluate condition into current block
        visit(expr.test);

        // Provision blocks for the branch

        BlockId true_block_id = graph.create_block();
        BlockId false_block_id = expr.alternative ? graph.create_block() : InvalidBlockId;
        BlockId end_block_id = graph.create_block();

        if (!expr.alternative)
        {
            false_block_id = end_block_id;
        }

        // Track logical edges in your CFG
        graph.add_edge(current_block_id, true_block_id);
        graph.add_edge(current_block_id, false_block_id);

        emit(OpCode::JUMP_IF_FALSE, static_cast<int>(false_block_id));

        // Compile the true branch
        set_current_block(true_block_id);
        visit(expr.true_expression);
        emit(OpCode::JUMP, static_cast<int>(end_block_id));
        graph.add_edge(true_block_id, end_block_id);

        // Compile the false (alternative) branch
        set_current_block(false_block_id);
        visit(expr.alternative);
        emit(OpCode::JUMP, static_cast<int>(end_block_id));
        graph.add_edge(false_block_id, end_block_id);

        set_current_block(end_block_id);
    }

    void Compiler::visit(ElseTernaryBranch &expr)
    {
        visit(expr.expression);
    }

    void Compiler::visit(Call &expr) {}

    void Compiler::visit(DotLiteral &expr) {}

    // -----------------------------------------------------------------------
    // UTILS
    // -----------------------------------------------------------------------

    void Compiler::set_current_block(BlockId block_id)
    {
        current_block_id = block_id;
    }

    void Compiler::compile_variable_definition(const Expression_ptr &assignment, bool is_mutable, bool as_expression)
    {
        NULL_CHECK(assignment);

        std::string var_name;
        Expression_ptr rhs_expr = nullptr;

        if (assignment->is<UntypedAssignment>())
        {
            const auto &assign = assignment->as<UntypedAssignment>();
            if (assign.lhs_expression->is<Identifier>())
            {
                var_name = assign.lhs_expression->as<Identifier>().name;
                rhs_expr = assign.rhs_expression;
            }
            else
            {
                FATAL("Left-hand side of definition must be an Identifier.");
            }
        }
        else if (assignment->is<TypedAssignment>())
        {
            const auto &assign = assignment->as<TypedAssignment>();
            if (assign.lhs_expression->is<Identifier>())
            {
                var_name = assign.lhs_expression->as<Identifier>().name;
                rhs_expr = assign.rhs_expression;
            }
            else
            {
                FATAL("Left-hand side of definition must be an Identifier.");
            }
        }
        else
        {
            FATAL("Definition assignment must be an UntypedAssignment or TypedAssignment.");
        }

        // Visit RHS & push result onto stack

        visit(rhs_expr);

        if (as_expression)
        {
            emit(OpCode::DUP);
        }

        // Register symbol in the current lexical scope

        int symbol_id = next_symbol_id++;

        auto symbol = std::make_shared<Symbol>(
            symbol_id,
            var_name,
            nullptr, // Type info could go here later if using TypedAssignment
            false,   // is_public
            is_mutable);

        current_scope->define(var_name, symbol);
        debug_name_map[symbol_id] = var_name;

        // Emit

        emit(OpCode::DEFINE_LOCAL, symbol_id);
    }

    void Compiler::compile_assignment(const Expression_ptr &lhs, const Expression_ptr &rhs)
    {
        visit(rhs);

        if (lhs->is<Identifier>())
        {
            const auto &var_name = lhs->as<Identifier>().name;
            auto symbol = current_scope->lookup(var_name);

            if (!symbol)
            {
                FATAL("Compiler Error: Attempted to assign to undefined variable '" + var_name + "'");
            }

            if (!symbol->is_mutable)
            {
                FATAL("Compiler Error: Cannot reassign immutable variable '" + var_name + "'");
            }

            emit(OpCode::SET_LOCAL, symbol->id);
        }
        else
        {
            FATAL("Unsupported assignment target. Only simple variable assignments are supported for now.");
        }
    }

    std::map<BlockId, size_t> Compiler::calculate_block_offsets() const
    {
        std::map<BlockId, size_t> offsets;
        size_t current_offset = 0;

        for (const auto &block : graph.get_all_blocks())
        {
            offsets[block.get_id()] = current_offset;
            current_offset += block.get_code().length();
        }

        return offsets;
    }

    void Compiler::resolve_jumps_in_block(ByteVector &bytes, const std::map<BlockId, size_t> &offsets) const
    {
        size_t ip = 0;
        while (ip < bytes.size())
        {
            OpCode op = static_cast<OpCode>(bytes[ip]);

            // If it is a jump instruction, replace the 16-bit BlockId operand with the absolute byte offset
            if (op == OpCode::JUMP || op == OpCode::JUMP_IF_FALSE || op == OpCode::LOOP_ITER)
            {
                // 1. Read the 2-byte BlockId placeholder (Little Endian)
                BlockId target_block = static_cast<BlockId>(
                    static_cast<uint8_t>(bytes[ip + 1]) |
                    (static_cast<uint8_t>(bytes[ip + 2]) << 8));

                // 2. Look up the true absolute offset
                size_t absolute_offset = offsets.at(target_block);

                // 3. Safety check
                if (absolute_offset > 0xFFFF)
                {
                    FATAL("Jump target offset exceeds 16-bit limit (65535 bytes).");
                }

                // 4. Write the 2-byte absolute offset back (Little Endian)
                bytes[ip + 1] = static_cast<std::byte>(absolute_offset & 0xFF);
                bytes[ip + 2] = static_cast<std::byte>((absolute_offset >> 8) & 0xFF);
            }

            // Skip to the next instruction: 1 byte for OpCode + N bytes for operands
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

    // -----------------------------------------------------------------------
    // RUN
    // -----------------------------------------------------------------------

    std::tuple<ConstantPool_ptr, CodeObject> Compiler::run(const Module &module)
    {
        current_block_id = graph.create_block();
        graph.set_entry_block(current_block_id);

        emit(OpCode::ENTER_MODULE);
        enter_scope(ScopeType::MODULE, false);

        for (const auto &stmt : module.statements)
        {
            visit(stmt);
        }

        leave_scope(false);

        BlockId exit_block_id = graph.create_block();
        graph.add_edge(current_block_id, exit_block_id);
        emit(OpCode::JUMP, static_cast<int>(exit_block_id));
        set_current_block(exit_block_id);
        emit(OpCode::EXIT_MODULE);

        CodeObject final_bytecode = flatten();

        return {constant_pool, final_bytecode};
    }
}