#include "Doctor.h"
#include "Objects.h"
#include "OpCode.h"
#include "VM.h"

#include <cmath>
#include <memory>
#include <string>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void VM::execute_binary_op(OpCode op)
{
    Object_ptr right = pop_from_stack();
    Object_ptr left = pop_from_stack();
    Object_ptr result = nullptr;

    switch (op)
    {
    case OpCode::ADD:
        result = perform_add(left, right);
        break;
    case OpCode::SUB:
        result = perform_subtract(left, right);
        break;
    case OpCode::MUL:
        result = perform_multiply(left, right);
        break;
    case OpCode::DIV:
        result = perform_divide(left, right);
        break;
    case OpCode::MOD:
        result = perform_reminder(left, right);
        break;
    case OpCode::POW:
        result = perform_power(left, right);
        break;
    case OpCode::EQ:
        result = perform_equal(left, right);
        break;
    case OpCode::NE:
        result = perform_not_equal(left, right);
        break;
    case OpCode::LT:
        result = perform_lesser_than(left, right);
        break;
    case OpCode::LE:
        result = perform_lesser_than_equal(left, right);
        break;
    case OpCode::GT:
        result = perform_greater_than(left, right);
        break;
    case OpCode::GE:
        result = perform_greater_than_equal(left, right);
        break;
    case OpCode::LOGICAL_AND:
        result = perform_logical_and(left, right);
        break;
    case OpCode::LOGICAL_OR:
        result = perform_logical_or(left, right);
        break;
    default:
        Doctor::get().fatal(WaspStage::VM, "Invalid binary opcode");
    }

    push_to_stack(result);
}

void VM::execute_unary_op(OpCode op)
{
    Object_ptr top = pop_from_stack();

    if (op == OpCode::NEGATE)
    {
        push_to_stack(perform_unary_negative(top));
    }
    else if (op == OpCode::NOT)
    {
        push_to_stack(perform_unary_not(top));
    }
}

// ---------------------------------------------------------
// Unary Operations
// ---------------------------------------------------------

Object_ptr VM::perform_unary_negative(Object_ptr obj)
{
    return std::visit(
        overloaded{
            [&](IntObject_ptr i) -> Object_ptr
            {
                return make_object(std::make_shared<IntObject>(-i->value));
            },
            [&](FloatObject_ptr f) -> Object_ptr
            {
                return make_object(std::make_shared<FloatObject>(-f->value));
            },
            [&](auto) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::VM,
                    "Cannot negate non-numeric type"
                );
            }
        },
        obj->value
    );
}

Object_ptr VM::perform_unary_not(Object_ptr obj)
{
    bool truthy = is_truthy(obj);
    return truthy ? workspace->pool->get_false_object()
                  : workspace->pool->get_true_object();
}

// ---------------------------------------------------------
// Binary Operations
// ---------------------------------------------------------

Object_ptr VM::perform_add(Object_ptr left, Object_ptr right)
{
    return std::visit(
        overloaded{
            // Numeric Addition
            [&](IntObject_ptr l, IntObject_ptr r) -> Object_ptr
            {
                return make_object(
                    std::make_shared<IntObject>(l->value + r->value)
                );
            },
            [&](FloatObject_ptr l, FloatObject_ptr r) -> Object_ptr
            {
                return make_object(
                    std::make_shared<FloatObject>(l->value + r->value)
                );
            },
            [&](IntObject_ptr l, FloatObject_ptr r) -> Object_ptr
            {
                return make_object(
                    std::make_shared<FloatObject>(l->value + r->value)
                );
            },
            [&](FloatObject_ptr l, IntObject_ptr r) -> Object_ptr
            {
                return make_object(
                    std::make_shared<FloatObject>(l->value + r->value)
                );
            },
            // String Concatenation
            [&](StringObject_ptr l, StringObject_ptr r) -> Object_ptr
            {
                return make_object(
                    std::make_shared<StringObject>(l->value + r->value)
                );
            },
            // Mixed String + Number
            [&](StringObject_ptr l, IntObject_ptr r) -> Object_ptr
            {
                return make_object(
                    std::make_shared<StringObject>(
                        l->value + std::to_string(r->value)
                    )
                );
            },
            [&](IntObject_ptr l, StringObject_ptr r) -> Object_ptr
            {
                return make_object(
                    std::make_shared<StringObject>(
                        std::to_string(l->value) + r->value
                    )
                );
            },
            [&](StringObject_ptr l, FloatObject_ptr r) -> Object_ptr
            {
                return make_object(
                    std::make_shared<StringObject>(
                        l->value + std::to_string(r->value)
                    )
                );
            },
            [&](FloatObject_ptr l, StringObject_ptr r) -> Object_ptr
            {
                return make_object(
                    std::make_shared<StringObject>(
                        std::to_string(l->value) + r->value
                    )
                );
            },
            // Fallback
            [&](auto&, auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::VM,
                    "Unsupported operand types for +"
                );
            }
        },
        left->value,
        right->value
    );
}

