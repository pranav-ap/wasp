#include "InstructionPrinter.h"
#include <iostream>
#include <iomanip>
#include <sstream>

#ifndef ASSERT
#include <cassert>
#define ASSERT(condition, message) assert((condition) && message)
#endif

#define OPCODE_WIDTH 10
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
        int op1_int = std::to_integer<int>(op1);
        int op2_int = std::to_integer<int>(op2);
        std::stringstream ss;

        ss << std::left << setw(OPCODE_WIDTH) << stringify_opcode(opcode)
           << " " << op1_int << " " << op2_int;

        switch (static_cast<OpCode>(opcode))
        {
        case OpCode::CALL:
        {
            string name = name_map.contains(op1_int) ? name_map.at(op1_int) : "unknown";
            ss << " (fn " << name << ", " << op2_int << " args)";
            break;
        }
        default:
            break;
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
}