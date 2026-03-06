#include "VM.h"

#include <variant>
#include <stdexcept>

template <class... Ts>
struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{
    bool VM::is_truthy(Object_ptr obj) const
    {
        return std::visit(overloaded{[](std::monostate &)
                                     { return false; },

                                     [](NoneObject &)
                                     { return false; },

                                     [](BooleanObject &b)
                                     { return b.value; },

                                     [](IntObject &i)
                                     { return i.value > 0; },
                                     [](FloatObject &f)
                                     { return f.value > 0.0; },

                                     [](StringObject &s)
                                     { return !s.value.empty(); },

                                     [](ListObject &l)
                                     { return !l.values.empty(); },
                                     [](TupleObject &t)
                                     { return !t.values.empty(); },
                                     [](SetObject &s)
                                     { return !s.values.empty(); },
                                     [](MapObject &m)
                                     { return !m.pairs.empty(); },

                                     [](ErrorObject &e)
                                     { return false; },

                                     [](auto &)
                                     { return true; }},
                          obj->value);
    }

    void VM::execute()
    {
        CallFrame *frame = &frames.back();

        while (true)
        {
            OpCode instruction = static_cast<OpCode>(frame->consume_byte());

            switch (instruction)
            {
                // ==========================================
                // --- System & Lifecycle ---
                // ==========================================

            case OpCode::NO_OP:
                break;

            case OpCode::HALT:
                return;

            case OpCode::ENTER_MODULE:
                break;

            case OpCode::EXIT_MODULE:
                return;

                // ==========================================
                // --- Stack Manipulation ---
                // ==========================================

            case OpCode::POP:
            {
                pop_from_stack();
                break;
            }

            case OpCode::DUP:
            {
                Object_ptr top = peek_tos();
                push_to_stack(top);
                break;
            }

                // ==========================================
                // --- Constants ---
                // ==========================================

            case OpCode::LOAD_CONST:
            {
                int index = static_cast<int>(frame->consume_byte());
                push_to_stack(pool->get(index));
                break;
            }

            case OpCode::LOAD_TRUE:
            {
                push_to_stack(pool->get_true_object());
                break;
            }

            case OpCode::LOAD_FALSE:
            {
                push_to_stack(pool->get_false_object());
                break;
            }

            case OpCode::LOAD_NONE:
            {
                push_to_stack(pool->get_none_object());
                break;
            }

                // ==========================================
                // --- Variables & Scoping ---
                // ==========================================

            case OpCode::DEFINE_LOCAL:
            {
                frame->consume_byte();
                break;
            }

            case OpCode::SET_LOCAL:
            {
                int index = static_cast<int>(frame->consume_byte());
                // Peek the value (assignments evaluate to the value itself)
                Object_ptr value = peek_tos();

                if (index >= stack.size())
                {
                    stack.resize(index + 1);
                }

                stack[frame->base_pointer + index] = value;
                break;
            }

            case OpCode::GET_LOCAL:
            {
                int index = static_cast<int>(frame->consume_byte());

                if (index >= stack.size() || !stack[index])
                {
                    std::cerr << "VM Error: Uninitialized local variable at index " << index << std::endl;
                    break;
                }

                push_to_stack(stack[frame->base_pointer + index]);
                break;
            }

            case OpCode::SET_GLOBAL:
            {
                int index = static_cast<int>(frame->consume_byte());
                // Peek the value (assignments evaluate to the value itself)
                Object_ptr value = peek_tos();

                if (index >= globals.size())
                {
                    globals.resize(index + 1);
                }

                globals[index] = value;
                break;
            }

            case OpCode::GET_GLOBAL:
            {
                int index = static_cast<int>(frame->consume_byte());

                if (index >= globals.size() || !globals[index])
                {
                    std::cerr << "VM Error: Uninitialized global variable at index " << index << std::endl;
                    break;
                }

                push_to_stack(globals[index]);
                break;
            }

            case OpCode::PUSH_SCOPE:
            {
                frame->scope_bases.push_back(stack.size());
                break;
            }

            case OpCode::POP_SCOPE:
            {
                if (frame->scope_bases.empty())
                {
                    std::cerr << "VM Error: POP_SCOPE called with no active scope!" << std::endl;
                    break;
                }

                size_t base = frame->scope_bases.back();
                frame->scope_bases.pop_back();

                while (stack.size() > base)
                {
                    pop_from_stack();
                }

                break;
            }

                // ==========================================
                // Control Flow
                // ==========================================

            case OpCode::JUMP:
            {
                uint8_t low = static_cast<uint8_t>(frame->consume_byte());
                uint8_t high = static_cast<uint8_t>(frame->consume_byte());
                uint16_t target_ip = low | (high << 8);

                frame->ip = target_ip;
                break;
            }

            case OpCode::JUMP_IF_FALSE:
            {
                uint8_t low = static_cast<uint8_t>(frame->consume_byte());
                uint8_t high = static_cast<uint8_t>(frame->consume_byte());
                uint16_t target_ip = low | (high << 8);

                Object_ptr condition = pop_from_stack();

                bool is_false = !is_truthy(condition);

                if (is_false)
                {
                    frame->ip = target_ip;
                }

                break;
            }

                // ==========================================
                // Call
                // ==========================================

            case OpCode::CALL:
            {
                int arg_count = static_cast<int>(frame->consume_byte());
                Object_ptr callee = peek_tos(arg_count);

                std::visit(overloaded{[&](std::shared_ptr<FunctionObject> &func)
                                      {
                                          size_t new_base_pointer = stack.size() - arg_count;
                                          frames.emplace_back(func, new_base_pointer);
                                          // loop will start reading the new function's bytecode
                                          // in next itreation
                                          frame = &frames.back();
                                      },

                                      [&](NativeFunctionObject &native)
                                      {
                                          if (native.arity != arg_count)
                                          {
                                              throw std::runtime_error("Arity mismatch in native function call");
                                          }

                                          // Collect arguments from the stack
                                          std::vector<Object_ptr> args(arg_count);
                                          for (int i = arg_count - 1; i >= 0; i--)
                                          {
                                              args[i] = pop_from_stack();
                                          }

                                          // Pop the native function object itself off the stack
                                          pop_from_stack();

                                          Object_ptr result = native.function(args);
                                          push_to_stack(result);
                                      },

                                      [](auto &)
                                      {
                                          throw std::runtime_error("Attempted to call a non-callable object");
                                      }},
                           callee->value);

                break;
            }

                // ------------------------------------------
                // Binary Operations
                // ------------------------------------------

            case OpCode::ADD:
            {
                Object_ptr right = pop_from_stack();
                Object_ptr left = pop_from_stack();
                push_to_stack(perform_add(left, right));
                break;
            }

            case OpCode::SUB:
            {
                Object_ptr right = pop_from_stack();
                Object_ptr left = pop_from_stack();
                push_to_stack(perform_subtract(left, right));
                break;
            }

            case OpCode::MUL:
            {
                Object_ptr right = pop_from_stack();
                Object_ptr left = pop_from_stack();
                push_to_stack(perform_multiply(left, right));
                break;
            }

            case OpCode::DIV:
            {
                Object_ptr right = pop_from_stack();
                Object_ptr left = pop_from_stack();
                push_to_stack(perform_divide(left, right));
                break;
            }

            case OpCode::MOD:
            {
                Object_ptr right = pop_from_stack();
                Object_ptr left = pop_from_stack();
                push_to_stack(perform_reminder(left, right));
                break;
            }

            case OpCode::POW:
            {
                Object_ptr right = pop_from_stack();
                Object_ptr left = pop_from_stack();
                push_to_stack(perform_power(left, right));
                break;
            }

                // ==========================================
                // Comparisons
                // ==========================================

            case OpCode::EQ:
            {
                Object_ptr right = pop_from_stack();
                Object_ptr left = pop_from_stack();
                push_to_stack(perform_equal(left, right));
                break;
            }
            case OpCode::NE:
            {
                Object_ptr right = pop_from_stack();
                Object_ptr left = pop_from_stack();
                push_to_stack(perform_not_equal(left, right));
                break;
            }
            case OpCode::LT:
            {
                Object_ptr right = pop_from_stack();
                Object_ptr left = pop_from_stack();
                push_to_stack(perform_lesser_than(left, right));
                break;
            }
            case OpCode::LE:
            {
                Object_ptr right = pop_from_stack();
                Object_ptr left = pop_from_stack();
                push_to_stack(perform_lesser_than_equal(left, right));
                break;
            }
            case OpCode::GT:
            {
                Object_ptr right = pop_from_stack();
                Object_ptr left = pop_from_stack();
                push_to_stack(perform_greater_than(left, right));
                break;
            }
            case OpCode::GE:
            {
                Object_ptr right = pop_from_stack();
                Object_ptr left = pop_from_stack();
                push_to_stack(perform_greater_than_equal(left, right));
                break;
            }

                // ==========================================
                // Unary
                // ==========================================

            case OpCode::NEGATE:
            {
                Object_ptr top = pop_from_stack();
                push_to_stack(perform_unary_negative(top));
                break;
            }

            case OpCode::NOT:
            {
                Object_ptr top = pop_from_stack();
                push_to_stack(perform_unary_not(top));
                break;
            }

                // ==========================================
                // Logic
                // ==========================================

            case OpCode::LOGICAL_AND:
            {
                Object_ptr right = pop_from_stack();
                Object_ptr left = pop_from_stack();
                push_to_stack(perform_logical_and(left, right));
                break;
            }

            case OpCode::LOGICAL_OR:
            {
                Object_ptr right = pop_from_stack();
                Object_ptr left = pop_from_stack();
                push_to_stack(perform_logical_or(left, right));
                break;
            }

            default:
                std::cerr << "VM Error: Unknown OpCode encountered!" << std::endl;
                std::cerr << "Instruction: " << static_cast<int>(instruction) << std::endl;
                return;
            }
        }
    }
}