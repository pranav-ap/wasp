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
        int slot_index = static_cast<int>(
            std::to_integer<int>(frame->consume_byte())
        );

        Doctor::get().assert(
            frame->base_pointer + slot_index < stack.size(),
            WaspStage::VM,
            "Stack overflow: Assignment to invalid local slot!"
        );

        stack[frame->base_pointer + slot_index] = peek_tos();
        break;
    }

    case OpCode::GET_LOCAL: {
        int slot_index = static_cast<int>(
            std::to_integer<int>(frame->consume_byte())
        );

        Doctor::get().assert(
            frame->base_pointer + slot_index < stack.size(),
            WaspStage::VM,
            "Stack underflow: Read from invalid local slot!"
        );

        push_to_stack(stack[frame->base_pointer + slot_index]);
        break;
    }

    case OpCode::GET_NATIVE: {
        int index = static_cast<int>(
            std::to_integer<int>(frame->consume_byte())
        );
        push_to_stack(workspace->native_registry->get_native_object(index));
        break;
    }

    case OpCode::GET_UPVALUE: {
        int index = static_cast<int>(
            std::to_integer<int>(frame->consume_byte())
        );
        push_to_stack(frame->function->upvalues[index]);
        break;
    }

    case OpCode::SET_UPVALUE: {
        int index = static_cast<int>(
            std::to_integer<int>(frame->consume_byte())
        );
        frame->function->upvalues[index] = peek_tos();
        break;
    }

    default:
        break;
    }
}

