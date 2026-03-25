#include "CFGraph.h"
#include "Compiler.h"
#include "Doctor.h"
#include "OpCode.h"
#include "Workspace.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Wasp
{
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

int Compiler::add_upvalue(const Upvalue& uv, const std::string& name)
{
    // Check if we have already captured this exact variable

    for (int i = 0; i < static_cast<int>(upvalues.size()); i++)
    {
        if (upvalues[i].is_local_to_parent == uv.is_local_to_parent)
        {
            if (uv.is_local_to_parent && upvalues[i].index == uv.index)
            {
                return i;
            }

            if (!uv.is_local_to_parent && upvalues[i].index == uv.index)
            {
                return i;
            }
        }
    }

    upvalues.push_back(uv);

    int id = static_cast<int>(upvalues.size()) - 1;
    upvalue_index_to_name_map[id] = name;

    return id;
}

int Compiler::resolve_upvalue(Compiler* current_compiler, Symbol_ptr symbol)
{
    Doctor::get().fatal_if_nullptr(current_compiler->parent, WaspStage::Compiler);

    // Capturing an immediate parent's local variable
    if (symbol->declaration_depth == current_compiler->parent->compiler_depth)
    {
        Upvalue uv;
        uv.is_local_to_parent = true;
        uv.index = symbol->id;

        return current_compiler->add_upvalue(uv, symbol->name);
    }

    // Capturing a grandparent's variable
    int index = resolve_upvalue(current_compiler->parent, symbol);

    Upvalue uv;
    uv.is_local_to_parent = false;
    uv.index = index;

    return current_compiler->add_upvalue(uv, symbol->name);
}

// -----------------------------------------------------------------------
// Emit
// ----------------------------------------------------------------------

void Compiler::emit(OpCode opcode) { graph.get_block(current_block_id).get_code().emit(opcode); }

void Compiler::emit(OpCode opcode, int operand)
{
    graph.get_block(current_block_id).get_code().emit(opcode, operand);
}

void Compiler::emit(OpCode opcode, int operand_1, int operand_2)
{
    graph.get_block(current_block_id).get_code().emit(opcode, operand_1, operand_2);
}

void Compiler::emit_raw_byte(std::byte b)
{
    ByteVector bv = {b};
    graph.get_block(current_block_id).get_code().push(bv);
}

// -----------------------------------------------------------------------
// Flatten
// -----------------------------------------------------------------------

std::map<BlockId, size_t> Compiler::calculate_block_offsets() const
{
    std::map<BlockId, size_t> offsets;
    size_t current = 0;
    for (const auto& block : graph.get_all_blocks())
    {
        offsets[block.get_id()] = current;
        current += block.get_code().length();
    }
    return offsets;
}

void Compiler::resolve_jumps_in_block(ByteVector& bytes, const std::map<BlockId, size_t>& offsets)
    const
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
            BlockId target = static_cast<BlockId>(
                static_cast<uint8_t>(bytes[ip + 1]) | (static_cast<uint8_t>(bytes[ip + 2]) << 8));
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
    const auto& all_blocks = graph.get_all_blocks();

    std::map<BlockId, size_t> block_offsets = calculate_block_offsets();

    for (const auto& block : all_blocks)
    {
        const auto& block_code = block.get_code();

        if (block_code.length() > 0)
        {
            ByteVector temp_bytes(block_code.data(), block_code.data() + block_code.length());
            resolve_jumps_in_block(temp_bytes, block_offsets);
            final_bytecode.push(temp_bytes);
        }
    }

    return final_bytecode;
}

} // namespace Wasp
