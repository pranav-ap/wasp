#include "CFGraph.h"
#include "Compiler.h"
#include "Doctor.h"
#include "OpCode.h"
#include "Workspace.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
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
    while (!locals.empty() && locals.back()->lexical_depth == current_lexical_scope_depth)
    {
        // Add a comment so the printer shows WHICH variable is being popped
        std::string var_name = locals.back()->name;
        locals.pop_back();

        emit(OpCode::POP, "pop " + var_name);
    }

    emit(OpCode::POP_SCOPE);
    current_lexical_scope_depth--;
}

// ------------------------------------------------------------------------
// Symbol & Closure Support
// ------------------------------------------------------------------------

int Compiler::resolve_local(int symbol_id)
{
    // Search backward to find the most recent declaration (shadowing support)
    for (int i = static_cast<int>(locals.size()) - 1; i >= 0; i--)
    {
        if (locals[i]->id == symbol_id)
        {
            // This is the physical stack index
            return i;
        }
    }

    // Not a local variable (must be an upvalue, native)
    return -1;
}

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

        uv.index = current_compiler->parent->resolve_local(symbol->id);

        Doctor::get().assert(
            uv.index != -1,
            WaspStage::Compiler,
            "Closure tried to capture an unknown local variable"
        );

        return current_compiler->add_upvalue(uv, symbol->name);
    }

    Upvalue uv;
    uv.is_local_to_parent = false;
    uv.index = resolve_upvalue(current_compiler->parent, symbol);

    return current_compiler->add_upvalue(uv, symbol->name);
}

// -----------------------------------------------------------------------
// Emit
// ----------------------------------------------------------------------

void Compiler::emit_raw_byte(std::byte b)
{
    ByteVector bv = {b};
    // The push method will automatically add the empty string padding
    // to the comments vector to keep everything aligned!
    graph.get_block(current_block_id).get_code().push(bv);
}

void Compiler::emit(OpCode opcode, std::string comment)
{
    graph.get_block(current_block_id).get_code().emit(opcode, std::move(comment));
}

void Compiler::emit(OpCode opcode, int operand, std::string comment)
{
    graph.get_block(current_block_id).get_code().emit(opcode, operand, std::move(comment));
}

void Compiler::emit(OpCode opcode, int operand_1, int operand_2, std::string comment)
{
    graph.get_block(current_block_id)
        .get_code()
        .emit(opcode, operand_1, operand_2, std::move(comment));
}

void Compiler::emit_local_cleanups(int target_depth)
{
    int locals_to_pop = 0;

    // Count how many physical variables are trapped in the scopes we are skipping
    for (auto it = locals.rbegin(); it != locals.rend(); ++it)
    {
        if ((*it)->lexical_depth > target_depth)
        {
            locals_to_pop++;
        }
        else
        {
            break;
        }
    }

    // Tell the VM to physically POP them!
    for (int i = 0; i < locals_to_pop; ++i)
    {
        emit(OpCode::POP, "branch bypass local cleanup");
    }

    // Now safe to pop the scope frames
    int scopes_to_pop = current_lexical_scope_depth - target_depth;
    for (int i = 0; i < scopes_to_pop; ++i)
    {
        emit(OpCode::POP_SCOPE);
    }
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

void Compiler::resolve_jumps_in_block(
    CodeObject& code,
    const std::map<BlockId, size_t>& offsets
) const
{
    size_t ip = 0;
    size_t len = code.length();
    const std::byte* data = code.data();

    while (ip < len)
    {
        OpCode op = static_cast<OpCode>(data[ip]);

        // Skip over function definitions
        if (op == OpCode::MAKE_FUNCTION)
        {
            ip += 2 + (static_cast<int>(data[ip + 1]) * 2);
            continue;
        }

        // Fix Jump Targets
        if (op == OpCode::JUMP || op == OpCode::JUMP_IF_FALSE || op == OpCode::LOOP_ITER)
        {
            BlockId target = static_cast<BlockId>(
                static_cast<uint8_t>(data[ip + 1]) | (static_cast<uint8_t>(data[ip + 2]) << 8)
            );

            size_t absolute_offset = offsets.at(target);

            // Use replace to update the bytes without touching the comments
            code.replace(ip + 1, static_cast<std::byte>(absolute_offset & 0xFF));
            code.replace(ip + 2, static_cast<std::byte>((absolute_offset >> 8) & 0xFF));
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
        CodeObject block_code = block.get_code();

        if (block_code.length() > 0)
        {
            resolve_jumps_in_block(block_code, block_offsets);
            final_bytecode.push(block_code);
        }
    }

    return final_bytecode;
}
} // namespace Wasp
