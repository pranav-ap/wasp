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
    case OpCode::LOAD_CONST: {
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

        Doctor::get()
            .fatal_if_nullptr(obj, WaspStage::VM, "Cannot read property of null.");

        push_to_stack(perform_get_member(obj, member_index));
    }
    else if (op == OpCode::SET_MEMBER)
    {
        Object_ptr val = pop_from_stack();
        Object_ptr obj = pop_from_stack();

        Doctor::get()
            .fatal_if_nullptr(obj, WaspStage::VM, "Cannot set property on null.");

        perform_set_member(obj, member_index, val);
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

            [&](std::shared_ptr<InstanceObject>& instance) -> Object_ptr
            {
                return instance->get_member(member_index);
            },
            [&](std::shared_ptr<ClassBlueprintObject>& class_obj) -> Object_ptr
            {
                return class_obj->get_member(member_index - class_obj->fields_count);
            },
            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::VM,
                    "Object of this type does not support reading "
                    "properties."
                );
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

            [&](std::shared_ptr<InstanceObject>& instance)
            {
                instance->set_member(member_index, value);
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

    // Cleanup the frame
    size_t bp = frame->base_pointer;
    stack.erase(stack.begin() + bp, stack.end());
    frames.pop_back();

    // Make module object available to importer
    push_to_stack(module_obj);
}

} // namespace Wasp
