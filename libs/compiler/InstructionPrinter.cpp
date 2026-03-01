#include "InstructionPrinter.h"
#include <iostream>
#include <iomanip>
#include <sstream>

#ifndef ASSERT
#include <cassert>
#define ASSERT(condition, message) assert((condition) && message)
#endif

#define OPCODE_WIDTH 15 // Widened slightly to fit longer names like JUMP_IF_FALSE
#define OPERAND_WIDTH 10

using std::byte;
using std::cout;
using std::setw;
using std::string;

namespace Wasp
{
    string InstructionPrinter::stringify_instruction(byte opcode, byte operand)
    {
        int op_int = std::to_integer<int>(operand);
        std::stringstream ss;

        ss << std::left << setw(OPCODE_WIDTH) << stringify_opcode(opcode) << " " << op_int;

        switch (static_cast<OpCode>(opcode))
        {
        case OpCode::LOAD_CONST:
            if (constant_pool)
            {
                ss << std::right << setw(OPERAND_WIDTH) << " (" << stringify_object(constant_pool->get(op_int)) << ")";
            }
            break;
        case OpCode::DEFINE_LOCAL:
        case OpCode::SET_LOCAL:
        case OpCode::GET_LOCAL:
            if (name_map.contains(op_int))
            {
                ss << std::right << setw(OPERAND_WIDTH) << " (" << name_map.at(op_int) << ")";
            }
            break;
        default:
            break;
        }
        return ss.str();
    }

    string InstructionPrinter::stringify_instruction(byte opcode, byte op1, byte op2)
    {
        OpCode op = static_cast<OpCode>(opcode);
        std::stringstream ss;

        // Check if this is a 16-bit little-endian payload (our jumps)
        if (op == OpCode::JUMP || op == OpCode::JUMP_IF_FALSE || op == OpCode::LOOP_ITER)
        {
            // Reconstruct the 16-bit absolute byte offset
            int target_offset = std::to_integer<int>(op1) | (std::to_integer<int>(op2) << 8);

            ss << std::left << setw(OPCODE_WIDTH) << stringify_opcode(opcode)
               << " " << target_offset;
        }
        // Otherwise, treat as two distinct 8-bit operands (like CALL)
        else
        {
            int op1_int = std::to_integer<int>(op1);
            int op2_int = std::to_integer<int>(op2);

            ss << std::left << setw(OPCODE_WIDTH) << stringify_opcode(opcode)
               << " " << op1_int << " " << op2_int;

            if (op == OpCode::CALL)
            {
                string name = name_map.contains(op1_int) ? name_map.at(op1_int) : "unknown";
                ss << " (fn " << name << ", " << op2_int << " args)";
            }
        }

        return ss.str();
    }

    void InstructionPrinter::print(const CodeObject &code_object, std::ostream &out)
    {
        int length = static_cast<int>(code_object.length());
        int index_width = std::to_string(length).size() + 2;

        for (int index = 0; index < length;)
        {
            auto instruction = code_object.instruction_at(index);
            byte opcode = instruction[0];
            int arity = static_cast<int>(instruction.size()) - 1;

            out << std::right << setw(index_width) << index << ": ";

            if (arity == 0)
                out << stringify_opcode(opcode) << "\n";
            else if (arity == 1)
                out << stringify_instruction(opcode, instruction[1]) << "\n";
            else if (arity == 2)
                out << stringify_instruction(opcode, instruction[1], instruction[2]) << "\n";

            index += static_cast<int>(instruction.size());
        }
        out << std::endl;
    }

    void InstructionPrinter::print(const CFGraph &graph, std::ostream &out)
    {
        out << "digraph CFG {\n";
        // Use a monospaced font so your hex dump alignment stays perfect
        out << "    node [shape=box, fontname=\"Courier\", style=filled, fillcolor=\"#f9f9f9\"];\n\n";

        // 1. Output all the Blocks (Nodes)
        for (const auto &block : graph.get_all_blocks())
        {
            out << "    block" << block.get_id() << " [label=\"Block " << block.get_id() << "\\l";
            out << "---------------------------------\\l";

            if (block.get_code().length() > 0)
            {
                std::stringstream ss;
                // Reuse your existing hex-dump printer!
                print(block.get_code(), ss);
                std::string code_str = ss.str();

                // Graphviz strings require special escaping.
                // \l means "left-aligned newline" in DOT syntax.
                for (char c : code_str)
                {
                    if (c == '\n')
                        out << "\\l";
                    else if (c == '\"')
                        out << "\\\"";
                    else if (c == '\\')
                        out << "\\\\";
                    else
                        out << c;
                }
            }
            else
            {
                out << "(Empty)\\l";
            }
            out << "\"];\n";
        }

        out << "\n    // 2. Output all the Edges\n";
        for (const auto &block : graph.get_all_blocks())
        {
            for (auto succ : block.get_successors())
            {
                out << "    block" << block.get_id() << " -> block" << succ << ";\n";
            }
        }
        out << "}\n";
    }

}
