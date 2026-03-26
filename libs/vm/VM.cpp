#include "VM.h"
#include "Doctor.h"
#include "Objects.h"
#include "OpCode.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{
void VM::execute_constant(OpCode op, CallFrame* frame)
{
    switch (op)
    {
    case OpCode::LOAD_CONST: {
        push_to_stack(
            workspace->pool->get(static_cast<int>(frame->consume_byte()))
        );
        break;
    }
    case OpCode::LOAD_TRUE: {
        push_to_stack(workspace->pool->get_true_object());
        break;
    }
    case OpCode::LOAD_FALSE: {
        push_to_stack(workspace->pool->get_false_object());
        break;
    }
    case OpCode::LOAD_NONE: {
        push_to_stack(workspace->pool->get_none_object());
        break;
    }
    default:
        break;
    }
}
void VM::execute_variable(OpCode op, CallFrame* frame)
{
    switch (op)
    {
    case OpCode::SET_LOCAL: {
        int slot_index = static_cast<int>(std::to_integer<int>(frame->consume_byte()));

        Doctor::get().assert(
            frame->base_pointer + slot_index < stack.size(),
            WaspStage::VM,
            "Stack overflow: Assignment to invalid local slot!"
        );

        stack[frame->base_pointer + slot_index] = peek_tos();
        break;
    }

    case OpCode::GET_LOCAL: {
        int slot_index = static_cast<int>(std::to_integer<int>(frame->consume_byte()));

        Doctor::get().assert(
            frame->base_pointer + slot_index < stack.size(),
            WaspStage::VM,
            "Stack underflow: Read from invalid local slot!"
        );

        push_to_stack(stack[frame->base_pointer + slot_index]);
        break;
    }

    case OpCode::GET_NATIVE: {
        int index = static_cast<int>(std::to_integer<int>(frame->consume_byte()));
        push_to_stack(workspace->native_registry->get_native_object(index));
        break;
    }

    case OpCode::GET_UPVALUE: {
        int index = static_cast<int>(std::to_integer<int>(frame->consume_byte()));
        push_to_stack(frame->function->upvalues[index]);
        break;
    }

    case OpCode::SET_UPVALUE: {
        int index = static_cast<int>(std::to_integer<int>(frame->consume_byte()));
        frame->function->upvalues[index] = peek_tos();
        break;
    }

    default:
        break;
    }
}

void VM::execute_scope_op(OpCode op, CallFrame* frame)
{
    switch (op)
    {
    case OpCode::PUSH_SCOPE: {
        frame->scope_bases.push_back(stack.size());
        break;
    }
    case OpCode::POP_SCOPE: {
        size_t base = frame->scope_bases.back();
        frame->scope_bases.pop_back();

        while (stack.size() > base)
        {
            pop_from_stack();
        }

        break;
    }
    default:
        break;
    }
}

void VM::execute_control_flow(OpCode op, CallFrame* frame)
{
    uint8_t low = static_cast<uint8_t>(frame->consume_byte());
    uint8_t high = static_cast<uint8_t>(frame->consume_byte());
    uint16_t target_ip = low | (high << 8);

    if (op == OpCode::JUMP ||
        (op == OpCode::JUMP_IF_FALSE && !is_truthy(peek_tos())))
    {
        frame->ip = target_ip;
    }
}

// ----------------------------------------------
// Member Access
// ----------------------------------------------

void VM::execute_member(OpCode op, CallFrame* frame)
{
    int member_index = static_cast<int>(frame->consume_byte());

    if (op == OpCode::GET_MEMBER)
    {
        Object_ptr obj = pop_from_stack();

        Doctor::get().fatal_if_nullptr(
            obj,
            WaspStage::VM,
            "Cannot read property of null."
        );

        push_to_stack(perform_get_member(obj, member_index));
    }
    else if (op == OpCode::SET_MEMBER)
    {
        Object_ptr val = pop_from_stack();
        Object_ptr obj = pop_from_stack();

        Doctor::get().fatal_if_nullptr(
            obj,
            WaspStage::VM,
            "Cannot set property on null."
        );

        perform_set_member(obj, member_index, val);

        // Put the value back on the stack so expression cleanups (POP)
        // work correctly
        push_to_stack(val);
    }
}

Object_ptr VM::perform_get_member(Object_ptr obj, int member_index)
{
    return std::visit(
        overloaded{
            [&](std::shared_ptr<ModuleObject>& mod) -> Object_ptr
            {
                return mod->get_member(member_index);
            },
            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::VM,
                    "Object of this type does not support reading "
                    "properties."
                );
                return nullptr;
            }
        },
        obj->value
    );
}

