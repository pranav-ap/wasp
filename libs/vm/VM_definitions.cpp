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

void VM::execute_build_overload_group(CallFrame* frame)
{
    int count = std::to_integer<int>(frame->consume_byte());
    ObjectVector overloads = pop_n_from_stack(count);

    auto group = make_object(std::make_shared<ObjectOverloadList>(std::move(overloads)));
    push_to_stack(group);
}

void VM::execute_build_class(CallFrame* frame)
{
    int num_methods = static_cast<int>(frame->consume_byte());
    int num_fields = static_cast<int>(frame->consume_byte());

    Object_ptr blueprint_obj = pop_from_stack();
    Object_ptr class_type_obj = pop_from_stack();
    ObjectVector methods = pop_n_from_stack(num_methods);

    auto blueprint = blueprint_obj->as<std::shared_ptr<ClassBlueprintObject>>();
    auto class_type = class_type_obj->as<std::shared_ptr<ClassType>>();

    blueprint->members = std::move(methods);
    blueprint->fields_count = num_fields;
    blueprint->itables = class_type->itables;

    push_to_stack(blueprint_obj);
}

void VM::execute_instantiate(CallFrame* frame)
{
    int num_fields = std::to_integer<int>(frame->consume_byte());

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

    auto instance = make_object(
        std::make_shared<ClassInstanceObject>(blueprint, std::move(instance_memory))
    );

    push_to_stack(instance);
}

void VM::execute_make_function(CallFrame* frame)
{
    int upvalue_count = std::to_integer<int>(frame->consume_byte());
    Object_ptr blueprint_obj = pop_from_stack();

    Doctor::get().assert(
        blueprint_obj->is<std::shared_ptr<FunctionBlueprintObject>>(),
        WaspStage::VM,
        "MAKE_FUNCTION expects a FunctionBlueprintObject on the stack"
    );

    auto blueprint = blueprint_obj->as<std::shared_ptr<FunctionBlueprintObject>>();
    ObjectVector captured_upvalues;
    captured_upvalues.reserve(upvalue_count);

    for (int i = 0; i < upvalue_count; i++)
    {
        bool is_local_to_parent = (frame->consume_byte() == std::byte{1});
        int slot_or_index = std::to_integer<int>(frame->consume_byte());

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

void VM::execute_store_function_overload(CallFrame* frame)
{
    int slot_index = std::to_integer<int>(frame->consume_byte());
    Object_ptr new_func = pop_from_stack();
    size_t absolute_idx = frame->base_pointer + slot_index;

    if (absolute_idx >= stack.size())
    {
        stack.resize(absolute_idx + 1, nullptr);
    }

    Object_ptr existing_obj = stack[absolute_idx];

    if (existing_obj == nullptr)
    {
        ObjectVector initial_overloads;
        initial_overloads.push_back(new_func);

        auto group = make_object(
            std::make_shared<ObjectOverloadList>(std::move(initial_overloads))
        );

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
        group->overloads.push_back(new_func);
    }
}

void VM::execute_resolve_function(CallFrame* frame)
{
    int overload_index = std::to_integer<int>(frame->consume_byte());
    Object_ptr obj = pop_from_stack();

    if (obj->is<std::shared_ptr<NativeFunctionObject>>())
    {
        push_to_stack(obj);
        return;
    }

    Doctor::get().assert(
        obj->is<std::shared_ptr<ObjectOverloadList>>(),
        WaspStage::VM,
        "RESOLVE_FUNCTION expects an Overload Group or Function on the stack!"
    );

    auto group = obj->as<std::shared_ptr<ObjectOverloadList>>();

    Doctor::get().assert(
        overload_index >= 0 && overload_index < static_cast<int>(group->overloads.size()),
        WaspStage::VM,
        "Overload index out of bounds!"
    );

    push_to_stack(group->overloads[overload_index]);
}

void VM::execute_call(CallFrame* frame)
{
    int arg_count = std::to_integer<int>(frame->consume_byte());
    Object_ptr callable = peek_tos(arg_count);

    std::visit(
        overloaded{
            [&](std::shared_ptr<FunctionRuntimeObject>& func)
            {
                size_t new_base_pointer = stack.size() - arg_count;
                frames.emplace_back(func, new_base_pointer);
            },
            [&](std::shared_ptr<NativeFunctionObject>& native)
            {
                ObjectVector args = pop_n_from_stack(arg_count);
                pop_from_stack();
                push_to_stack(native->function(args));
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

    if (bp > 0)
    {
        stack.erase(stack.begin() + (bp - 1), stack.end());
    }
    else
    {
        stack.clear();
    }

    push_to_stack(result);
}
} // namespace Wasp
