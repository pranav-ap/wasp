#include "Doctor.h"
#include "Objects.h"
#include "OpCode.h"
#include "VM.h"

#include <cstddef>
#include <cstdint>
#include <map>
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
    case OpCode::LOAD_CONSTANT: {
        push_to_stack(workspace->pool->get(static_cast<int>(frame->consume_byte())));
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

void VM::execute_build_collection(OpCode op, CallFrame* frame)
{
    // Consume how many items we need
    int count = static_cast<int>(std::to_integer<int>(frame->consume_byte()));

    switch (op)
    {
    case OpCode::BUILD_LIST: {
        push_to_stack(make_object(std::make_shared<ListObject>(pop_n_from_stack(count))));
        break;
    }

    case OpCode::BUILD_TUPLE: {
        push_to_stack(make_object(std::make_shared<TupleObject>(pop_n_from_stack(count))));
        break;
    }

    case OpCode::BUILD_SET: {
        push_to_stack(make_object(std::make_shared<SetObject>(pop_n_from_stack(count))));
        break;
    }

    case OpCode::BUILD_MAP: {
        // Maps have 2 items per pair (Key + Value)
        ObjectVector elements = pop_n_from_stack(count * 2);

        std::map<Object_ptr, Object_ptr> map_elements;

        // Iterate by 2 skips: elements[0] is Key, elements[1] is Value
        for (size_t i = 0; i < elements.size(); i += 2)
        {
            map_elements[elements[i]] = elements[i + 1];
        }

        push_to_stack(make_object(std::make_shared<MapObject>(map_elements)));
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
    case OpCode::POP_SCOPE_KEEP_TOS: {
        size_t base = frame->scope_bases.back();
        frame->scope_bases.pop_back();

        Object_ptr tos = peek_tos();

        while (stack.size() > base)
        {
            pop_from_stack();
        }

        push_to_stack(tos);
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

    if (op == OpCode::JUMP || (op == OpCode::JUMP_IF_FALSE && !is_truthy(peek_tos())))
    {
        frame->ip = target_ip;
    }
}

void VM::execute_iter(OpCode op, CallFrame* frame)
{
    switch (op)
    {
    case OpCode::GET_ITER: {
        // Pop the collection off the stack
        Object_ptr iterable = pop_from_stack();
        Object_ptr iterator_instance = nullptr;

        // std::variant requires exact type matching, so we check our concrete collections!
        if (iterable->is<std::shared_ptr<ListObject>>())
        {
            iterator_instance = iterable->as<std::shared_ptr<ListObject>>()->get_iter();
        }
        else if (iterable->is<std::shared_ptr<SetObject>>())
        {
            iterator_instance = iterable->as<std::shared_ptr<SetObject>>()->get_iter();
        }
        else if (iterable->is<std::shared_ptr<MapObject>>())
        {
            iterator_instance = iterable->as<std::shared_ptr<MapObject>>()->get_iter();
        }
        else
        {
            Doctor::get().fatal(WaspStage::VM, "Cannot iterate over a non-iterable object");
        }

        // Push the new IteratorObject back onto the stack
        push_to_stack(iterator_instance);
        break;
    }

    case OpCode::LOOP_ITER: {
        // Read the 16-bit jump target offset (used if the iterator is exhausted)
        uint8_t low = static_cast<uint8_t>(frame->consume_byte());
        uint8_t high = static_cast<uint8_t>(frame->consume_byte());
        uint16_t target_ip = low | (high << 8);

        // The iterator must stay on the stack until the loop ends
        Object_ptr iterator_obj = peek_tos();

        auto iter = iterator_obj->as<std::shared_ptr<IteratorObject>>();

        if (auto next_val = iter->get_next())
        {
            push_to_stack(*next_val);
        }
        else
        {

            // std::nullopt was returned. The iterator is exhausted.
            frame->ip = target_ip;
        }
        break;
    }

    default:
        break;
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

        Doctor::get().fatal_if_nullptr(obj, WaspStage::VM, "Cannot read property of null.");

        push_to_stack(execute_GET_MEMBER(obj, member_index));
    }
    else if (op == OpCode::SET_MEMBER)
    {
        Object_ptr val = pop_from_stack();
        Object_ptr obj = pop_from_stack();

        Doctor::get().fatal_if_nullptr(obj, WaspStage::VM, "Cannot set property on null.");

        execute_SET_MEMBER(obj, member_index, val);
        push_to_stack(val);
    }
}

Object_ptr VM::execute_GET_MEMBER(Object_ptr obj, int member_index)
{
    return std::visit(
        overloaded{
            [&](std::shared_ptr<ModuleObject>& mod) -> Object_ptr
            {
                return mod->get_member(member_index);
            },
            [&](std::shared_ptr<ClassInstanceObject>& class_inst) -> Object_ptr
            {
                Doctor::get().assert(
                    member_index >= 0 && member_index < static_cast<int>(class_inst->fields.size()),
                    WaspStage::VM,
                    "Field index out of bounds!"
                );

                return class_inst->fields[member_index];
            },
            [&](std::shared_ptr<TraitInstanceObject>& trait_inst) -> Object_ptr
            {
                return execute_GET_MEMBER(trait_inst->class_instance, member_index);
            },
            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::VM,
                    "Object of this type does not support reading properties."
                );
            }
        },
        obj->value
    );
}

void VM::execute_SET_MEMBER(Object_ptr obj, int member_index, Object_ptr value)
{
    std::visit(
        overloaded{
            [&](std::shared_ptr<ModuleObject>& mod)
            {
                mod->set_member(member_index, value);
            },
            [&](std::shared_ptr<ClassInstanceObject>& class_inst)
            {
                Doctor::get().assert(
                    member_index >= 0 && member_index < static_cast<int>(class_inst->fields.size()),
                    WaspStage::VM,
                    "Field index out of bounds!"
                );

                class_inst->fields[member_index] = value;
            },
            [&](std::shared_ptr<TraitInstanceObject>& trait_inst)
            {
                execute_SET_MEMBER(trait_inst->class_instance, member_index, value);
            },
            [&](auto&)
            {
                Doctor::get().fatal(WaspStage::VM, "Object does not support setting properties.");
            }
        },
        obj->value
    );
}

// ----------------------------------------------
// Boxing
// ----------------------------------------------

void VM::execute_BOX(CallFrame* frame)
{
    int trait_id = static_cast<int>(frame->consume_byte());
    Object_ptr instance_obj = pop_from_stack();

    auto class_instance = instance_obj->as<std::shared_ptr<ClassInstanceObject>>();
    Doctor::get().fatal_if_nullptr(class_instance, WaspStage::VM);

    auto itable = class_instance->blueprint->itables.at(trait_id);
    auto boxed = make_object(std::make_shared<TraitInstanceObject>(instance_obj, itable));

    push_to_stack(boxed);
}

// ----------------------------------------------
// FUNCTION CALLS
// ----------------------------------------------

void VM::execute_GET_FUNCTION(CallFrame* frame)
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
        "GET_FUNCTION expects an Overload Group or Function on the stack!"
    );

    auto group = obj->as<std::shared_ptr<ObjectOverloadList>>();

    Doctor::get().assert(
        overload_index >= 0 && overload_index < static_cast<int>(group->overloads.size()),
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

    auto class_inst = obj->as<std::shared_ptr<ClassInstanceObject>>();
    auto method_obj = class_inst->blueprint->get_method(member_idx);
    auto overload_list = method_obj->as<std::shared_ptr<ObjectOverloadList>>();
    auto concrete_method = overload_list->overloads[overload_idx];

    push_to_stack(concrete_method);
    push_to_stack(obj);
}

void VM::execute_GET_TRAIT_METHOD(CallFrame* frame)
{
    int trait_member_index = static_cast<int>(frame->consume_byte());
    int trait_overload_index = static_cast<int>(frame->consume_byte());

    Object_ptr boxed_obj = pop_from_stack();

    auto trait_inst = boxed_obj->as<std::shared_ptr<TraitInstanceObject>>();
    Doctor::get().fatal_if_nullptr(trait_inst, WaspStage::VM, "Expected boxed trait on stack.");

    OverloadCoordinate trait_coord{trait_member_index, trait_overload_index};

    Doctor::get().assert(
        trait_inst->itable.find(trait_coord) != trait_inst->itable.end(),
        WaspStage::VM,
        "Trait ITable is missing the requested overload coordinate."
    );

    OverloadCoordinate class_coord = trait_inst->itable.at(trait_coord);

    auto class_instance = trait_inst->class_instance->as<std::shared_ptr<ClassInstanceObject>>();
    auto overloads_obj = class_instance->blueprint->get_method(class_coord.member_index);
    auto overloads_list = overloads_obj->as<std::shared_ptr<ObjectOverloadList>>();

    Doctor::get().assert(
        class_coord.overload_index >= 0 &&
            class_coord.overload_index < static_cast<int>(overloads_list->overloads.size()),
        WaspStage::VM,
        "Mapped overload index out of bounds."
    );

    auto concrete_method = overloads_list->overloads[class_coord.overload_index];

    push_to_stack(concrete_method);
    push_to_stack(boxed_obj);
}

void VM::execute_BUILD_FUNCTION(CallFrame* frame)
{
    int upvalue_count = std::to_integer<int>(frame->consume_byte());
    Object_ptr blueprint_obj = pop_from_stack();

    Doctor::get().assert(
        blueprint_obj->is<std::shared_ptr<FunctionBlueprintObject>>(),
        WaspStage::VM,
        "BUILD_FUNCTION expects a FunctionBlueprintObject on the stack"
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

void VM::execute_CALL(CallFrame* frame)
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

void VM::execute_RETURN(CallFrame* frame)
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

// ================================================
// Class
// ================================================

void VM::execute_BUILD_OVERLOAD_GROUP(CallFrame* frame)
{
    int count = std::to_integer<int>(frame->consume_byte());
    ObjectVector overloads = pop_n_from_stack(count);

    auto group = make_object(std::make_shared<ObjectOverloadList>(std::move(overloads)));
    push_to_stack(group);
}

void VM::execute_BUILD_CLASS(CallFrame* frame)
{
    int num_methods = static_cast<int>(frame->consume_byte());

    Object_ptr blueprint_obj = pop_from_stack();
    Object_ptr class_type_obj = pop_from_stack();
    ObjectVector popped_methods = pop_n_from_stack(num_methods);

    auto class_type = class_type_obj->as<std::shared_ptr<ClassType>>();
    auto blueprint = blueprint_obj->as<std::shared_ptr<ClassBlueprintObject>>();

    // 1. Get the number of fields so we know how much to offset the methods
    // (If your ClassType doesn't have a get_fields() method, use class_type->fields.size())
    int num_fields = static_cast<int>(class_type->get_fields().size());

    // 2. Resize the blueprint's methods array to cover the highest absolute index
    blueprint->methods.resize(num_fields + num_methods);

    // 3. Place the methods exactly where the GET_CLASS_METHOD opcode expects them
    for (int i = 0; i < num_methods; ++i)
    {
        blueprint->methods[num_fields + i] = std::move(popped_methods[i]);
    }

    blueprint->itables = class_type->itables;

    push_to_stack(blueprint_obj);
}

void VM::execute_INSTANTIATE(CallFrame* frame)
{
    int num_fields = std::to_integer<int>(frame->consume_byte());

    Object_ptr blueprint_obj = pop_from_stack();
    ObjectVector fields = pop_n_from_stack(num_fields);

    auto blueprint = blueprint_obj->as<std::shared_ptr<ClassBlueprintObject>>();

    auto instance = make_object(
        std::make_shared<ClassInstanceObject>(blueprint, std::move(fields))
    );

    push_to_stack(instance);
}

// ================================================
// Module
// ================================================

void VM::execute_import_module(CallFrame* frame)
{
    int module_index = static_cast<int>(frame->consume_byte());

    auto target_module = workspace->get_module(module_index);

    Doctor::get().fatal_if_nullptr(
        target_module,
        WaspStage::VM,
        "Module not found at registry index: " + std::to_string(module_index)
    );

    void* blueprint_ptr = target_module->blueprint.get();
    if (evaluated_modules.contains(blueprint_ptr))
    {
        push_to_stack(evaluated_modules.at(blueprint_ptr));
        return;
    }

    auto module_func = std::make_shared<FunctionRuntimeObject>(target_module->blueprint);

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

    auto module_obj = make_object(exports);

    void* blueprint_ptr = frame->function->blueprint.get();
    evaluated_modules[blueprint_ptr] = module_obj;

    // Cleanup the frame
    size_t bp = frame->base_pointer;
    stack.erase(stack.begin() + bp, stack.end());
    frames.pop_back();

    // Make module object available to importer
    push_to_stack(module_obj);
}

} // namespace Wasp
