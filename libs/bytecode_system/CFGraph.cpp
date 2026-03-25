#include "CFGraph.h"
#include "Doctor.h"
#include "OpCode.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace Wasp
{
    // ============================================================================
    // CodeObject
    // ============================================================================

    std::size_t CodeObject::length() const
    {
        return instructions.size();
    }

    void CodeObject::push(const ByteVector &instruction)
    {
        instructions.insert(
            instructions.end(),
            instruction.begin(),
            instruction.end());
    }

    void CodeObject::replace(std::size_t index, std::byte replacement)
    {
        instructions.at(index) = replacement;
    }

    void CodeObject::set(ByteVector instrs)
    {
        instructions = std::move(instrs);
    }
    void CodeObject::emit(OpCode opcode)
    {
        instructions.push_back(static_cast<std::byte>(opcode));
    }

    void CodeObject::emit(OpCode opcode, int operand)
    {
        instructions.push_back(static_cast<std::byte>(opcode));

        // Explicitly handle 16-bit operands for jumps
        if (opcode == OpCode::JUMP ||
            opcode == OpCode::JUMP_IF_FALSE ||
            opcode == OpCode::LOOP_ITER)
        {
            // Expand the assertion to allow up to 65,535
            Doctor::get().assert(
                operand >= 0 && operand <= 65535,
                WaspStage::Compiler,
                "Operand out of range for jump instruction (must be between 0 and 65535)"
            );

            // Write 16-bit payload in Little Endian (Low byte, then High byte)
            instructions.push_back(static_cast<std::byte>(operand & 0xFF));
            instructions.push_back(static_cast<std::byte>((operand >> 8) & 0xFF));
        }
        else
        {
            // Standard 8-bit operand
            Doctor::get().assert(
                operand >= 0 && operand <= 255,
                WaspStage::Compiler,
                "Operand out of range for 8-bit instruction (must be between 0 and 255)"
            );

            instructions.push_back(static_cast<std::byte>(operand));
        }
    }

    void CodeObject::emit(OpCode opcode, int operand_1, int operand_2) {
        Doctor::get().assert(
            operand_1 >= 0 && operand_1 <= 255,
            WaspStage::Compiler,
            "Operand 1 out of range for 8-bit encoding"
        );
        Doctor::get().assert(
            operand_2 >= 0 && operand_2 <= 255,
            WaspStage::Compiler,
            "Operand 2 out of range for 8-bit encoding"
        );

        instructions.push_back(static_cast<std::byte>(opcode));
        instructions.push_back(static_cast<std::byte>(operand_1));
        instructions.push_back(static_cast<std::byte>(operand_2));
    }

    ByteVector CodeObject::instruction_at(std::size_t index) const
    {
        std::byte opcode = instructions.at(index);
        int arity = get_opcode_arity(opcode);

        return ByteVector(
            instructions.begin() + index,
            instructions.begin() + index + 1 + arity);
    }

    ByteVector CodeObject::operands_of_opcode_at(std::size_t opcode_index) const
    {
        std::byte opcode = instructions.at(opcode_index);
        int arity = get_opcode_arity(opcode);

        if (arity == 0)
            return {};

        return ByteVector(
            instructions.begin() + opcode_index + 1,
            instructions.begin() + opcode_index + 1 + arity);
    }

    const std::byte *CodeObject::data() const
    {
        return instructions.data();
    }

    // ============================================================================
    // CFGraph
    // ============================================================================

    BlockId CFGraph::create_block()
    {
        BlockId new_id = blocks.size();
        blocks.push_back(BasicBlock(new_id));
        return new_id;
    }

    void CFGraph::set_entry_block(BlockId id) {
        Doctor::get().assert(id < blocks.size(), WaspStage::Compiler, "Invalid entry block ID");

        entry_block_id = id;
    }

    void CFGraph::add_edge(BlockId from_id, BlockId to_id)
    {
        Doctor::get().assert(
            from_id < blocks.size() && to_id < blocks.size(),
            WaspStage::Compiler,
            "Invalid block ID for edge"
        );

        auto &successors = blocks[from_id].successors;
        auto &predecessors = blocks[to_id].predecessors;

        // Only add the edge if it does not already exist
        if (std::find(successors.begin(), successors.end(), to_id) == successors.end())
        {
            successors.push_back(to_id);
            predecessors.push_back(from_id);
        }
    }

    BasicBlock &CFGraph::get_block(BlockId id)
    {
        Doctor::get().assert(id < blocks.size(), WaspStage::Compiler, "Invalid block ID");

        return blocks[id];
    }

    const BasicBlock &CFGraph::get_block(BlockId id) const
    {
        Doctor::get().assert(id < blocks.size(), WaspStage::Compiler, "Invalid block ID");

        return blocks[id];
    }
}