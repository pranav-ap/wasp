#include "Doctor.h"
#include "Objects.h"
#include "OpCode.h"
#include "VM.h"

#include <cmath>
#include <cstddef>
#include <memory>
#include <string>

#include <memory>
#include <string>
#include <variant>

template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp {
void VM::execute_binary_op(OpCode op) {
    Object_ptr right = pop_from_stack();
    Object_ptr left = pop_from_stack();
    Object_ptr result = nullptr;

    switch (op) {
    // Math
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
    // Comparisons
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
    // Logic
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

void VM::execute_unary_op(OpCode op) {
    Object_ptr top = pop_from_stack();

    if (op == OpCode::NEGATE)
        push_to_stack(perform_unary_negative(top));
    else if (op == OpCode::NOT)
        push_to_stack(perform_unary_not(top));
}

// ---------------------------------------------------------
// Unary Operations
// ---------------------------------------------------------

Object_ptr VM::perform_unary_negative(Object_ptr obj) {
    return std::visit(
        overloaded{
            [&](IntObject& obj) -> Object_ptr {
                obj.value = -obj.value;
                return make_object(obj);
            },
            [&](FloatObject& obj) -> Object_ptr {
                obj.value = -obj.value;
                return make_object(obj);
            },
            [&](auto) -> Object_ptr { return workspace->pool->make_error_object("_"); }
        },
        obj->value
    );
}

Object_ptr VM::perform_unary_not(Object_ptr obj) {
    return std::visit(
        overloaded{
            [&](BooleanObject& obj) -> Object_ptr {
                obj.value = !obj.value;
                return make_object(obj);
            },
            [&](auto) -> Object_ptr { return workspace->pool->make_error_object("_"); }
        },
        obj->value
    );
}

// ---------------------------------------------------------
// Binary Operations
// ---------------------------------------------------------

Object_ptr VM::perform_add(Object_ptr left, Object_ptr right) {
    return std::visit(
        overloaded{
            [&](IntObject& left, IntObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value + right.value);
            },
            [&](FloatObject& left, FloatObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value + right.value);
            },
            [&](IntObject& left, FloatObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value + right.value);
            },
            [&](FloatObject& left, IntObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value + right.value);
            },
            [&](auto, auto) -> Object_ptr { return workspace->pool->make_error_object("_"); }
        },
        left->value,
        right->value
    );
}

Object_ptr VM::perform_subtract(Object_ptr left, Object_ptr right) {
    return std::visit(
        overloaded{
            [&](IntObject& left, IntObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value - right.value);
            },
            [&](FloatObject& left, FloatObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value - right.value);
            },
            [&](IntObject& left, FloatObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value - right.value);
            },
            [&](FloatObject& left, IntObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value - right.value);
            },
            [&](auto, auto) -> Object_ptr { return workspace->pool->make_error_object("_"); }
        },
        left->value,
        right->value
    );
}

Object_ptr VM::perform_multiply(Object_ptr left, Object_ptr right) {
    return std::visit(
        overloaded{
            [&](IntObject& left, IntObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value * right.value);
            },
            [&](FloatObject& left, FloatObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value * right.value);
            },
            [&](IntObject& left, FloatObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value * right.value);
            },
            [&](FloatObject& left, IntObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value * right.value);
            },
            // String duplication
            [&](StringObject& left, IntObject& right) -> Object_ptr {
                std::string result_string("");
                int count = 0;
                while (count < right.value) {
                    result_string += left.value;
                    count++;
                }
                return workspace->pool->make_object(result_string);
            },
            [&](auto, auto) -> Object_ptr { return workspace->pool->make_error_object("_"); }
        },
        left->value,
        right->value
    );
}

Object_ptr VM::perform_divide(Object_ptr left, Object_ptr right) {
    return std::visit(
        overloaded{
            [&](IntObject& left, IntObject& right) -> Object_ptr {
                if (right.value == 0)
                    return workspace->pool->make_error_object("_");
                return workspace->pool->make_object(left.value / right.value);
            },
            [&](FloatObject& left, FloatObject& right) -> Object_ptr {
                if (right.value == 0.0)
                    return workspace->pool->make_error_object("_");
                return workspace->pool->make_object(left.value / right.value);
            },
            [&](IntObject& left, FloatObject& right) -> Object_ptr {
                if (right.value == 0.0)
                    return workspace->pool->make_error_object("_");
                return workspace->pool->make_object(left.value / right.value);
            },
            [&](FloatObject& left, IntObject& right) -> Object_ptr {
                if (right.value == 0)
                    return workspace->pool->make_error_object("_");
                return workspace->pool->make_object(left.value / right.value);
            },
            [&](auto, auto) -> Object_ptr { return workspace->pool->make_error_object("_"); }
        },
        left->value,
        right->value
    );
}

