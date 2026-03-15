#include "VM.h"
#include "Doctor.h"
#include "Objects.h"
#include "OpCode.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp {
void VM::push_to_stack(Object_ptr value) { stack.push_back(std::move(value)); }

Object_ptr VM::pop_from_stack() {
    auto val = std::move(stack.back());
    stack.pop_back();
    return val;
}

ObjectVector VM::pop_n_from_stack(size_t n) {
    ObjectVector values;
    values.reserve(n);

    for (size_t i = 0; i < n; i++) {
        values.push_back(pop_from_stack());
    }

    return values;
}

Object_ptr VM::peek_tos(size_t distance) const { return stack[stack.size() - 1 - distance]; }

bool VM::is_truthy(Object_ptr obj) const {
    return std::visit(
        overloaded{
            [](std::monostate&) { return false; },

            [](NoneObject&) { return false; },

            [](BooleanObject& b) { return b.value; },

            [](IntObject& i) { return i.value > 0; },
            [](FloatObject& f) { return f.value > 0.0; },

            [](StringObject& s) { return !s.value.empty(); },

            [](ListObject& l) { return !l.values.empty(); },
            [](TupleObject& t) { return !t.values.empty(); },
            [](SetObject& s) { return !s.values.empty(); },
            [](MapObject& m) { return !m.pairs.empty(); },

            [](ErrorObject& e) { return false; },

            [](auto&) { return true; }
        },
        obj->value
    );
}

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

void VM::execute_constant(OpCode op, CallFrame* frame) {
    switch (op) {
    case OpCode::LOAD_CONST:
        push_to_stack(pool->get(static_cast<int>(frame->consume_byte())));
        break;
    case OpCode::LOAD_TRUE:
        push_to_stack(pool->get_true_object());
        break;
    case OpCode::LOAD_FALSE:
        push_to_stack(pool->get_false_object());
        break;
    case OpCode::LOAD_NONE:
        push_to_stack(pool->get_none_object());
        break;
    default:
        break;
    }
}

void VM::execute_variable(OpCode op, CallFrame* frame) {
    switch (op) {
    case OpCode::DEFINE_LOCAL:
        frame->consume_byte();
        break;
    case OpCode::SET_LOCAL: {
        int index = static_cast<int>(frame->consume_byte());
        if (index >= stack.size())
            stack.resize(index + 1);
        stack[frame->base_pointer + index] = peek_tos();
        break;
    }
    case OpCode::GET_LOCAL: {
        int index = static_cast<int>(frame->consume_byte());
        push_to_stack(stack[frame->base_pointer + index]);
        break;
    }
    case OpCode::GET_NATIVE: {
        int index = static_cast<int>(frame->consume_byte());
        push_to_stack(native_registry->get_native_object(index));
        break;
    }
    case OpCode::PUSH_SCOPE:
        frame->scope_bases.push_back(stack.size());
        break;
    case OpCode::POP_SCOPE: {
        size_t base = frame->scope_bases.back();
        frame->scope_bases.pop_back();
        while (stack.size() > base)
            pop_from_stack();
        break;
    }
    default:
        break;
    }
}

void VM::execute_control_flow(OpCode op, CallFrame* frame) {
    uint8_t low = static_cast<uint8_t>(frame->consume_byte());
    uint8_t high = static_cast<uint8_t>(frame->consume_byte());
    uint16_t target_ip = low | (high << 8);

    if (op == OpCode::JUMP || (op == OpCode::JUMP_IF_FALSE && !is_truthy(peek_tos()))) {
        frame->ip = target_ip;
    }
}

void VM::execute_stack_op(OpCode op) {
    if (op == OpCode::POP)
        pop_from_stack();
    else if (op == OpCode::DUP)
        push_to_stack(peek_tos());
}

void VM::execute_call(CallFrame* frame) {
    int arg_count = static_cast<int>(frame->consume_byte());
    Object_ptr callable = peek_tos(arg_count);

    std::visit(
        overloaded{
            [&](std::shared_ptr<FunctionObject>& func) {
                size_t new_base_pointer = stack.size() - arg_count;
                frames.emplace_back(func, new_base_pointer);
                // The main execution loop will automatically start reading
                // the new function's bytecode on the next iteration
            },

            [&](std::shared_ptr<NativeFunctionObject>& native) {
                Doctor::get().assert(
                    native->arity == -1 || native->arity == arg_count,
                    WaspStage::VM,
                    "Arity mismatch in native function call"
                );

                // Collect arguments from the stack
                std::vector<Object_ptr> args(arg_count);
                for (int i = arg_count - 1; i >= 0; i--) {
                    args[i] = pop_from_stack();
                }

                // Pop the native function object off the stack
                pop_from_stack();

                Object_ptr result = native->function(args);
                push_to_stack(result);
            },

            [](auto&) {
                Doctor::get().fatal(WaspStage::VM, "Attempted to call a non-callable object");
            }
        },
        callable->value
    );
}