Object_ptr VM::perform_subtract(Object_ptr left, Object_ptr right)
{
    return std::visit(
        overloaded{
            [&](IntObject_ptr l, IntObject_ptr r) -> Object_ptr
            {
                return make_object(
                    std::make_shared<IntObject>(l->value - r->value)
                );
            },
            [&](FloatObject_ptr l, FloatObject_ptr r) -> Object_ptr
            {
                return make_object(
                    std::make_shared<FloatObject>(l->value - r->value)
                );
            },
            [&](IntObject_ptr l, FloatObject_ptr r) -> Object_ptr
            {
                return make_object(
                    std::make_shared<FloatObject>(l->value - r->value)
                );
            },
            [&](FloatObject_ptr l, IntObject_ptr r) -> Object_ptr
            {
                return make_object(
                    std::make_shared<FloatObject>(l->value - r->value)
                );
            },
            [&](auto&, auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::VM,
                    "Unsupported operand types for -"
                );
            }
        },
        left->value,
        right->value
    );
}

Object_ptr VM::perform_multiply(Object_ptr left, Object_ptr right)
{
    return std::visit(
        overloaded{
            [&](IntObject_ptr l, IntObject_ptr r) -> Object_ptr
            {
                return make_object(
                    std::make_shared<IntObject>(l->value * r->value)
                );
            },
            [&](FloatObject_ptr l, FloatObject_ptr r) -> Object_ptr
            {
                return make_object(
                    std::make_shared<FloatObject>(l->value * r->value)
                );
            },
            [&](IntObject_ptr l, FloatObject_ptr r) -> Object_ptr
            {
                return make_object(
                    std::make_shared<FloatObject>(l->value * r->value)
                );
            },
            [&](FloatObject_ptr l, IntObject_ptr r) -> Object_ptr
            {
                return make_object(
                    std::make_shared<FloatObject>(l->value * r->value)
                );
            },
            // String duplication
            [&](StringObject_ptr l, IntObject_ptr r) -> Object_ptr
            {
                std::string result;
                result.reserve(l->value.size() * r->value);
                for (int i = 0; i < r->value; ++i)
                {
                    result += l->value;
                }
                return make_object(std::make_shared<StringObject>(result));
            },
            [&](auto&, auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::VM,
                    "Unsupported operand types for *"
                );
            }
        },
        left->value,
        right->value
    );
}

Object_ptr VM::perform_divide(Object_ptr left, Object_ptr right)
{
    return std::visit(
        overloaded{
            [&](IntObject_ptr l, IntObject_ptr r) -> Object_ptr
            {
                if (r->value == 0)
                {
                    Doctor::get().fatal(WaspStage::VM, "Division by zero");
                }
                return make_object(
                    std::make_shared<IntObject>(l->value / r->value)
                );
            },
            [&](FloatObject_ptr l, FloatObject_ptr r) -> Object_ptr
            {
                if (r->value == 0.0)
                {
                    Doctor::get().fatal(WaspStage::VM, "Division by zero");
                }
                return make_object(
                    std::make_shared<FloatObject>(l->value / r->value)
                );
            },
            [&](IntObject_ptr l, FloatObject_ptr r) -> Object_ptr
            {
                if (r->value == 0.0)
                {
                    Doctor::get().fatal(WaspStage::VM, "Division by zero");
                }
                return make_object(
                    std::make_shared<FloatObject>(l->value / r->value)
                );
            },
            [&](FloatObject_ptr l, IntObject_ptr r) -> Object_ptr
            {
                if (r->value == 0)
                {
                    Doctor::get().fatal(WaspStage::VM, "Division by zero");
                }
                return make_object(
                    std::make_shared<FloatObject>(l->value / r->value)
                );
            },
            [&](auto&, auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::VM,
                    "Unsupported operand types for /"
                );
            }
        },
        left->value,
        right->value
    );
}