void VM::execute_build_collection(OpCode op, CallFrame* frame)
{
    int count = static_cast<int>(std::to_integer<int>(frame->consume_byte()));

    switch (op)
    {
    case OpCode::BUILD_LIST: {
        push_to_stack(
            make_object(std::make_shared<ListObject>(pop_n_from_stack(count)))
        );
        break;
    }

    case OpCode::BUILD_TUPLE: {
        push_to_stack(
            make_object(std::make_shared<TupleObject>(pop_n_from_stack(count)))
        );
        break;
    }

    case OpCode::BUILD_SET: {
        push_to_stack(
            make_object(std::make_shared<SetObject>(pop_n_from_stack(count)))
        );
        break;
    }

    case OpCode::BUILD_MAP: {
        ObjectVector elements = pop_n_from_stack(count * 2);
        std::map<Object_ptr, Object_ptr> map_elements;

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

    if (op == OpCode::JUMP ||
        (op == OpCode::JUMP_IF_FALSE && !is_truthy(peek_tos())))
    {
        frame->ip = target_ip;
    }
}

void VM::execute_iter(OpCode op, CallFrame* frame)
{
    switch (op)
    {
    case OpCode::GET_ITER: {
        Object_ptr iterable = pop_from_stack();
        Object_ptr iterator_instance = nullptr;

        if (iterable->is<ListObject_ptr>())
        {
            iterator_instance = iterable->as<ListObject_ptr>()->get_iter();
        }
        else if (iterable->is<SetObject_ptr>())
        {
            iterator_instance = iterable->as<SetObject_ptr>()->get_iter();
        }
        else if (iterable->is<MapObject_ptr>())
        {
            iterator_instance = iterable->as<MapObject_ptr>()->get_iter();
        }
        else if (iterable->is<StringObject_ptr>())
        {
            iterator_instance = iterable->as<StringObject_ptr>()->get_iter();
        }
        else
        {
            Doctor::get().fatal(
                WaspStage::VM,
                "Cannot iterate over a non-iterable object"
            );
        }

        push_to_stack(iterator_instance);
        break;
    }

    case OpCode::LOOP_ITER: {
        uint8_t low = static_cast<uint8_t>(frame->consume_byte());
        uint8_t high = static_cast<uint8_t>(frame->consume_byte());
        uint16_t target_ip = low | (high << 8);

        Object_ptr iterator_obj = peek_tos();
        auto iter = iterator_obj->as<IteratorObject_ptr>();

        if (auto next_val = iter->get_next())
        {
            push_to_stack(*next_val);
        }
        else
        {
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
        Doctor::get().fatal_if_nullptr(
            obj,
            WaspStage::VM,
            "Cannot read property of null."
        );
        push_to_stack(execute_GET_MEMBER(obj, member_index));
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
        execute_SET_MEMBER(obj, member_index, val);
        push_to_stack(val);
    }
}

Object_ptr VM::execute_GET_MEMBER(Object_ptr obj, int member_index)
{
    return std::visit(
        overloaded{
            [&](ModuleObject_ptr mod) -> Object_ptr
            {
                return mod->get_member(member_index);
            },
            [&](RecordObject_ptr record) -> Object_ptr
            {
                Doctor::get().assert(
                    member_index >= 0 &&
                        member_index < static_cast<int>(record->fields.size()),
                    WaspStage::VM,
                    "Field index out of bounds!"
                );
                return record->fields[member_index];
            },
            [&](ClassInstance_ptr instance) -> Object_ptr
            {
                Doctor::get().assert(
                    member_index >= 0 &&
                        member_index <
                            static_cast<int>(instance->record->fields.size()),
                    WaspStage::VM,
                    "Field index out of bounds!"
                );

                return instance->record->fields[member_index];
            },
            [&](TraitObject_ptr trait_obj) -> Object_ptr
            {
                Doctor::get().assert(
                    member_index >= 0 &&
                        member_index <
                            static_cast<int>(
                                trait_obj->class_instance->record->fields.size()
                            ),
                    WaspStage::VM,
                    "Field index out of bounds!"
                );

                return trait_obj->class_instance->record->fields[member_index];
            },
            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::VM,
                    "Object of this type does not support reading properties."
                );
                return nullptr;
            }
        },
        obj->value
    );
}

void VM::execute_SET_MEMBER(Object_ptr obj, int member_index, Object_ptr value)
{
    std::visit(
        overloaded{
            [&](ModuleObject_ptr mod)
            {
                mod->set_member(member_index, value);
            },
            [&](RecordObject_ptr record)
            {
                Doctor::get().assert(
                    member_index >= 0 &&
                        member_index < static_cast<int>(record->fields.size()),
                    WaspStage::VM,
                    "Field index out of bounds!"
                );
                record->fields[member_index] = value;
            },
            [&](ClassInstance_ptr instance)
            {
                Doctor::get().assert(
                    member_index >= 0 &&
                        member_index <
                            static_cast<int>(instance->record->fields.size()),
                    WaspStage::VM,
                    "Field index out of bounds!"
                );

                instance->record->fields[member_index] = value;
            },
            [&](TraitObject_ptr trait_obj)
            {
                Doctor::get().assert(
                    member_index >= 0 &&
                        member_index <
                            static_cast<int>(
                                trait_obj->class_instance->record->fields.size()
                            ),
                    WaspStage::VM,
                    "Field index out of bounds!"
                );

                trait_obj->class_instance->record->fields[member_index] = value;
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

// ----------------------------------------------
// Boxing (Trait Object Creation)
// ----------------------------------------------

void VM::execute_BOX(CallFrame* frame)
{
    int trait_id = static_cast<int>(frame->consume_byte());
    Object_ptr instance_obj = pop_from_stack();

    // Get the ClassInstance (contains both record and bag)
    auto class_instance = instance_obj->as<ClassInstance_ptr>();
    Doctor::get().fatal_if_nullptr(class_instance, WaspStage::VM);

    // Get the itable for this trait from the class instance's record
    auto& itables = class_instance->record->itables;
    auto itable = itables.at(trait_id);

    // Create TraitObject with ClassInstance and itable
    auto trait_obj = std::make_shared<TraitObject>(class_instance, itable);

    push_to_stack(make_object(trait_obj));
}

// ----------------------------------------------
// FUNCTION CALLS
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
    Doctor::get()
        .fatal_if_nullptr(overloads_set, WaspStage::VM, "Method not found");

    auto method = overloads_set->get_overload(overload_idx);
    push_to_stack(method);
    push_to_stack(obj); // Push instance as 'self'
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

    Object_ptr class_type_obj = pop_from_stack();
    Object_ptr record_obj = pop_from_stack();
    ObjectVector methods = pop_n_from_stack(num_methods);

    auto class_type = class_type_obj->as<ClassType_ptr>();
    auto record = record_obj->as<RecordObject_ptr>();

    // Build the BagObject with method overloads
    auto bag = std::make_shared<BagObject>();
    for (size_t i = 0; i < methods.size(); i++)
    {
        auto overloads_set = methods[i]->as<OverloadsSet_ptr>();
        bag->overloads_set_vector.push_back(overloads_set);
    }
    bag->itables = class_type->bag_type->itables;

    // Create the ClassInstance blueprint
    auto blueprint = std::make_shared<ClassInstance>(record, bag);

    push_to_stack(make_object(blueprint));
}

void VM::execute_INSTANTIATE(CallFrame* frame)
{
    int num_fields = std::to_integer<int>(frame->consume_byte());

    Object_ptr blueprint_obj = pop_from_stack();
    ObjectVector field_values = pop_n_from_stack(num_fields);

    auto blueprint = blueprint_obj->as<ClassInstance_ptr>();
    Doctor::get().fatal_if_nullptr(
        blueprint,
        WaspStage::VM,
        "INSTANTIATE requires a ClassInstance blueprint"
    );

    // Create a new instance with its own record (shared bag for methods)
    auto instance = std::make_shared<ClassInstance>(
        std::make_shared<RecordObject>(
            field_values,
            blueprint->record->itables
        ),
        blueprint->bag // Share the bag (methods are shared across instances)
    );

    push_to_stack(make_object(instance));
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

    auto module_func = std::make_shared<FunctionRuntimeObject>(
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

    auto module_obj = make_object(exports);

    void* blueprint_ptr = frame->function->blueprint.get();
    evaluated_modules[blueprint_ptr] = module_obj;

    size_t bp = frame->base_pointer;
    stack.erase(stack.begin() + static_cast<ptrdiff_t>(bp), stack.end());
    frames.pop_back();

    push_to_stack(module_obj);
}

} // namespace Wasp
