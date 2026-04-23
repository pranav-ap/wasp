#include "Doctor.h"
#include "Objects.h"
#include "VM.h"

#include <cstddef>
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

// =========================================================================
// CLASS
// ======================================================================

void VM::execute_build_overload_group(CallFrame* frame)
{
    // How many method overloads are in this group?
    int count = static_cast<int>(std::to_integer<int>(frame->consume_byte()));
    ObjectVector overloads = pop_n_from_stack(count);

    auto group = make_object(std::make_shared<ObjectOverloadList>(std::move(overloads)));
    push_to_stack(group);
}

void VM::execute_build_class(CallFrame* frame)
{
    int method_count = static_cast<int>(std::to_integer<int>(frame->consume_byte()));
    int fields_count = static_cast<int>(std::to_integer<int>(frame->consume_byte()));

    ObjectVector methods = pop_n_from_stack(method_count);

    auto class_blueprint = make_object(
        std::make_shared<ClassBlueprintObject>(std::move(methods), fields_count)
    );

    push_to_stack(class_blueprint);
}

void VM::execute_instantiate(CallFrame* frame)
{
    int num_fields = static_cast<int>(std::to_integer<int>(frame->consume_byte()));

    Object_ptr blueprint_obj = pop_from_stack();
    ObjectVector fields = pop_n_from_stack(num_fields);

    Doctor::get().assert(
        blueprint_obj->is<std::shared_ptr<ClassBlueprintObject>>(),
        WaspStage::VM,
        "OpCode::INSTANTIATE expects a ClassBlueprintObject on the stack!"
    );

    auto blueprint = blueprint_obj->as<std::shared_ptr<ClassBlueprintObject>>();

    ObjectVector instance_memory = std::move(fields);
    instance_memory
        .insert(instance_memory.end(), blueprint->members.begin(), blueprint->members.end());

    auto instance = make_object(std::make_shared<InstanceObject>(std::move(instance_memory)));

    push_to_stack(instance);
}

// --------------------------------------
// FUNCTIONS & METHODS
// --------------------------------------

void VM::execute_make_function(CallFrame* frame)
{
    // How many upvalues to capture?
    int upvalue_count = static_cast<int>(frame->consume_byte());

    // FunctionBlueprintObject is on the stack
    Object_ptr blueprint_obj = pop_from_stack();

    Doctor::get().assert(
        blueprint_obj->is<std::shared_ptr<FunctionBlueprintObject>>(),
        WaspStage::VM,
        "MAKE_FUNCTION expects a FunctionBlueprintObject on the stack"
    );

    auto blueprint = blueprint_obj->as<std::shared_ptr<FunctionBlueprintObject>>();

    // Capture the variables

    ObjectVector captured_upvalues;
    captured_upvalues.reserve(upvalue_count);

    for (int i = 0; i < upvalue_count; i++)
    {
        bool is_local_to_parent = (frame->consume_byte() == std::byte{1});
        int slot_or_index = static_cast<int>(frame->consume_byte());

        if (is_local_to_parent)
        {
            size_t absolute_idx = frame->base_pointer + slot_or_index;

            Doctor::get().assert(
                absolute_idx < stack.size(),
                WaspStage::VM,
                "Closure attempted to capture an invalid stack slot: " +
                    std::to_string(slot_or_index)
            );

            captured_upvalues.push_back(stack[absolute_idx]);
        }
        else
        {
            captured_upvalues.push_back(frame->function->upvalues[slot_or_index]);
        }
    }

    auto runtime_closure = std::make_shared<FunctionRuntimeObject>(
        blueprint,
        std::move(captured_upvalues)
    );

    push_to_stack(make_object(runtime_closure));
}

void VM::execute_overload_function(CallFrame* frame)
{
    // The operand is now the physical slot index (0, 1, 2...)
    int slot_index = static_cast<int>(std::to_integer<int>(frame->consume_byte()));

    Object_ptr new_func = pop_from_stack();

    size_t absolute_idx = frame->base_pointer + slot_index;

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
            std::make_shared<ObjectOverloadList>(std::move(initial_overloads))
        );

        // Store the group container directly in its assigned slot
        stack[absolute_idx] = group;
    }
    else
    {
        Doctor::get().assert(
            existing_obj->is<std::shared_ptr<ObjectOverloadList>>(),
            WaspStage::VM,
            "Cannot add overload to a slot that contains a non-function object."
        );

        auto group = existing_obj->as<std::shared_ptr<ObjectOverloadList>>();

        // Simply append the new version to the existing set
        group->overloads.push_back(new_func);
    }
}

void VM::execute_resolve_function(CallFrame* frame)
{
    int overload_index = static_cast<int>(frame->consume_byte());

    Object_ptr group_obj = pop_from_stack();

    Doctor::get().assert(
        group_obj->is<std::shared_ptr<ObjectOverloadList>>(),
        WaspStage::VM,
        "RESOLVE_FUNCTION expects an Overload Group on the stack!"
    );

    auto group = group_obj->as<std::shared_ptr<ObjectOverloadList>>();

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
            [&](std::shared_ptr<FunctionRuntimeObject>& func)
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
                ObjectVector args = pop_n_from_stack(arg_count);

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

    // 'bp' points to arg1. 'bp - 1' is the FunctionRuntimeObject (the
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
} // namespace Wasp