Object_ptr VM::perform_reminder(Object_ptr left, Object_ptr right) {
    return std::visit(
        overloaded{
            [&](IntObject& left, IntObject& right) -> Object_ptr {
                if (right.value == 0)
                    return workspace->pool->make_error_object("_");
                return workspace->pool->make_object(left.value % right.value);
            },
            [&](FloatObject& left, FloatObject& right) -> Object_ptr {
                if (right.value == 0.0)
                    return workspace->pool->make_error_object("_");
                return workspace->pool->make_object(std::fmod(left.value, right.value));
            },
            [&](IntObject& left, FloatObject& right) -> Object_ptr {
                if (right.value == 0.0)
                    return workspace->pool->make_error_object("_");
                return workspace->pool->make_object(std::fmod(left.value, right.value));
            },
            [&](FloatObject& left, IntObject& right) -> Object_ptr {
                if (right.value == 0)
                    return workspace->pool->make_error_object("_");
                return workspace->pool->make_object(std::fmod(left.value, right.value));
            },
            [&](auto, auto) -> Object_ptr { return workspace->pool->make_error_object("_"); }
        },
        left->value,
        right->value
    );
}

Object_ptr VM::perform_power(Object_ptr left, Object_ptr right) {
    return std::visit(
        overloaded{
            [&](IntObject& left, IntObject& right) -> Object_ptr {
                return workspace->pool->make_object(std::pow(left.value, right.value));
            },
            [&](FloatObject& left, IntObject& right) -> Object_ptr {
                return workspace->pool->make_object(std::pow(left.value, right.value));
            },
            [&](auto, auto) -> Object_ptr { return workspace->pool->make_error_object("_"); }
        },
        left->value,
        right->value
    );
}

// ---------------------------------------------------------
// Logical Operations
// ---------------------------------------------------------

Object_ptr VM::perform_logical_and(Object_ptr left, Object_ptr right) {
    return std::visit(
        overloaded{
            [&](BooleanObject& left, BooleanObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value & right.value);
            },
            [&](auto, auto) -> Object_ptr { return workspace->pool->make_error_object("_"); }
        },
        left->value,
        right->value
    );
}

Object_ptr VM::perform_logical_or(Object_ptr left, Object_ptr right) {
    return std::visit(
        overloaded{
            [&](BooleanObject& left, BooleanObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value | right.value);
            },
            [&](auto, auto) -> Object_ptr { return workspace->pool->make_error_object("_"); }
        },
        left->value,
        right->value
    );
}

// ---------------------------------------------------------
// Comparison Operations
// ---------------------------------------------------------

Object_ptr VM::perform_not_equal(Object_ptr left, Object_ptr right) {
    return std::visit(
        overloaded{
            [&](IntObject& left, IntObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value != right.value);
            },
            [&](FloatObject& left, FloatObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value != right.value);
            },
            [&](StringObject& left, StringObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value != right.value);
            },
            [&](BooleanObject& left, BooleanObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value != right.value);
            },
            [&](TupleObject& left, TupleObject& right) -> Object_ptr {
                if (left.values.size() != right.values.size())
                    return workspace->pool->get_false_object();
                for (size_t i = 0; i < left.values.size(); i++) {
                    if (perform_equal(left.values[i], right.values[i])) {
                        return workspace->pool->get_false_object();
                    }
                }
                return workspace->pool->get_true_object();
            },
            [&](ListObject& left, ListObject& right) -> Object_ptr {
                if (left.values.size() != right.values.size())
                    return workspace->pool->get_false_object();
                for (size_t i = 0; i < left.values.size(); i++) {
                    if (perform_equal(left.values[i], right.values[i])) {
                        return workspace->pool->get_false_object();
                    }
                }
                return workspace->pool->get_true_object();
            },
            [&](VariantObject& left, VariantObject& right) -> Object_ptr {
                return perform_not_equal(left.value, right.value);
            },
            [&](auto, auto) -> Object_ptr { return workspace->pool->make_error_object("_"); }
        },
        left->value,
        right->value
    );
}

