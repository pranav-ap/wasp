#include "VM.h"

#include <variant>

template <class... Ts>
struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{
    void VM::print_object(Object_ptr obj)
    {
        std::visit(overloaded{[](IntObject &i)
                              { std::cout << i.value << std::endl; },
                              [](FloatObject &f)
                              { std::cout << f.value << std::endl; },
                              [](BooleanObject &b)
                              { std::cout << (b.value ? "true" : "false") << std::endl; },
                              [](StringObject &s)
                              { std::cout << s.value << std::endl; },
                              [](auto &)
                              { std::cout << "<object>" << std::endl; }},
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
                Object_ptr top = peek();
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
                // The value is already on the stack. Just consume the slot index.
                frame->consume_byte();
                break;
            }

            case OpCode::SET_LOCAL:
            {
                int index = static_cast<int>(frame->consume_byte());
                // Peek the value (assignments evaluate to the value itself)
                Object_ptr value = peek();
                stack[frame->base_pointer + index] = value;
                break;
            }

            case OpCode::GET_LOCAL:
            {
                int index = static_cast<int>(frame->consume_byte());
                push_to_stack(stack[frame->base_pointer + index]);
                break;
            }

            case OpCode::GET_GLOBAL:
            {
                int name_index = static_cast<int>(frame->consume_byte());
                Object_ptr name_obj = pool->get(name_index);

                std::visit(overloaded{[&](StringObject &s)
                                      {
                                          auto it = globals.find(s.value);
                                          if (it != globals.end())
                                          {
                                              push_to_stack(it->second);
                                          }
                                          else
                                          {
                                              std::cerr << "NameError: undefined global '" << s.value << "'" << std::endl;
                                              // Handle halt
                                          }
                                      },
                                      [](auto &)
                                      { std::cerr << "VM Error: Global name not a string." << std::endl; }},
                           *name_obj);
                break;
            }

            case OpCode::SET_GLOBAL:
            {
                int name_index = static_cast<int>(frame->consume_byte());
                Object_ptr name_obj = pool->get(name_index);
                Object_ptr value = peek(); // Don't pop, leave it for chained assignment

                std::visit(overloaded{[&](StringObject &s)
                                      {
                                          globals[s.value] = value;
                                      },
                                      [](auto &)
                                      { std::cerr << "VM Error: Global name not a string." << std::endl; }},
                           name_obj->value);
                break;
            }

            case OpCode::PUSH_SCOPE:
            case OpCode::POP_SCOPE:
            {
                // Depends on your compiler design. Often POP_SCOPE requires popping
                // locals off the stack if blocks clean up after themselves.
                break;
            }

                // ==========================================
                // Control Flow
                // ==========================================

            case OpCode::JUMP:
            {
                // Read 16-bit little-endian offset
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

                // Check truthiness (assuming BooleanObject is the standard for now)
                bool is_false = false;
                std::visit(overloaded{[&](BooleanObject &b)
                                      { is_false = !b.value; },
                                      [&](auto &) {}},
                           condition->value);

                if (is_false)
                {
                    frame->ip = target_ip;
                }
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
                return;
            }
        }
    }
}