#include "InstructionPrinter.h"
#include "CFGraph.h"
#include "Doctor.h"
#include "Objects.h"
#include "OpCode.h"

#include <cstddef>
#include <iomanip>
#include <ios>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>

#define OPCODE_WIDTH 18
#define OPERAND_WIDTH 5
#define METADATA_WIDTH 15

using std::byte;
using std::setw;
using std::string;

namespace Wasp
{

void InstructionPrinter::print_pool_functions(std::ostream& out)
{
    out << "\n";
    out << "  ╭──────────────────────────────────────────────────────────╮\n";
    out << "  │                 CONSTANT POOL FUNCTIONS                  │\n";
    out << "  ╰──────────────────────────────────────────────────────────╯\n\n";

    bool found_functions = false;

    for (size_t i = 0; i < ws->pool->get_size(); i++)
    {
        auto obj = ws->pool->get(i);

        if (obj && obj->is<std::shared_ptr<StaticFunctionObject>>())
        {
            auto func_obj = obj->as<std::shared_ptr<StaticFunctionObject>>();
            found_functions = true;

            out << "  ┌── [ Constant Pool ID : " << std::right << setw(3) << std::setfill('0') << i
                << std::setfill(' ') << " ] ────────────────────┐\n";

            out << "  │ Name : " << std::left << setw(41) << func_obj->name << " │\n";

            out << "  └──────────────────────────────────────────────────┘\n";

            print(func_obj, out);
            out << "\n";
        }
    }

    if (!found_functions)
    {
        out << "  [ No static function objects found in the constant pool ]\n\n";
    }
}

string InstructionPrinter::stringify_instruction(byte opcode, const std::string& comment)
{
    std::stringstream ss;
    ss << std::left << setw(OPCODE_WIDTH) << stringify_opcode(opcode);

    if (!comment.empty())
    {
        ss << setw(OPERAND_WIDTH + METADATA_WIDTH + 2) << " " << " ; " << comment;
    }

    return ss.str();
}

string InstructionPrinter::stringify_instruction(
    byte opcode,
    byte operand,
    const std::string& comment
)
{
    int op_int = std::to_integer<int>(operand);
    OpCode op = static_cast<OpCode>(opcode);
    std::stringstream ss;

    ss << std::left << setw(OPCODE_WIDTH) << stringify_opcode(opcode) << " ";
    ss << std::right << setw(OPERAND_WIDTH) << op_int << " ";

    std::string metadata = "";

    switch (op)
    {
        break;
    case OpCode::GET_LOCAL:
    case OpCode::SET_LOCAL:
        metadata = "stack index";
        break;
    case OpCode::CALL:
        metadata = "args";
        break;
    case OpCode::EXIT_MODULE:
        metadata = "exports";
        break;
    case OpCode::IMPORT_MODULE:
        metadata = "module id";
        break;
    default:
        break;
    }

    ss << std::left << setw(METADATA_WIDTH) << metadata;

    if (!comment.empty())
    {
        ss << " ; " << comment;
    }

    return ss.str();
}

string InstructionPrinter::stringify_instruction(
    byte opcode,
    byte op1,
    byte op2,
    const std::string& comment
)
{
    OpCode op = static_cast<OpCode>(opcode);
    std::stringstream ss;

    ss << std::left << setw(OPCODE_WIDTH) << stringify_opcode(opcode) << " ";

    if (op == OpCode::JUMP || op == OpCode::JUMP_IF_FALSE || op == OpCode::LOOP_ITER)
    {
        int target_offset = std::to_integer<int>(op1) | (std::to_integer<int>(op2) << 8);
        ss << std::right << setw(OPERAND_WIDTH) << target_offset << " ";
        ss << std::left << setw(METADATA_WIDTH) << " ";
    }
    else
    {
        int op1_int = std::to_integer<int>(op1);
        int op2_int = std::to_integer<int>(op2);
        ss << std::right << setw(OPERAND_WIDTH) << op1_int << " ";
        ss << std::left << setw(METADATA_WIDTH) << op2_int;
    }

    if (!comment.empty())
    {
        ss << " ; " << comment;
    }

    return ss.str();
}

void InstructionPrinter::print_bytecode(const CodeObject& code, std::ostream& out)
{
    int length = static_cast<int>(code.length());
    const byte* data = code.data();

    for (int index = 0; index < length;)
    {
        byte opcode = data[index];
        OpCode op = static_cast<OpCode>(opcode);
        std::string comment = code.get_comment_at(index);

        // Print cleanly formatted index (e.g., "   0012 │ ")
        out << "   " << std::right << setw(4) << std::setfill('0') << index << std::setfill(' ')
            << " │ ";

        if (op == OpCode::MAKE_FUNCTION)
        {
            int upvalue_count = std::to_integer<int>(data[index + 1]);

            out << std::left << setw(OPCODE_WIDTH) << stringify_opcode(opcode) << " " << std::right
                << setw(OPERAND_WIDTH) << upvalue_count << " " << std::left << setw(METADATA_WIDTH)
                << ("(" + std::to_string(upvalue_count) + " upvalues)") << "\n";

            int capture_offset = index + 2;
            for (int i = 0; i < upvalue_count; i++)
            {
                bool is_local = std::to_integer<int>(data[capture_offset + (i * 2)]) == 1;
                int upv_idx = std::to_integer<int>(data[capture_offset + (i * 2) + 1]);

                // Perfectly align capture logic under the standard instruction flow
                out << "        │ " << std::left << setw(OPCODE_WIDTH)
                    << (is_local ? "CAPTURE_LOCAL" : "CAPTURE_UPVAL") << " " << std::right
                    << setw(OPERAND_WIDTH) << upv_idx << "\n";
            }

            index += 2 + (upvalue_count * 2);
            continue;
        }

        auto instruction = code.instruction_at(index);
        int arity = get_opcode_arity(op);

        if (arity == 0)
        {
            out << stringify_instruction(opcode, comment) << "\n";
        }
        else if (arity == 1)
        {
            out << stringify_instruction(opcode, instruction[1], comment) << "\n";
        }
        else if (arity == 2)
        {
            out << stringify_instruction(opcode, instruction[1], instruction[2], comment) << "\n";
        }

        index += static_cast<int>(instruction.size());
    }
}

void InstructionPrinter::print(const Object_ptr obj, std::ostream& out)
{
    Doctor::get().fatal_if_nullptr(obj, WaspStage::Compiler, "Cannot print a null object");

    Doctor::get().assert(
        obj->is<StaticFunctionObject_ptr>(),
        WaspStage::Compiler,
        "Can only print StaticFunctionObjects"
    );

    print(obj->as<StaticFunctionObject_ptr>(), out);
}

void InstructionPrinter::print(const StaticFunctionObject_ptr function_obj, std::ostream& out)
{
    print_bytecode(function_obj->code, out);
}

void InstructionPrinter::print(const CodeObject& code_object, std::ostream& out)
{
    print_bytecode(code_object, out);
}

} // namespace Wasp
