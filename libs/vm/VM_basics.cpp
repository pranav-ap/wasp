#include "Doctor.h"
#include "Objects.h"
#include "OpCode.h"
#include "VM.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
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

        auto get_iterator = [](auto&& obj) -> Object_ptr
        {
            using T = std::decay_t<decltype(obj)>;

            if constexpr (requires { obj->get_iter(); })
            {
                return obj->get_iter();
            }
            else
            {
                Doctor::get().fatal(
                    WaspStage::VM,
                    "Cannot iterate over a non-iterable object"
                );
            }
        };

        Object_ptr iterator_instance = std::visit(
            [&](auto&& arg)
            {
                return get_iterator(arg);
            },
            iterable->value
        );

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

} // namespace Wasp