void VM::execute_member(OpCode op, CallFrame* frame) {
    //  Get the property name from the constant pool
    int name_index = static_cast<int>(frame->consume_byte());
    Object_ptr name_obj = pool->get(name_index);

    Doctor::get().assert(
        std::holds_alternative<StringObject>(name_obj->value),
        WaspStage::VM,
        "Member name in constant pool must be a string."
    );

    std::string member_name = std::get<StringObject>(name_obj->value).value;

    // Perform the operation
    if (op == OpCode::GET_MEMBER) {
        Object_ptr obj = pop_from_stack();
        Doctor::get().fatal_if_nullptr(
            obj, WaspStage::VM, "Cannot read property '" + member_name + "' of null."
        );

        push_to_stack(perform_get_member(obj, member_name));
    } else if (op == OpCode::SET_MEMBER) {
        Object_ptr val = pop_from_stack(); // Pushed second
        Object_ptr obj = pop_from_stack(); // Pushed first
        perform_set_member(obj, member_name, val);
    }
}

Object_ptr VM::perform_get_member(Object_ptr obj, const std::string& name) {
    return std::visit(
        overloaded{
            [&](std::shared_ptr<ModuleObject>& module_obj) -> Object_ptr {
                Object_ptr result = module_obj->get_member(name);
                Doctor::get().fatal_if_nullptr(
                    result,
                    WaspStage::VM,
                    "Module '" + module_obj->name + "' has no member named '" + name + "'."
                );
                return result;
            },
            [&](auto&) -> Object_ptr {
                Doctor::get().fatal(WaspStage::VM, "Object does not support reading properties.");
                return nullptr;
            }
        },
        obj->value
    );
}

void VM::perform_set_member(Object_ptr obj, const std::string& name, Object_ptr value) {
    std::visit(
        overloaded{
            [&](std::shared_ptr<ModuleObject>& module_obj) { module_obj->set_member(name, value); },
            [&](auto&) {
                Doctor::get().fatal(WaspStage::VM, "Object does not support setting properties.");
            }
        },
        obj->value
    );
}

void VM::execute() {
    while (true) {
        CallFrame* frame = &frames.back();
        OpCode instruction = static_cast<OpCode>(frame->consume_byte());

        switch (instruction) {
            // Lifecycle

        case OpCode::NO_OP:
            break;
        case OpCode::ENTER_WORKSPACE:
            break;
        case OpCode::ENTER_MODULE:
            break;
        case OpCode::EXIT_WORKSPACE:
        case OpCode::EXIT_MODULE:
        case OpCode::HALT:
            return;

            // Stack Manipulations

        case OpCode::POP:
        case OpCode::DUP:
            execute_stack_op(instruction);
            break;

            // Constants

        case OpCode::LOAD_CONST:
        case OpCode::LOAD_TRUE:
        case OpCode::LOAD_FALSE:
        case OpCode::LOAD_NONE:
            execute_constant(instruction, frame);
            break;

            // Variables & Scope

        case OpCode::DEFINE_LOCAL:
        case OpCode::SET_LOCAL:
        case OpCode::GET_LOCAL:
        case OpCode::GET_NATIVE:
        case OpCode::PUSH_SCOPE:
        case OpCode::POP_SCOPE:
            execute_variable(instruction, frame);
            break;

        // Members
        case OpCode::GET_MEMBER:
        case OpCode::SET_MEMBER:
            execute_member(instruction, frame);
            break;

            // Control Flow

        case OpCode::JUMP:
        case OpCode::JUMP_IF_FALSE:
            execute_control_flow(instruction, frame);
            break;

            // Binary Math, Comparisons, Logic

        case OpCode::ADD:
        case OpCode::SUB:
        case OpCode::MUL:
        case OpCode::DIV:
        case OpCode::MOD:
        case OpCode::POW:
        case OpCode::EQ:
        case OpCode::NE:
        case OpCode::LT:
        case OpCode::LE:
        case OpCode::GT:
        case OpCode::GE:
        case OpCode::LOGICAL_AND:
        case OpCode::LOGICAL_OR:
            execute_binary_op(instruction);
            break;

            // Unary

        case OpCode::NEGATE:
        case OpCode::NOT:
            execute_unary_op(instruction);
            break;

            // Calls

        case OpCode::CALL:
            execute_call(frame);
            break;

        default:
            Doctor::get().fatal(WaspStage::VM, "Unknown OpCode encountered");
        }
    }
}
} // namespace Wasp