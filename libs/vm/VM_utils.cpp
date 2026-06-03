#include "Objects.h"
#include "OpCode.h"
#include "VM.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void VM::execute_stack_op(OpCode op)
{
    if (op == OpCode::POP)
    {
        pop_from_stack();
    }
    else if (op == OpCode::DUP)
    {
        push_to_stack(peek_tos());
    }
}

void VM::push_to_stack(Object_ptr value)
{
    stack.push_back(std::move(value));
}

Object_ptr VM::pop_from_stack()
{
    auto val = std::move(stack.back());
    stack.pop_back();
    return val;
}

ObjectVector VM::pop_n_from_stack(size_t n)
{
    ObjectVector values(n);

    for (int i = static_cast<int>(n) - 1; i >= 0; --i)
    {
        values[i] = pop_from_stack();
    }

    return values;
}

Object_ptr VM::peek_tos(size_t distance) const
{
    return stack[stack.size() - 1 - distance];
}

bool VM::is_truthy(Object_ptr obj) const
{
    return std::visit(
        overloaded{
            [](std::monostate)
            {
                return false;
            },

            [](NoneObject_ptr)
            {
                return false;
            },

            [](BooleanObject_ptr b)
            {
                return b->value;
            },

            [](IntObject_ptr i)
            {
                return i->value > 0;
            },
            [](FloatObject_ptr f)
            {
                return f->value > 0.0;
            },

            [](StringObject_ptr s)
            {
                return !s->value.empty();
            },

            [](ListObject_ptr l)
            {
                return !l->values.empty();
            },
            [](TupleObject_ptr t)
            {
                return !t->values.empty();
            },
            [](SetObject_ptr s)
            {
                return !s->values.empty();
            },
            [](MapObject_ptr m)
            {
                return !m->pairs.empty();
            },
            [](auto&)
            {
                return true;
            }
        },
        obj->value
    );
}

} // namespace Wasp
