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

    // -----------------------------------------------------------------------
    // Scoping and Symbol Resolution
    // -----------------------------------------------------------------------

    void Compiler::enter_scope(ScopeType type)
    {
        current_scope = std::make_shared<SymbolScope>(type, current_scope);
    }

    void Compiler::leave_scope()
    {
        if (current_scope)
        {
            current_scope = current_scope->get_enclosing();
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
                              [](auto)
                              { FATAL("Never Seen this Statement before!"); }},
                   statement->data);
    }

    void Compiler::visit(ExpressionStatement &statement)
    {
        visit(statement.expression);
        emit(OpCode::POP_FROM_STACK);
    }

    // ========================================================================
    // Expression Visitors
    // ========================================================================

    void Compiler::visit(const Expression_ptr expr)
    {
        NULL_CHECK(expr);

        std::visit(overloaded{[&](std::monostate) { /* Do nothing */ },

                              // Primitives
                              [&](int val)
                              { visit(val); },
                              [&](double val)
                              { visit(val); },
                              [&](std::string val)
                              { visit(val); },
                              [&](bool val)
                              { visit(val); },

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
        emit(OpCode::PUSH_CONSTANT, id);
    }

    void Compiler::visit(double expr)
    {
        int id = constant_pool->allocate(expr);
        emit(OpCode::PUSH_CONSTANT, id);
    }

    void Compiler::visit(std::string expr)
    {
        int id = constant_pool->allocate(expr);
        emit(OpCode::PUSH_CONSTANT, id);
    }

    void Compiler::visit(bool expr)
    {
        if (expr)
        {
            emit(OpCode::PUSH_CONSTANT_TRUE);
        }
        else
        {
            emit(OpCode::PUSH_CONSTANT_FALSE);
        }
    }

    void Compiler::visit(Identifier &expr) {}
    void Compiler::visit(DotLiteral &expr) {}
    void Compiler::visit(DotDotLiteral &expr) {}
    void Compiler::visit(DotDotDotLiteral &expr) {}

    void Compiler::visit(Prefix &expr) {}

    void Compiler::visit(Infix &expr)
    {
    }

    void Compiler::visit(Postfix &expr) {}

    void Compiler::visit(ListLiteral &expr) {}
    void Compiler::visit(TupleLiteral &expr) {}
    void Compiler::visit(MapLiteral &expr) {}
    void Compiler::visit(SetLiteral &expr) {}
    void Compiler::visit(RangeLiteral &expr) {}

    void Compiler::visit(VariableDefinitionExpression &expr) {}
    void Compiler::visit(UntypedAssignment &expr) {}
    void Compiler::visit(TypedAssignment &expr) {}
    void Compiler::visit(TypePattern &expr) {}

    void Compiler::visit(IfTernaryBranch &expr) {}
    void Compiler::visit(ElseTernaryBranch &expr) {}

    void Compiler::visit(Call &expr) {}

    // -----------------------------------------------------------------------
    // UTILS
    // -----------------------------------------------------------------------

    CodeObject Compiler::flatten()
    {
        CodeObject final_bytecode;
        const auto &all_blocks = graph.get_all_blocks();

        // Pass 1: Map BlockId to its starting offset in the final byte stream
        std::map<BlockId, size_t> block_offsets;
        size_t current_offset = 0;

        for (const auto &block : all_blocks)
        {
            block_offsets[block.get_id()] = current_offset;
            current_offset += block.get_code().length();
        }

        // Pass 2: Concatenate all block instructions into one linear object
        for (const auto &block : all_blocks)
        {
            const auto &block_code = block.get_code();

            // Create a temporary ByteVector from the raw data to match your push API
            if (block_code.length() > 0)
            {
                ByteVector temp_bytes(block_code.data(), block_code.data() + block_code.length());
                final_bytecode.push(temp_bytes);
            }
        }

        // NOTE: In the future, you will add a Pass 3 here to iterate through
        // final_bytecode and replace jump-block-IDs with these block_offsets.

        return final_bytecode;
    }

    // -----------------------------------------------------------------------
    // RUN
    // -----------------------------------------------------------------------

    std::tuple<ConstantPool_ptr, CodeObject> Compiler::run(const Module &module)
    {
        current_block_id = graph.create_block();
        graph.set_entry_block(current_block_id);

        emit(OpCode::START);
        enter_scope(ScopeType::MODULE);

        for (const auto &stmt : module.statements)
        {
            visit(stmt);
        }

        leave_scope();
        emit(OpCode::STOP);

        CodeObject final_bytecode = flatten();

        return {constant_pool, final_bytecode};
    }
}