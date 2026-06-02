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

// ----------------------------------------------
// Boxing (Trait Object Creation)
// ----------------------------------------------

void VM::execute_BOX(CallFrame* frame)
{
    int trait_count = static_cast<int>(frame->consume_byte());
    std::vector<int> trait_ids;

    for (int i = 0; i < trait_count; i++)
    {
        trait_ids.push_back(static_cast<int>(frame->consume_byte()));
    }

    Object_ptr instance_obj = pop_from_stack();
    auto class_instance = instance_obj->as<ClassInstance_ptr>();
    Doctor::get().fatal_if_nullptr(class_instance, WaspStage::VM);

    auto trait_obj = std::make_shared<TraitObject>(class_instance, trait_ids);
    push_to_stack(make_object(trait_obj));
}

// ----------------------------------------------
// FIELDS
// ----------------------------------------------

void VM::execute_GET_FIELD(CallFrame* frame)
{
    int member_index = static_cast<int>(frame->consume_byte());

    Object_ptr obj = pop_from_stack();
    Doctor::get().fatal_if_nullptr(obj, WaspStage::VM);

    std::visit(
        overloaded{
            [&](ClassInstance_ptr instance)
            {
                auto record = instance->record;
                push_to_stack(record->fields[member_index]);
            },
            [&](TraitObject_ptr trait_obj)
            {
                auto record = trait_obj->class_instance->record;
                push_to_stack(record->fields[member_index]);
            },
            [&](auto&)
            {
                Doctor::get().fatal(
                    WaspStage::VM,
                    "Object of this type does not support field access."
                );
            }
        },
        obj->value
    );
}

void VM::execute_SET_FIELD(CallFrame* frame)
{
    int member_index = static_cast<int>(frame->consume_byte());

    Object_ptr value = pop_from_stack();
    Doctor::get().fatal_if_nullptr(value, WaspStage::VM);

    Object_ptr obj = pop_from_stack();
    Doctor::get().fatal_if_nullptr(obj, WaspStage::VM);

    std::visit(
        overloaded{
            [&](ClassInstance_ptr instance)
            {
                auto record = instance->record;
                record->fields[member_index] = value;
            },
            [&](auto&)
            {
                Doctor::get().fatal(
                    WaspStage::VM,
                    "Object of this type does not support field assignment."
                );
            }
        },
        obj->value
    );
}

// ----------------------------------------------
// FUNCTION
// ----------------------------------------------

void VM::execute_GET_FUNCTION(CallFrame* frame)
{
    int overload_index = std::to_integer<int>(frame->consume_byte());
    Object_ptr obj = pop_from_stack();

    if (obj->is<NativeFunctionRuntimeObject_ptr>())
    {
        push_to_stack(obj);
        return;
    }

    Doctor::get().assert(
        obj->is<OverloadsSet_ptr>(),
        WaspStage::VM,
        "GET_FUNCTION expects an Overload Group or Function on the stack!"
    );

    auto group = obj->as<OverloadsSet_ptr>();

    Doctor::get().assert(
        overload_index >= 0 &&
            overload_index < static_cast<int>(group->overloads.size()),
        WaspStage::VM,
        "Overload index out of bounds!"
    );

    push_to_stack(group->overloads[overload_index]);
}

void VM::execute_GET_CLASS_METHOD(CallFrame* frame)
{
    int member_idx = static_cast<int>(frame->consume_byte());
    int overload_idx = static_cast<int>(frame->consume_byte());

    Object_ptr obj = pop_from_stack();

    auto instance = obj->as<ClassInstance_ptr>();
    Doctor::get().fatal_if_nullptr(
        instance,
        WaspStage::VM,
        "GET_CLASS_METHOD requires a ClassInstance"
    );

    auto overloads_set = instance->bag->get_overloads_set(member_idx);
    Doctor::get().fatal_if_nullptr(
        overloads_set,
        WaspStage::VM,
        "Method Overlaods Set not found"
    );

    auto method = overloads_set->get_overload(overload_idx);

    push_to_stack(method);
    push_to_stack(obj);
}

void VM::execute_GET_TRAIT_METHOD(CallFrame* frame)
{
    int trait_member_index = static_cast<int>(frame->consume_byte());
    int trait_overload_index = static_cast<int>(frame->consume_byte());

    Object_ptr boxed_obj = pop_from_stack();

    auto trait_obj = boxed_obj->as<TraitObject_ptr>();
    Doctor::get().fatal_if_nullptr(
        trait_obj,
        WaspStage::VM,
        "Expected boxed trait on stack."
    );

    OverloadCoordinate trait_coord{trait_member_index, trait_overload_index};

    Doctor::get().assert(
        trait_obj->itable.find(trait_coord) != trait_obj->itable.end(),
        WaspStage::VM,
        "Trait ITable is missing the requested overload coordinate."
    );

    OverloadCoordinate class_coord = trait_obj->itable.at(trait_coord);

    auto overloads_set = trait_obj->class_instance->bag->get_overloads_set(
        class_coord.member_index
    );
    Doctor::get()
        .fatal_if_nullptr(overloads_set, WaspStage::VM, "Method not found");

    auto method = overloads_set->get_overload(class_coord.overload_index);

    push_to_stack(method);
    push_to_stack(boxed_obj); // Push trait object as 'self'
}