Object_ptr VM::perform_equal(Object_ptr left, Object_ptr right) {
    return std::visit(
        overloaded{
            [&](IntObject& left, IntObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value == right.value);
            },
            [&](FloatObject& left, FloatObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value == right.value);
            },
            [&](StringObject& left, StringObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value == right.value);
            },
            [&](BooleanObject& left, BooleanObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value == right.value);
            },
            [&](TupleObject& left, TupleObject& right) -> Object_ptr {
                if (left.values.size() != right.values.size())
                    return workspace->pool->get_false_object();
                for (size_t i = 0; i < left.values.size(); i++) {
                    if (perform_not_equal(left.values[i], right.values[i])) {
                        return workspace->pool->get_false_object();
                    }
                }
                return workspace->pool->get_true_object();
            },
            [&](ListObject& left, ListObject& right) -> Object_ptr {
                if (left.values.size() != right.values.size())
                    return workspace->pool->get_false_object();
                for (size_t i = 0; i < left.values.size(); i++) {
                    if (perform_not_equal(left.values[i], right.values[i])) {
                        return workspace->pool->get_false_object();
                    }
                }
                return workspace->pool->get_true_object();
            },
            [&](VariantObject& left, VariantObject& right) -> Object_ptr {
                return perform_equal(left.value, right.value);
            },
            [&](auto, auto) -> Object_ptr { return workspace->pool->make_error_object("_"); }
        },
        left->value,
        right->value
    );
}

Object_ptr VM::perform_lesser_than(Object_ptr left, Object_ptr right) {
    return std::visit(
        overloaded{
            [&](IntObject& left, IntObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value < right.value);
            },
            [&](FloatObject& left, FloatObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value < right.value);
            },
            [&](IntObject& left, FloatObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value < right.value);
            },
            [&](FloatObject& left, IntObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value < right.value);
            },
            [&](StringObject& left, StringObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value < right.value);
            },
            [&](VariantObject& left, VariantObject& right) -> Object_ptr {
                return perform_lesser_than(left.value, right.value);
            },
            [&](auto, auto) -> Object_ptr { return workspace->pool->make_error_object("_"); }
        },
        left->value,
        right->value
    );
}

Object_ptr VM::perform_lesser_than_equal(Object_ptr left, Object_ptr right) {
    return std::visit(
        overloaded{
            [&](IntObject& left, IntObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value <= right.value);
            },
            [&](FloatObject& left, FloatObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value <= right.value);
            },
            [&](IntObject& left, FloatObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value <= right.value);
            },
            [&](FloatObject& left, IntObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value <= right.value);
            },
            [&](StringObject& left, StringObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value <= right.value);
            },
            [&](VariantObject& left, VariantObject& right) -> Object_ptr {
                return perform_lesser_than_equal(left.value, right.value);
            },
            [&](auto, auto) -> Object_ptr { return workspace->pool->make_error_object("_"); }
        },
        left->value,
        right->value
    );
}

Object_ptr VM::perform_greater_than(Object_ptr left, Object_ptr right) {
    return std::visit(
        overloaded{
            [&](IntObject& left, IntObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value > right.value);
            },
            [&](FloatObject& left, FloatObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value > right.value);
            },
            [&](IntObject& left, FloatObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value > right.value);
            },
            [&](FloatObject& left, IntObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value > right.value);
            },
            [&](StringObject& left, StringObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value > right.value);
            },
            [&](VariantObject& left, VariantObject& right) -> Object_ptr {
                return perform_greater_than(left.value, right.value);
            },
            [&](auto, auto) -> Object_ptr { return workspace->pool->make_error_object("_"); }
        },
        left->value,
        right->value
    );
}

Object_ptr VM::perform_greater_than_equal(Object_ptr left, Object_ptr right) {
    return std::visit(
        overloaded{
            [&](IntObject& left, IntObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value >= right.value);
            },
            [&](FloatObject& left, FloatObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value >= right.value);
            },
            [&](IntObject& left, FloatObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value >= right.value);
            },
            [&](FloatObject& left, IntObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value >= right.value);
            },
            [&](StringObject& left, StringObject& right) -> Object_ptr {
                return workspace->pool->make_object(left.value >= right.value);
            },
            [&](VariantObject& left, VariantObject& right) -> Object_ptr {
                return perform_greater_than_equal(left.value, right.value);
            },
            [&](auto, auto) -> Object_ptr { return workspace->pool->make_error_object("_"); }
        },
        left->value,
        right->value
    );
}
} // namespace Wasp
