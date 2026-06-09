#include "CFGraph.h"
#include "Compiler.h"
#include "Doctor.h"
#include "OpCode.h"
#include "Workspace.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

std::string Compiler::mangle_name(
    const std::string& fn_name,
    const std::string& class_name,
    const std::string& path_override
)
{
    std::string full_path = path_override.empty() ? this->module_path : path_override;

    if (full_path.empty())
    {
        return (class_name.empty() ? "" : class_name + ".") + fn_name;
    }

    std::replace(full_path.begin(), full_path.end(), '\\', '/');

    std::string prefix;
    size_t libs_pos = full_path.find("libs/");

    if (libs_pos != std::string::npos)
    {
        prefix = full_path.substr(libs_pos);
    }
    else
    {
        prefix = std::filesystem::path(full_path).stem().string();
    }

    size_t ext_pos = prefix.rfind(".wasp");
    if (ext_pos != std::string::npos)
    {
        prefix.erase(ext_pos);
    }

    // Replace folder separators with dots
    std::string::size_type pos = 0;
    while ((pos = prefix.find('/', pos)) != std::string::npos)
    {
        prefix.replace(pos, 1, ".");
        pos += 1;
    }

    // Final Assembly
    std::string result = prefix;
    if (!class_name.empty())
        result += "." + class_name;

    result += "." + fn_name;

    return result;
}

// ------------------------------------------------------------------------
// Scope
// ------------------------------------------------------------------------

void Compiler::enter_scope(std::string comment)
{
    emit(OpCode::PUSH_SCOPE, std::move(comment));
    current_lexical_scope_depth++;
}

void Compiler::dumb_leave_scope(std::string comment)
{
    emit(OpCode::POP_SCOPE, std::move(comment));
}

void Compiler::leave_scope(std::string comment)
{
    while (!stack.empty() && stack.back()->lexical_depth == current_lexical_scope_depth)
    {
        stack.pop_back();
    }

    emit(OpCode::POP_SCOPE, std::move(comment));
    current_lexical_scope_depth--;
}

void Compiler::leave_scope_keep_tos(std::string comment)
{
    Symbol_ptr tos = nullptr;

    if (!stack.empty() && stack.back()->lexical_depth == current_lexical_scope_depth)
    {
        tos = stack.back();
    }

    while (!stack.empty() && stack.back()->lexical_depth == current_lexical_scope_depth)
    {
        stack.pop_back();
    }

    emit(OpCode::POP_SCOPE_KEEP_TOS, std::move(comment));
    current_lexical_scope_depth--;

    if (tos)
    {
        tos->lexical_depth = current_lexical_scope_depth;
        stack.push_back(tos);
    }
}

// ------------------------------------------------------------------------
// Symbol & Closure Support
// ------------------------------------------------------------------------

int Compiler::get_or_add_local_index(const Symbol_ptr& symbol)
{
    int physical_index = resolve_local(symbol->id);

    if (physical_index == -1)
    {
        physical_index = static_cast<int>(stack.size());
        stack.push_back(symbol);
    }

    return physical_index;
}

int Compiler::resolve_local(int symbol_id)
{
    // Search backward to find the most recent declaration (shadowing support)
    for (int i = static_cast<int>(stack.size()) - 1; i >= 0; i--)
    {
        if (stack[i]->id == symbol_id)
        {
            // This is the stack index
            return i;
        }
    }

    if (parent != nullptr)
    {
        return parent->resolve_local(symbol_id);
    }

    // Not a variable on stack
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
    if (symbol->closure_depth == current_compiler->parent->compiler_depth)
    {
        Upvalue uv;
        uv.is_local_to_parent = true;

        uv.index = current_compiler->parent->resolve_local(symbol->id);

        Doctor::get().assert(
            uv.index != -1,
            WaspStage::Compiler,
            "Closure tried to capture an unknown local variable " + symbol->name
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

void Compiler::emit(
    OpCode opcode,
    int operand_1,
    int operand_2,
    int operand_3,
    std::string comment
)
{
    graph.get_block(current_block_id)
        .get_code()
        .emit(opcode, operand_1, operand_2, operand_3, std::move(comment));
}

void Compiler::emit_local_cleanups(int target_depth)
{
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

        if (op == OpCode::BUILD_FUNCTION)
        {
            ip += 2 + (static_cast<int>(data[ip + 1]) * 2);
            continue;
        }

        if (op == OpCode::JUMP || op == OpCode::JUMP_IF_FALSE || op == OpCode::LOOP_ITER)
        {
            BlockId target = static_cast<BlockId>(
                static_cast<uint8_t>(data[ip + 1]) | (static_cast<uint8_t>(data[ip + 2]) << 8)
            );

            size_t absolute_offset = offsets.at(target);

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
