#include "VM.h"
#include "Doctor.h"
#include "Objects.h"
#include "OpCode.h"

#include <cstddef>
#include <iostream>
#include <memory>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

// --------------------------------------------------------------
// Main Execution Loop
// --------------------------------------------------------------

void VM::run(FunctionBlueprintObject_ptr function_object)
{
    frames.emplace_back(std::make_shared<FunctionRuntimeObject>(function_object), 0);

    while (true)
    {
        CallFrame* frame = &frames.back();
        OpCode instruction = static_cast<OpCode>(frame->consume_byte());

        switch (instruction)
        {
            // Lifecycle

        case OpCode::NO_OP:
            break;

        case OpCode::ENTER_WORKSPACE:
            break;

        case OpCode::EXIT_WORKSPACE: {
            frames.pop_back();
            break;
        }

        case OpCode::ENTER_MODULE:
            break;

        case OpCode::EXIT_MODULE: {
            execute_exit_module(frame);
            // If you exit from the main module, stop the VM.
            if (frames.empty())
                return;

            break;
        }

        case OpCode::IMPORT_MODULE: {
            execute_import_module(frame);
            break;
        }

        case OpCode::HALT: {
            std::cerr << "Execution halted by HALT instruction.\n";
            return;
        }
            // Stack Manipulations

        case OpCode::POP:
        case OpCode::DUP: {
            execute_stack_op(instruction);
            break;
        }
            // Constants

        case OpCode::LOAD_CONSTANT:
        case OpCode::LOAD_TRUE:
        case OpCode::LOAD_FALSE:
        case OpCode::LOAD_NONE: {
            execute_constant(instruction, frame);
            break;
        }
            // Variables & Scope

        case OpCode::SET_LOCAL:
        case OpCode::GET_LOCAL:
        case OpCode::GET_NATIVE:
        case OpCode::GET_UPVALUE:
        case OpCode::SET_UPVALUE: {
            execute_variable(instruction, frame);
            break;
        }

        case OpCode::BUILD_LIST:
        case OpCode::BUILD_TUPLE:
        case OpCode::BUILD_SET:
        case OpCode::BUILD_MAP: {
            execute_build_collection(instruction, frame);
            break;
        }

        case OpCode::PUSH_SCOPE:
        case OpCode::POP_SCOPE:
        case OpCode::POP_SCOPE_KEEP_TOS: {
            execute_scope_op(instruction, frame);
            break;
        }

            // Members

        case OpCode::GET_MEMBER:
        case OpCode::SET_MEMBER: {
            execute_member(instruction, frame);
            break;
        }

            // Control Flow

        case OpCode::JUMP:
        case OpCode::JUMP_IF_FALSE: {
            execute_control_flow(instruction, frame);
            break;
        }

        case OpCode::GET_ITER:
        case OpCode::LOOP_ITER: {
            execute_iter(instruction, frame);
            break;
        }

            // Binary Math, Comparisons, Logic

        case OpCode::ADD:
        case OpCode::SUB:
        case OpCode::MUL:
        case OpCode::DIV:
        case OpCode::MOD:
        case OpCode::POW:
        case OpCode::EQ:
        case OpCode::NE:
        case OpCode::LT:
        case OpCode::LE:
        case OpCode::GT:
        case OpCode::GE:
        case OpCode::LOGICAL_AND:
        case OpCode::LOGICAL_OR: {
            execute_binary_op(instruction);
            break;
        }
            // Unary

        case OpCode::NEGATE:
        case OpCode::NOT: {
            execute_unary_op(instruction);
            break;
        }
            // FUNCTION

        case OpCode::BUILD_FUNCTION: {
            execute_BUILD_FUNCTION(frame);
            break;
        }

        case OpCode::STORE_FUNCTION_OVERLOAD: {
            execute_STORE_FUNCTION_OVERLOAD(frame);
            break;
        }

        case OpCode::GET_FUNCTION: {
            execute_GET_FUNCTION(frame);
            break;
        }

        case OpCode::GET_CLASS_METHOD: {
            execute_GET_CLASS_METHOD(frame);
            break;
        }

        case OpCode::GET_TRAIT_METHOD: {
            execute_GET_TRAIT_METHOD(frame);
            break;
        }
        case OpCode::CALL: {
            execute_CALL(frame);
            break;
        }

        case OpCode::RETURN: {
            execute_RETURN(frame);

            if (frames.empty())
            {
                return;
            }

            break;
        }

        case OpCode::BUILD_OVERLOAD_GROUP: {
            execute_BUILD_OVERLOAD_GROUP(frame);
            break;
        }

        case OpCode::PUSH_EMPTY_OVERLOAD_GROUP: {
            push_to_stack(
                make_object(std::make_shared<Pocket>(ObjectVector{}))
            );

            break;
        }
        case OpCode::BUILD_CLASS: {
            execute_BUILD_CLASS(frame);
            break;
        }

        case OpCode::INSTANTIATE: {
            execute_INSTANTIATE(frame);
            break;
        }

        case OpCode::BOX: {
            execute_BOX(frame);
            break;
        }

        case OpCode::UNPACK_MODULE_MEMBERS: {
            int module_slot = static_cast<int>(std::to_integer<int>(frame->consume_byte()));
            int count = static_cast<int>(std::to_integer<int>(frame->consume_byte()));

            Object_ptr module_obj = stack[frame->base_pointer + module_slot];

            // Loop through the remaining bytes to fetch each member
            for (int i = 0; i < count; i++)
            {
                int member_index = static_cast<int>(std::to_integer<int>(frame->consume_byte()));
                push_to_stack(execute_GET_MEMBER(module_obj, member_index));
            }
            break;
        }

        default: {
            Doctor::get().fatal(
                WaspStage::VM,
                "Unknown OpCode encountered: " + stringify_opcode(instruction)
            );
        }
        }
    }
}
} // namespace Wasp
