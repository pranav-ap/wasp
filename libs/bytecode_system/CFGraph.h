#pragma once

#include "OpCode.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace Wasp
{
    // ============================================================================
    // CodeObject
    // ============================================================================

    using ByteVector = std::vector<std::byte>;

    class CodeObject
    {
    private:
        ByteVector instructions;

    public:
        CodeObject() = default;
        CodeObject(ByteVector instrs) : instructions(std::move(instrs)) {}

        std::size_t length() const;

        void push(const ByteVector &instruction);
        void replace(std::size_t index, std::byte replacement);
        void set(ByteVector instrs);

        void emit(OpCode opcode);
        void emit(OpCode opcode, int operand);
        void emit(OpCode opcode, int operand_1, int operand_2);

        ByteVector instruction_at(std::size_t index) const;
        ByteVector operands_of_opcode_at(std::size_t opcode_index) const;

        const std::byte *data() const;
    };

    using CodeObject_ptr = std::shared_ptr<CodeObject>;

    // ============================================================================
    // BasicBlock
    // ============================================================================

    using BlockId = std::size_t;
    constexpr BlockId InvalidBlockId = static_cast<BlockId>(-1);

    class CFGraph;

    class BasicBlock
    {
        friend class CFGraph;

    private:
        BlockId id;

        CodeObject code;

        std::vector<BlockId> successors;
        std::vector<BlockId> predecessors;

        explicit BasicBlock(BlockId id) : id(id) {}

    public:
        BasicBlock() = delete;

        BlockId get_id() const { return id; }

        CodeObject &get_code() { return code; }
        const CodeObject &get_code() const { return code; }

        const std::vector<BlockId> &get_successors() const { return successors; }
        const std::vector<BlockId> &get_predecessors() const { return predecessors; }
    };

    // ============================================================================
    // CFGraph
    // ============================================================================

    class CFGraph
    {
    private:
        std::vector<BasicBlock> blocks;
        BlockId entry_block_id = InvalidBlockId;

    public:
        CFGraph() = default;

        BlockId create_block();

        void set_entry_block(BlockId id);
        void add_edge(BlockId from_id, BlockId to_id);

        BlockId get_entry_block() const { return entry_block_id; }

        BasicBlock &get_block(BlockId id);
        const BasicBlock &get_block(BlockId id) const;
        const std::vector<BasicBlock> &get_all_blocks() const { return blocks; }
    };
}