void VM::perform_set_member(Object_ptr obj, int member_index, Object_ptr value)
{
    std::visit(
        overloaded{
            [&](std::shared_ptr<ModuleObject>& mod)
            {
                mod->set_member(member_index, value);
            },
            [&](auto&)
            {
                Doctor::get().fatal(
                    WaspStage::VM,
                    "Object does not support setting properties."
                );
            }
        },
        obj->value
    );
}

// --------------------------------------
// Function Calls
// --------------------------------------

void VM::execute_make_function(CallFrame* frame)
{
    // 1. How many upvalues to capture?
    int upvalue_count = static_cast<int>(frame->consume_byte());

    // 2. The StaticFunctionObject (blueprint) is on the stack
    Object_ptr blueprint_obj = pop_from_stack();

    Doctor::get().assert(
        blueprint_obj->is<std::shared_ptr<StaticFunctionObject>>(),
        WaspStage::VM,
        "MAKE_FUNCTION expects a StaticFunctionObject on the stack"
    );

    auto blueprint = blueprint_obj->as<std::shared_ptr<StaticFunctionObject>>();

    ObjectVector captured_upvalues;
    captured_upvalues.reserve(upvalue_count);

    // 3. Capture the variables
    for (int i = 0; i < upvalue_count; i++)
    {
        bool is_local_to_parent = (frame->consume_byte() == std::byte{1});
        int slot_or_index = static_cast<int>(frame->consume_byte());

        if (is_local_to_parent)
        {
            // THE FIX: The compiler now emits the physical slot index relative
            // to the current frame's base pointer.
            size_t absolute_idx = frame->base_pointer + slot_or_index;

            Doctor::get().assert(
                absolute_idx < stack.size(),
                WaspStage::VM,
                "Closure attempted to capture an invalid stack slot: " +
                    std::to_string(slot_or_index)
            );

            // We grab the value directly from the stack
            captured_upvalues.push_back(stack[absolute_idx]);
        }
        else
        {
            // Capturing an upvalue that the current function already holds.
            // This was already index-based, so it stays simple.
            captured_upvalues.push_back(frame->function->upvalues[slot_or_index]);
        }
    }

    // 4. Create the runtime closure with the actual captured values
    auto runtime_closure = std::make_shared<RuntimeFunctionObject>(
        blueprint,
        std::move(captured_upvalues)
    );

    push_to_stack(make_object(runtime_closure));
}

void VM::execute_overload_function(CallFrame* frame)
{
    // The operand is now the physical slot index (0, 1, 2...)
    int slot_index = static_cast<int>(std::to_integer<int>(frame->consume_byte()));

    // The new function version (pushed by LOAD_CONST or MAKE_FUNCTION)
    Object_ptr new_func = pop_from_stack();

    size_t absolute_idx = frame->base_pointer + slot_index;

    // Ensure the stack is physically large enough to hold this slot
    // (Usually handles the very first definition in a module/function)
    if (absolute_idx >= stack.size())
    {
        stack.resize(absolute_idx + 1, nullptr);
    }

    Object_ptr existing_obj = stack[absolute_idx];

    if (existing_obj == nullptr)
    {
        // First version of this function: Create the Overload Group container
        ObjectVector initial_overloads;
        initial_overloads.push_back(new_func);

        auto group = make_object(
            std::make_shared<OverloadedObjectsSet>(std::move(initial_overloads))
        );

        // Store the group container directly in its assigned slot
        stack[absolute_idx] = group;
    }
    else
    {
        // The group already exists in this slot!
        Doctor::get().assert(
            existing_obj->is<std::shared_ptr<OverloadedObjectsSet>>(),
            WaspStage::VM,
            "Cannot add overload to a slot that contains a non-function object."
        );

        auto group = existing_obj->as<std::shared_ptr<OverloadedObjectsSet>>();

        // Simply append the new version to the existing set
        group->overloads.push_back(new_func);
    }
}

void VM::execute_resolve_function(CallFrame* frame)
{
    int overload_index = static_cast<int>(frame->consume_byte());

    Object_ptr group_obj = pop_from_stack();

    Doctor::get().assert(
        group_obj->is<std::shared_ptr<OverloadedObjectsSet>>(),
        WaspStage::VM,
        "RESOLVE_FUNCTION expects an Overload Group on the stack!"
    );

    auto group = group_obj->as<std::shared_ptr<OverloadedObjectsSet>>();

    Doctor::get().assert(
        overload_index >= 0 && overload_index < group->overloads.size(),
        WaspStage::VM,
        "Overload index out of bounds!"
    );

    push_to_stack(group->overloads[overload_index]);
}

