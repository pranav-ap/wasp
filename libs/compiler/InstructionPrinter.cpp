#include "InstructionPrinter.h"
#include <iostream>
#include <iomanip>
#include <sstream>

#ifndef ASSERT
#include <cassert>
#define ASSERT(condition, message) assert((condition) && message)
#endif

#define OPCODE_WIDTH 15
#define OPERAND_WIDTH 10

using std::byte;
using std::cout;
using std::setw;
using std::string;

namespace Wasp
{
    string InstructionPrinter::stringify_instruction(byte opcode, byte operand, const std::map<int, std::string> &names)
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
            if (names.contains(op_int))
            {
                ss << std::right << setw(OPERAND_WIDTH) << " (" << names.at(op_int) << ")";
            }
            break;
        case OpCode::CALL:
            ss << std::right << setw(OPERAND_WIDTH) << " (" << op_int << " args)";
            break;
        default:
            break;
        }
        return ss.str();
    }

    string InstructionPrinter::stringify_instruction(byte opcode, byte op1, byte op2, const std::map<int, std::string> &names)
    {
        OpCode op = static_cast<OpCode>(opcode);
        std::stringstream ss;

        if (op == OpCode::JUMP || op == OpCode::JUMP_IF_FALSE || op == OpCode::LOOP_ITER)
        {
            int target_offset = std::to_integer<int>(op1) | (std::to_integer<int>(op2) << 8);

            ss << std::left << setw(OPCODE_WIDTH) << stringify_opcode(opcode)
               << " " << target_offset;
        }
        else
        {
            int op1_int = std::to_integer<int>(op1);
            int op2_int = std::to_integer<int>(op2);

            ss << std::left << setw(OPCODE_WIDTH) << stringify_opcode(opcode)
               << " " << op1_int << " " << op2_int;
        }

        return ss.str();
    }

    void InstructionPrinter::print(const CodeObject &code_object, std::ostream &out)
    {
        out << "=========================================\n";
        out << " INSTRUCTIONS\n";
        out << "=========================================\n\n";

        int length = static_cast<int>(code_object.length());
        int index_width = std::to_string(length).size() + 2;

        // Grab direct access to the raw bytes to handle variable-length payloads
        const byte *data = code_object.data();

        for (int index = 0; index < length;)
        {
            byte opcode = data[index];
            OpCode op = static_cast<OpCode>(opcode);

            out << std::right << setw(index_width) << index << ": ";

            // --- SPECIAL CASE: Variable length MAKE_FUNCTION ---
            if (op == OpCode::MAKE_FUNCTION)
            {
                int upvalue_count = std::to_integer<int>(data[index + 1]);

                out << std::left << setw(OPCODE_WIDTH) << stringify_opcode(opcode) << " " << upvalue_count;
                out << std::right << setw(OPERAND_WIDTH) << " (" << upvalue_count << " upvalues)\n";

                // Print the trailing breadcrumbs nicely
                int capture_offset = index + 2;
                for (int i = 0; i < upvalue_count; i++)
                {
                    bool is_local = std::to_integer<int>(data[capture_offset + (i * 2)]) == 1;
                    int upv_idx = std::to_integer<int>(data[capture_offset + (i * 2) + 1]);

                    out << std::right << setw(index_width) << " " << "  | capture "
                        << (is_local ? "local " : "upvalue ") << upv_idx << "\n";
                }

                index += 2 + (upvalue_count * 2);
                continue;
            }

            // --- STANDARD CASE: Fixed arity instructions ---
            auto instruction = code_object.instruction_at(index);
            int arity = static_cast<int>(instruction.size()) - 1;

            if (arity == 0)
                out << stringify_opcode(opcode) << "\n";
            else if (arity == 1)
                out << stringify_instruction(opcode, instruction[1], code_object.local_names) << "\n";
            else if (arity == 2)
                out << stringify_instruction(opcode, instruction[1], instruction[2], code_object.local_names) << "\n";

            index += static_cast<int>(instruction.size());
        }
    }

    void InstructionPrinter::print(const CFGraph &graph, std::ostream &out)
    {
        out << "digraph CFG {\n";
        out << "    node [shape=box, fontname=\"Courier\", style=filled, fillcolor=\"#f9f9f9\"];\n\n";

        for (const auto &block : graph.get_all_blocks())
        {
            out << "    block" << block.get_id() << " [label=\"Block " << block.get_id() << "\\l";
            out << "---------------------------------\\l";

            if (block.get_code().length() > 0)
            {
                std::stringstream ss;
                print(block.get_code(), ss);
                std::string code_str = ss.str();

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

        out << "\n    // Edges\n";
        for (const auto &block : graph.get_all_blocks())
        {
            for (auto succ : block.get_successors())
            {
                out << "    block" << block.get_id() << " -> block" << succ << ";\n";
            }
        }
        out << "}\n";
    }

    void InstructionPrinter::print_pool(std::ostream &out)
    {
        if (!constant_pool)
            return;

        out << "\n=========================================\n";
        out << " CONSTANT POOL FUNCTIONS\n";
        out << "=========================================\n\n";

        for (size_t i = 0; i < constant_pool->get_size(); i++)
        {
            auto obj = constant_pool->get(i);
            if (obj && obj->is<std::shared_ptr<FunctionObject>>())
            {
                auto func_obj = obj->as<std::shared_ptr<FunctionObject>>();

                out << "--- Pool Index " << i << " (" << func_obj->code.name << ") ---\n";

                print(func_obj->code, out);

                out << "\n";
            }
        }
    }
}