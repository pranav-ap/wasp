#include "VM.h"
#include "CFGraph.h"
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

template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp {
void VM::execute_constant(OpCode op, CallFrame* frame) {
    switch (op) {
    case OpCode::LOAD_CONST:
        push_to_stack(workspace->pool->get(static_cast<int>(frame->consume_byte())));
        break;
    case OpCode::LOAD_TRUE:
        push_to_stack(workspace->pool->get_true_object());
        break;
    case OpCode::LOAD_FALSE:
        push_to_stack(workspace->pool->get_false_object());
        break;
    case OpCode::LOAD_NONE:
        push_to_stack(workspace->pool->get_none_object());
        break;
    default:
        break;
    }
}

void VM::execute_variable(OpCode op, CallFrame* frame) {
    switch (op) {
    case OpCode::DEFINE_LOCAL:
        frame->consume_byte();
        break;
    case OpCode::SET_LOCAL: {
        int index = static_cast<int>(frame->consume_byte());
        if (index >= stack.size())
            stack.resize(index + 1);
        stack[frame->base_pointer + index] = peek_tos();
        break;
    }
    case OpCode::GET_LOCAL: {
        int index = static_cast<int>(frame->consume_byte());
        push_to_stack(stack[frame->base_pointer + index]);
        break;
    }
    case OpCode::GET_NATIVE: {
        int index = static_cast<int>(frame->consume_byte());
        push_to_stack(workspace->native_registry->get_native_object(index));
        break;
    }
    case OpCode::PUSH_SCOPE:
        frame->scope_bases.push_back(stack.size());
        break;
    case OpCode::POP_SCOPE: {
        size_t base = frame->scope_bases.back();
        frame->scope_bases.pop_back();
        while (stack.size() > base)
            pop_from_stack();
        break;
    }
    default:
        break;
    }
}

void VM::execute_control_flow(OpCode op, CallFrame* frame) {
    uint8_t low = static_cast<uint8_t>(frame->consume_byte());
    uint8_t high = static_cast<uint8_t>(frame->consume_byte());
    uint16_t target_ip = low | (high << 8);

    if (op == OpCode::JUMP || (op == OpCode::JUMP_IF_FALSE && !is_truthy(peek_tos()))) {
        frame->ip = target_ip;
    }
}

// --------------------------------------
// Function Calls
// --------------------------------------

void VM::execute_make_function(CallFrame* frame) {
    int upvalue_count = static_cast<int>(frame->consume_byte());

    // Pop the function blueprint from the stack (put there by LOAD_CONST)
    Object_ptr blueprint_obj = pop_from_stack();
    Doctor::get().assert(
        blueprint_obj->is<std::shared_ptr<FunctionObject>>(),
        WaspStage::VM,
        "MAKE_FUNCTION expects a FunctionObject to pop"
    );

    auto blueprint = blueprint_obj->as<std::shared_ptr<FunctionObject>>();

    // Prepare the upvalues

    ObjectVector captured_upvalues;
    captured_upvalues.reserve(upvalue_count);

    for (int i = 0; i < upvalue_count; i++) {
        bool is_local = (frame->consume_byte() == std::byte{1});
        uint8_t index = static_cast<uint8_t>(frame->consume_byte());

        if (is_local) {
            captured_upvalues.push_back(stack[frame->base_pointer + index]);
        } else {
            captured_upvalues.push_back(frame->function->upvalues[index]);
        }
    }

    auto runtime_closure =
        std::make_shared<FunctionVMObject>(blueprint->code, std::move(captured_upvalues));

    Object_ptr closure_obj = std::make_shared<Object>(runtime_closure);
    push_to_stack(closure_obj);
}

void VM::execute_call(CallFrame* frame) {
    int arg_count = static_cast<int>(frame->consume_byte());
    Object_ptr callable = peek_tos(arg_count);

    std::visit(
        overloaded{
            [&](std::shared_ptr<FunctionVMObject>& func) {
                size_t new_base_pointer = stack.size() - arg_count;
                frames.emplace_back(func, new_base_pointer);
                // The main execution loop will automatically start reading
                // the new function's bytecode on the next iteration
            },

            [&](std::shared_ptr<NativeFunctionObject>& native) {
                Doctor::get().assert(
                    native->arity == -1 || native->arity == arg_count,
                    WaspStage::VM,
                    "Arity mismatch in native function call"
                );

                // Collect arguments from the stack
                std::vector<Object_ptr> args(arg_count);
                for (int i = arg_count - 1; i >= 0; i--) {
                    args[i] = pop_from_stack();
                }

                // Pop the native function object off the stack
                pop_from_stack();

                Object_ptr result = native->function(args);
                push_to_stack(result);
            },

            [](auto&) {
                Doctor::get().fatal(WaspStage::VM, "Attempted to call a non-callable object");
            }
        },
        callable->value
    );
}

void VM::execute_return(CallFrame* frame) {
    Object_ptr result = pop_from_stack();

    size_t bp = frame->base_pointer;

    frames.pop_back();

    // 'bp' points to arg1. 'bp - 1' is the FunctionVMObject (the callable).
    // Remove the callable, all arguments, and all local variables.
    if (bp > 0) {
        stack.erase(stack.begin() + (bp - 1), stack.end());
    } else {
        // Fallback for top-level module returns
        stack.clear();
    }

    // Push the return value back onto the caller's stack
    push_to_stack(result);
}

void VM::execute_import(CallFrame* frame) {
    int path_index = static_cast<int>(frame->consume_byte());
    Object_ptr path_obj = workspace->pool->get(path_index);
    std::string module_path = path_obj->as<std::string>();

    auto target_module = workspace->get_module(module_path);
    Doctor::get().fatal_if_nullptr(
        target_module, WaspStage::VM, "Module not found : " + module_path
    );

    // 3. Prepare a new execution context
    // We create a temporary VM object for this module's top-level code
    auto module_func = std::make_shared<FunctionVMObject>(target_module->code);

    // We need to know where the stack starts BEFORE the module runs
    // so we can capture everything it defines as "exports"
    size_t export_base = stack.size();

    // 4. Push the frame and let the VM run it
    // Note: We use a specific BP so the module doesn't mess with our current locals
    frames.emplace_back(module_func, export_base);

    // THE TRICK: Since we are inside the main while(true) loop,
    // simply pushing to 'frames' is enough. The next iteration
    // of the loop will automatically start executing the imported module.

    // However, we need a way to KNOW when the module finishes so we can
    // wrap its results into a ModuleObject.
    // We'll handle this in EXIT_MODULE or by checking frame depth.
}

void VM::execute_exit_module() {
    // 1. Capture the return value (the result of the last expression in the module)
    Object_ptr module_result = pop_from_stack();

    // 2. Access the current frame before we destroy it
    CallFrame& frame = frames.back();
    size_t bp = frame.base_pointer;

    // 3. Create the ModuleObject to represent this namespace
    auto exports = std::make_shared<ModuleObject>();

    // 4. Collect Global Exports
    // Everything between the base_pointer and the current stack top
    // represents the variables defined in this module's scope.
    for (size_t i = bp; i < stack.size(); ++i) {
        // You'll need a way to look up the name for this index.
        // Usually, this is stored in the FunctionVMObject's debug_name_map
        std::string var_name = frame.function->get_name_for_index(i - bp);
        if (!var_name.empty()) {
            exports->fields[var_name] = stack[i];
        }
    }

    // 5. Cleanup the stack
    // We remove the module's locals but keep the 'bp' slot if we need
    // to place the ModuleObject there.
    stack.erase(stack.begin() + bp, stack.end());

    // 6. Pop the frame
    frames.pop_back();

    // 7. Push the resulting ModuleObject back to the caller's stack
    // This allows the caller to do: GET_LOCAL <module> -> GET_MEMBER "func"
    push_to_stack(std::make_shared<Object>(exports));
}

void VM::run(CodeObject code) {
    frames.emplace_back(std::make_shared<FunctionVMObject>(code), 0);

    while (true) {
        CallFrame* frame = &frames.back();
        OpCode instruction = static_cast<OpCode>(frame->consume_byte());

        switch (instruction) {
            // Lifecycle

        case OpCode::NO_OP:
            break;

        case OpCode::ENTER_WORKSPACE:
            break;
        case OpCode::EXIT_WORKSPACE:
            frames.pop_back();
            break;

        case OpCode::IMPORT:
            execute_import(frame);
            break;

        case OpCode::ENTER_MODULE:
            break;

        case OpCode::EXIT_MODULE:
            execute_exit_module();
            // If you exit from the main module, stop the VM.
            if (frames.empty())
                return;

            break;

        case OpCode::HALT:
            std::cerr << "Execution halted by HALT instruction.\n";
            return;

            // Stack Manipulations

        case OpCode::POP:
        case OpCode::DUP:
            execute_stack_op(instruction);
            break;

            // Constants

        case OpCode::LOAD_CONST:
        case OpCode::LOAD_TRUE:
        case OpCode::LOAD_FALSE:
        case OpCode::LOAD_NONE:
            execute_constant(instruction, frame);
            break;

            // Variables & Scope

        case OpCode::DEFINE_LOCAL:
        case OpCode::SET_LOCAL:
        case OpCode::GET_LOCAL:
        case OpCode::GET_NATIVE:
        case OpCode::PUSH_SCOPE:
        case OpCode::POP_SCOPE:
            execute_variable(instruction, frame);
            break;

        // Members
        case OpCode::GET_MEMBER:
        case OpCode::SET_MEMBER:
            execute_member(instruction, frame);
            break;

            // Control Flow

        case OpCode::JUMP:
        case OpCode::JUMP_IF_FALSE:
            execute_control_flow(instruction, frame);
            break;

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
        case OpCode::LOGICAL_OR:
            execute_binary_op(instruction);
            break;

            // Unary

        case OpCode::NEGATE:
        case OpCode::NOT:
            execute_unary_op(instruction);
            break;

            // FUNCTION

        case OpCode::MAKE_FUNCTION:
            execute_make_function(frame);
            break;

        case OpCode::CALL:
            execute_call(frame);
            break;

        case OpCode::RETURN:
            execute_return(frame);
            break;

        default:
            std::cerr << "Unknown OpCode encountered : " << stringify_opcode(instruction) << std::endl;
            Doctor::get().fatal(WaspStage::VM, "Unknown OpCode encountered");
        }
    }
}
} // namespace Wasp