void VM::execute_call(CallFrame* frame)
{
    int arg_count = static_cast<int>(std::to_integer<int>(frame->consume_byte()));

    // The callable sits just below the arguments on the stack
    Object_ptr callable = peek_tos(arg_count);

    std::visit(
        overloaded{
            [&](std::shared_ptr<RuntimeFunctionObject>& func)
            {
                // The new base pointer points to the first argument.
                // Slot 0 in the new function will now physically point to Argument 0.
                size_t new_base_pointer = stack.size() - arg_count;

                // Create the new frame. No symbol mapping required!
                frames.emplace_back(func, new_base_pointer);

                // On the next VM cycle, it will start executing the first
                // instruction of 'func' using the arguments already on the stack.
            },

            [&](std::shared_ptr<NativeFunctionObject>& native)
            {
                Doctor::get().assert(
                    native->arity == -1 || native->arity == arg_count,
                    WaspStage::VM,
                    "Arity mismatch in native function call: expected " +
                        std::to_string(native->arity) + " but got " + std::to_string(arg_count)
                );

                // Collect arguments
                std::vector<Object_ptr> args(arg_count);
                for (int i = arg_count - 1; i >= 0; i--)
                {
                    args[i] = pop_from_stack();
                }

                // Remove the native function object itself
                pop_from_stack();

                // Execute and push result
                Object_ptr result = native->function(args);
                push_to_stack(result);
            },

            [](auto&)
            {
                Doctor::get().fatal(WaspStage::VM, "Attempted to call a non-callable object");
            }
        },
        callable->value
    );
}

void VM::execute_return(CallFrame* frame)
{
    Object_ptr result = pop_from_stack();

    size_t bp = frame->base_pointer;

    frames.pop_back();

    // 'bp' points to arg1. 'bp - 1' is the RuntimeFunctionObject (the
    // callable). Remove the callable, all arguments, and all local
    // variables.
    if (bp > 0)
    {
        stack.erase(stack.begin() + (bp - 1), stack.end());
    }
    else
    {
        // Fallback for top-level module returns
        stack.clear();
    }

    // Push the return value back onto the caller's stack
    push_to_stack(result);
}

// ------------------------------------------------
// Module
// ------------------------------------------------

void VM::execute_import_module(CallFrame* frame)
{
    int module_index = static_cast<int>(frame->consume_byte());

    auto target_module = workspace->get_module(module_index);

    Doctor::get().fatal_if_nullptr(
        target_module,
        WaspStage::VM,
        "Module not found at registry index: " + std::to_string(module_index)
    );

    auto module_func = std::make_shared<RuntimeFunctionObject>(
        target_module->blueprint
    );

    size_t stack_base_pointer = stack.size();
    frames.emplace_back(module_func, stack_base_pointer);
}

void VM::execute_exit_module(CallFrame* frame)
{
    int export_count = static_cast<int>(frame->consume_byte());
    ObjectVector exported_members(export_count);

    for (int i = export_count - 1; i >= 0; i--)
    {
        exported_members[i] = pop_from_stack();
    }

    auto exports = std::make_shared<ModuleObject>(
        frame->function->blueprint->name,
        std::move(exported_members)
    );

    // Cleanup the frame
    size_t bp = frame->base_pointer;
    stack.erase(stack.begin() + bp, stack.end());
    frames.pop_back();

    // Make module object available to importer
    push_to_stack(make_object(exports));
}

// --------------------------------------------------------------
// Main Execution Loop
// --------------------------------------------------------------

void VM::run(StaticFunctionObject_ptr function_object)
{
    frames.emplace_back(
        std::make_shared<RuntimeFunctionObject>(function_object),
        0
    );

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

        case OpCode::LOAD_CONST:
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

        case OpCode::PUSH_SCOPE:
        case OpCode::POP_SCOPE: {
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

        case OpCode::MAKE_FUNCTION: {
            execute_make_function(frame);
            break;
        }

        case OpCode::OVERLOAD_FUNCTION: {
            execute_overload_function(frame);
            break;
        }

        case OpCode::RESOLVE_FUNCTION: {
            execute_resolve_function(frame);
            break;
        }

        case OpCode::CALL: {
            execute_call(frame);
            break;
        }

        case OpCode::RETURN: {
            execute_return(frame);

            if (frames.empty())
            {
                return;
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