Object_ptr VM::perform_reminder(Object_ptr left, Object_ptr right)
{
    return std::visit(
        overloaded{
            [&](IntObject_ptr l, IntObject_ptr r) -> Object_ptr
            {
                if (r->value == 0)
                {
                    Doctor::get().fatal(WaspStage::VM, "Modulo by zero");
                }
                return make_object(
                    std::make_shared<IntObject>(l->value % r->value)
                );
            },
            [&](FloatObject_ptr l, FloatObject_ptr r) -> Object_ptr
            {
                if (r->value == 0.0)
                {
                    Doctor::get().fatal(WaspStage::VM, "Modulo by zero");
                }
                return make_object(
                    std::make_shared<FloatObject>(std::fmod(l->value, r->value))
                );
            },
            [&](IntObject_ptr l, FloatObject_ptr r) -> Object_ptr
            {
                if (r->value == 0.0)
                {
                    Doctor::get().fatal(WaspStage::VM, "Modulo by zero");
                }
                return make_object(
                    std::make_shared<FloatObject>(std::fmod(l->value, r->value))
                );
            },
            [&](FloatObject_ptr l, IntObject_ptr r) -> Object_ptr
            {
                if (r->value == 0)
                {
                    Doctor::get().fatal(WaspStage::VM, "Modulo by zero");
                }
                return make_object(
                    std::make_shared<FloatObject>(std::fmod(l->value, r->value))
                );
            },
            [&](auto&, auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::VM,
                    "Unsupported operand types for %"
                );
            }
        },
        left->value,
        right->value
    );
}

Object_ptr VM::perform_power(Object_ptr left, Object_ptr right)
{
    return std::visit(
        overloaded{
            [&](IntObject_ptr l, IntObject_ptr r) -> Object_ptr
            {
                return make_object(
                    std::make_shared<IntObject>(std::pow(l->value, r->value))
                );
            },
            [&](FloatObject_ptr l, IntObject_ptr r) -> Object_ptr
            {
                return make_object(
                    std::make_shared<FloatObject>(std::pow(l->value, r->value))
                );
            },
            [&](FloatObject_ptr l, FloatObject_ptr r) -> Object_ptr
            {
                return make_object(
                    std::make_shared<FloatObject>(std::pow(l->value, r->value))
                );
            },
            [&](auto&, auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::VM,
                    "Unsupported operand types for **"
                );
            }
        },
        left->value,
        right->value
    );
}

// ---------------------------------------------------------
// Logical Operations
// ---------------------------------------------------------

Object_ptr VM::perform_logical_and(Object_ptr left, Object_ptr right)
{
    return (is_truthy(left) && is_truthy(right))
               ? workspace->pool->get_true_object()
               : workspace->pool->get_false_object();
}

Object_ptr VM::perform_logical_or(Object_ptr left, Object_ptr right)
{
    return (is_truthy(left) || is_truthy(right))
               ? workspace->pool->get_true_object()
               : workspace->pool->get_false_object();
}

// ---------------------------------------------------------
// Comparison Operations
// ---------------------------------------------------------

Object_ptr VM::perform_equal(Object_ptr left, Object_ptr right)
{
    // TODO : dummy
    return workspace->pool->get_false_object();
}

Object_ptr VM::perform_not_equal(Object_ptr left, Object_ptr right)
{
    // TODO : dummy
    return workspace->pool->get_false_object();
}

Object_ptr VM::perform_lesser_than(Object_ptr left, Object_ptr right)
{
    return std::visit(
        overloaded{
            [&](IntObject_ptr l, IntObject_ptr r) -> Object_ptr
            {
                return (l->value < r->value)
                           ? workspace->pool->get_true_object()
                           : workspace->pool->get_false_object();
            },
            [&](FloatObject_ptr l, FloatObject_ptr r) -> Object_ptr
            {
                return (l->value < r->value)
                           ? workspace->pool->get_true_object()
                           : workspace->pool->get_false_object();
            },
            [&](IntObject_ptr l, FloatObject_ptr r) -> Object_ptr
            {
                return (l->value < r->value)
                           ? workspace->pool->get_true_object()
                           : workspace->pool->get_false_object();
            },
            [&](FloatObject_ptr l, IntObject_ptr r) -> Object_ptr
            {
                return (l->value < r->value)
                           ? workspace->pool->get_true_object()
                           : workspace->pool->get_false_object();
            },
            [&](StringObject_ptr l, StringObject_ptr r) -> Object_ptr
            {
                return (l->value < r->value)
                           ? workspace->pool->get_true_object()
                           : workspace->pool->get_false_object();
            },
            [&](auto&, auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::VM,
                    "Unsupported operand types for <"
                );
            }
        },
        left->value,
        right->value
    );
}