void VM::execute_BUILD_FUNCTION(CallFrame* frame)
{
    int upvalue_count = std::to_integer<int>(frame->consume_byte());
    Object_ptr blueprint_obj = pop_from_stack();

    Doctor::get().assert(
        blueprint_obj->is<FunctionBlueprintObject_ptr>(),
        WaspStage::VM,
        "BUILD_FUNCTION expects a FunctionBlueprintObject on the stack"
    );

    auto blueprint = blueprint_obj->as<FunctionBlueprintObject_ptr>();
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
            captured_upvalues.push_back(
                frame->function->upvalues[slot_or_index]
            );
        }
    }

    auto runtime_closure = std::make_shared<FunctionRuntimeObject>(
        blueprint,
        std::move(captured_upvalues)
    );
    push_to_stack(make_object(runtime_closure));
}

void VM::execute_STORE_FUNCTION_OVERLOAD(CallFrame* frame)
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
            std::make_shared<OverloadsSet>(std::move(initial_overloads))
        );
        stack[absolute_idx] = group;
    }
    else
    {
        Doctor::get().assert(
            existing_obj->is<OverloadsSet_ptr>(),
            WaspStage::VM,
            "Cannot add overload to a slot that contains a non-function object."
        );

        auto group = existing_obj->as<OverloadsSet_ptr>();
        group->overloads.push_back(new_func);
    }
}

void VM::execute_CALL(CallFrame* frame)
{
    int arg_count = std::to_integer<int>(frame->consume_byte());
    Object_ptr callable = peek_tos(arg_count);

    std::visit(
        overloaded{
            [&](FunctionRuntimeObject_ptr func)
            {
                size_t new_base_pointer = stack.size() - arg_count;
                frames.emplace_back(func, new_base_pointer);
            },
            [&](NativeFunctionRuntimeObject_ptr native)
            {
                ObjectVector args = pop_n_from_stack(arg_count);
                pop_from_stack();
                push_to_stack(native->function(args));
            },
            [](auto&)
            {
                Doctor::get().fatal(
                    WaspStage::VM,
                    "Attempted to call a non-callable object"
                );
            }
        },
        callable->value
    );
}

void VM::execute_RETURN(CallFrame* frame)
{
    Object_ptr result = pop_from_stack();
    size_t bp = frame->base_pointer;

    frames.pop_back();

    if (bp > 0)
    {
        stack.erase(
            stack.begin() + static_cast<ptrdiff_t>(bp - 1),
            stack.end()
        );
    }
    else
    {
        stack.clear();
    }

    push_to_stack(result);
}

// ================================================
// Class Building
// ================================================

void VM::execute_BUILD_OVERLOAD_GROUP(CallFrame* frame)
{
    int count = std::to_integer<int>(frame->consume_byte());
    ObjectVector overloads = pop_n_from_stack(count);

    auto group = make_object(
        std::make_shared<OverloadsSet>(std::move(overloads))
    );
    push_to_stack(group);
}

void VM::execute_BUILD_CLASS(CallFrame* frame)
{
    int num_methods = static_cast<int>(frame->consume_byte());

    Object_ptr blueprint_obj = pop_from_stack();
    Object_ptr class_type_obj = pop_from_stack();
    ObjectVector methods = pop_n_from_stack(num_methods);

    auto class_type = class_type_obj->as<ClassType_ptr>();
    auto blueprint = blueprint_obj->as<ClassBlueprint_ptr>();

    Doctor::get().fatal_if_nullptr(
        class_type,
        WaspStage::VM,
        "BUILD_CLASS: class_type is null"
    );
    Doctor::get().fatal_if_nullptr(
        blueprint,
        WaspStage::VM,
        "BUILD_CLASS: blueprint is null"
    );
    Doctor::get().assert(
        methods.size() == static_cast<size_t>(num_methods),
        WaspStage::VM,
        "BUILD_CLASS: method count mismatch"
    );

    auto bag = std::make_shared<BagObject>();
    bag->overloads_set_vector.reserve(methods.size());

    for (size_t i = 0; i < methods.size(); i++)
    {
        auto overloads_set = methods[i]->as<OverloadsSet_ptr>();
        Doctor::get().fatal_if_nullptr(
            overloads_set,
            WaspStage::VM,
            "BUILD_CLASS: method group is null"
        );
        bag->overloads_set_vector.push_back(overloads_set);
    }

    bag->itables = class_type->bag_type->itables;

    blueprint->bag = bag;

    push_to_stack(make_object(blueprint));
}

void VM::execute_INSTANTIATE(CallFrame* frame)
{
    int num_fields = std::to_integer<int>(frame->consume_byte());

    Object_ptr blueprint_obj = pop_from_stack();
    ObjectVector field_values = pop_n_from_stack(num_fields);

    auto blueprint = blueprint_obj->as<ClassBlueprint_ptr>();
    Doctor::get().fatal_if_nullptr(
        blueprint,
        WaspStage::VM,
        "INSTANTIATE requires a ClassBlueprint"
    );

    auto instance = std::make_shared<ClassInstance>(
        std::make_shared<RecordObject>(field_values),
        blueprint->bag
    );

    push_to_stack(make_object(instance));
}

} // namespace Wasp