Object_ptr VM::perform_lesser_than_equal(Object_ptr left, Object_ptr right)
{
    return std::visit(
        overloaded{
            [&](IntObject_ptr l, IntObject_ptr r) -> Object_ptr
            {
                return (l->value <= r->value)
                           ? workspace->pool->get_true_object()
                           : workspace->pool->get_false_object();
            },
            [&](FloatObject_ptr l, FloatObject_ptr r) -> Object_ptr
            {
                return (l->value <= r->value)
                           ? workspace->pool->get_true_object()
                           : workspace->pool->get_false_object();
            },
            [&](IntObject_ptr l, FloatObject_ptr r) -> Object_ptr
            {
                return (l->value <= r->value)
                           ? workspace->pool->get_true_object()
                           : workspace->pool->get_false_object();
            },
            [&](FloatObject_ptr l, IntObject_ptr r) -> Object_ptr
            {
                return (l->value <= r->value)
                           ? workspace->pool->get_true_object()
                           : workspace->pool->get_false_object();
            },
            [&](StringObject_ptr l, StringObject_ptr r) -> Object_ptr
            {
                return (l->value <= r->value)
                           ? workspace->pool->get_true_object()
                           : workspace->pool->get_false_object();
            },
            [&](auto&, auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::VM,
                    "Unsupported operand types for <="
                );
            }
        },
        left->value,
        right->value
    );
}

Object_ptr VM::perform_greater_than(Object_ptr left, Object_ptr right)
{
    return std::visit(
        overloaded{
            [&](IntObject_ptr l, IntObject_ptr r) -> Object_ptr
            {
                return (l->value > r->value)
                           ? workspace->pool->get_true_object()
                           : workspace->pool->get_false_object();
            },
            [&](FloatObject_ptr l, FloatObject_ptr r) -> Object_ptr
            {
                return (l->value > r->value)
                           ? workspace->pool->get_true_object()
                           : workspace->pool->get_false_object();
            },
            [&](IntObject_ptr l, FloatObject_ptr r) -> Object_ptr
            {
                return (l->value > r->value)
                           ? workspace->pool->get_true_object()
                           : workspace->pool->get_false_object();
            },
            [&](FloatObject_ptr l, IntObject_ptr r) -> Object_ptr
            {
                return (l->value > r->value)
                           ? workspace->pool->get_true_object()
                           : workspace->pool->get_false_object();
            },
            [&](StringObject_ptr l, StringObject_ptr r) -> Object_ptr
            {
                return (l->value > r->value)
                           ? workspace->pool->get_true_object()
                           : workspace->pool->get_false_object();
            },
            [&](auto&, auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::VM,
                    "Unsupported operand types for >"
                );
            }
        },
        left->value,
        right->value
    );
}

Object_ptr VM::perform_greater_than_equal(Object_ptr left, Object_ptr right)
{
    return std::visit(
        overloaded{
            [&](IntObject_ptr l, IntObject_ptr r) -> Object_ptr
            {
                return (l->value >= r->value)
                           ? workspace->pool->get_true_object()
                           : workspace->pool->get_false_object();
            },
            [&](FloatObject_ptr l, FloatObject_ptr r) -> Object_ptr
            {
                return (l->value >= r->value)
                           ? workspace->pool->get_true_object()
                           : workspace->pool->get_false_object();
            },
            [&](IntObject_ptr l, FloatObject_ptr r) -> Object_ptr
            {
                return (l->value >= r->value)
                           ? workspace->pool->get_true_object()
                           : workspace->pool->get_false_object();
            },
            [&](FloatObject_ptr l, IntObject_ptr r) -> Object_ptr
            {
                return (l->value >= r->value)
                           ? workspace->pool->get_true_object()
                           : workspace->pool->get_false_object();
            },
            [&](StringObject_ptr l, StringObject_ptr r) -> Object_ptr
            {
                return (l->value >= r->value)
                           ? workspace->pool->get_true_object()
                           : workspace->pool->get_false_object();
            },
            [&](auto&, auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::VM,
                    "Unsupported operand types for >="
                );
            }
        },
        left->value,
        right->value
    );
}

} // namespace Wasp
