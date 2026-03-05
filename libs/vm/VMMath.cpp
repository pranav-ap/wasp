#include "VM.h"

#include <string>
#include <cmath>
#include <variant>

#ifndef ASSERT
#include <cassert>
#define ASSERT(condition, message) assert((condition) && message)
#endif

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)
#define MAKE_SHARED_OBJECT_VARIANT(Type, ...) std::make_shared<Object>(std::make_shared<Type>(__VA_ARGS__))

template <class... Ts>
struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

using std::string;
using std::to_string;

namespace Wasp
{
    // ---------------------------------------------------------
    // Unary Operations
    // ---------------------------------------------------------

    Object_ptr VM::perform_unary_negative(Object_ptr obj)
    {
        return std::visit(overloaded{[&](IntObject &obj) -> Object_ptr
                                     {
                                         obj.value = -obj.value;
                                         return MAKE_OBJECT_VARIANT(obj);
                                     },
                                     [&](FloatObject &obj) -> Object_ptr
                                     {
                                         obj.value = -obj.value;
                                         return MAKE_OBJECT_VARIANT(obj);
                                     },

                                     [&](auto) -> Object_ptr
                                     {
                                         return pool->make_error_object("_");
                                     }},
                          obj->value);
    }

    Object_ptr VM::perform_unary_not(Object_ptr obj)
    {
        return std::visit(overloaded{[&](BooleanObject &obj) -> Object_ptr
                                     {
                                         obj.value = !obj.value;
                                         return MAKE_OBJECT_VARIANT(obj);
                                     },

                                     [&](auto) -> Object_ptr
                                     {
                                         return pool->make_error_object("_");
                                     }},
                          obj->value);
    }

    // ---------------------------------------------------------
    // Binary Operations
    // ---------------------------------------------------------

    Object_ptr VM::perform_add(Object_ptr left, Object_ptr right)
    {
        return std::visit(overloaded{[&](IntObject &left, IntObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value + right.value);
                                     },
                                     [&](FloatObject &left, FloatObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value + right.value);
                                     },
                                     [&](IntObject &left, FloatObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value + right.value);
                                     },
                                     [&](FloatObject &left, IntObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value + right.value);
                                     },

                                     [&](auto, auto) -> Object_ptr
                                     {
                                         return pool->make_error_object("_");
                                     }},
                          left->value, right->value);
    }

    Object_ptr VM::perform_subtract(Object_ptr left, Object_ptr right)
    {
        return std::visit(overloaded{[&](IntObject &left, IntObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value - right.value);
                                     },
                                     [&](FloatObject &left, FloatObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value - right.value);
                                     },
                                     [&](IntObject &left, FloatObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value - right.value);
                                     },
                                     [&](FloatObject &left, IntObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value - right.value);
                                     },
                                     [&](auto, auto) -> Object_ptr
                                     {
                                         return pool->make_error_object("_");
                                     }},
                          left->value, right->value);
    }

    Object_ptr VM::perform_multiply(Object_ptr left, Object_ptr right)
    {
        return std::visit(overloaded{[&](IntObject &left, IntObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value * right.value);
                                     },
                                     [&](FloatObject &left, FloatObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value * right.value);
                                     },
                                     [&](IntObject &left, FloatObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value * right.value);
                                     },
                                     [&](FloatObject &left, IntObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value * right.value);
                                     },

                                     // String

                                     [&](StringObject &left, IntObject &right) -> Object_ptr
                                     {
                                         string result_string("");
                                         int count = 0;

                                         while (count < right.value)
                                         {
                                             result_string += left.value;
                                             count++;
                                         }

                                         return pool->make_object(result_string);
                                     },

                                     [&](auto, auto) -> Object_ptr
                                     {
                                         return pool->make_error_object("_");
                                     }},
                          left->value, right->value);
    }

    Object_ptr VM::perform_divide(Object_ptr left, Object_ptr right)
    {
        return std::visit(overloaded{[&](IntObject &left, IntObject &right) -> Object_ptr
                                     {
                                         if (right.value == 0)
                                         {
                                             return pool->make_error_object("_");
                                         }

                                         return pool->make_object(left.value / right.value);
                                     },
                                     [&](FloatObject &left, FloatObject &right) -> Object_ptr
                                     {
                                         if (right.value == 0 || right.value == 0.0)
                                         {
                                             return pool->make_error_object("_");
                                         }

                                         return pool->make_object(left.value / right.value);
                                     },
                                     [&](IntObject &left, FloatObject &right) -> Object_ptr
                                     {
                                         if (right.value == 0 || right.value == 0.0)
                                         {
                                             return pool->make_error_object("_");
                                         }

                                         return pool->make_object(left.value / right.value);
                                     },
                                     [&](FloatObject &left, IntObject &right) -> Object_ptr
                                     {
                                         if (right.value == 0)
                                         {
                                             return pool->make_error_object("_");
                                         }

                                         return pool->make_object(left.value / right.value);
                                     },

                                     [&](auto, auto) -> Object_ptr
                                     {
                                         return pool->make_error_object("_");
                                     }},
                          left->value, right->value);
    }

    Object_ptr VM::perform_reminder(Object_ptr left, Object_ptr right)
    {
        return std::visit(overloaded{[&](IntObject &left, IntObject &right) -> Object_ptr
                                     {
                                         if (right.value == 0)
                                         {
                                             return pool->make_error_object("_");
                                         }

                                         return pool->make_object(left.value % right.value);
                                     },
                                     [&](FloatObject &left, FloatObject &right) -> Object_ptr
                                     {
                                         if (right.value == 0 || right.value == 0.0)
                                         {
                                             return pool->make_error_object("_");
                                         }

                                         return pool->make_object(std::fmod(left.value, right.value));
                                     },
                                     [&](IntObject &left, FloatObject &right) -> Object_ptr
                                     {
                                         if (right.value == 0 || right.value == 0.0)
                                         {
                                             return pool->make_error_object("_");
                                         }

                                         return pool->make_object(std::fmod(left.value, right.value));
                                     },
                                     [&](FloatObject &left, IntObject &right) -> Object_ptr
                                     {
                                         if (right.value == 0)
                                         {
                                             return pool->make_error_object("_");
                                         }

                                         return pool->make_object(std::fmod(left.value, right.value));
                                     },

                                     [&](auto, auto) -> Object_ptr
                                     {
                                         return pool->make_error_object("_");
                                     }},
                          left->value, right->value);
    }

    Object_ptr VM::perform_power(Object_ptr left, Object_ptr right)
    {
        return std::visit(overloaded{[&](IntObject &left, IntObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(std::pow(left.value, right.value));
                                     },
                                     [&](FloatObject &left, IntObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(std::pow(left.value, right.value));
                                     },

                                     [&](auto, auto) -> Object_ptr
                                     {
                                         return pool->make_error_object("_");
                                     }},
                          left->value, right->value);
    }

    // ---------------------------------------------------------
    // Logical Operations
    // ---------------------------------------------------------

    Object_ptr VM::perform_logical_and(Object_ptr left, Object_ptr right)
    {
        return std::visit(overloaded{[&](BooleanObject &left, BooleanObject &right)
                                     {
                                         return pool->make_object(left.value & right.value);
                                     },
                                     [&](auto, auto)
                                     {
                                         return pool->make_error_object("_");
                                     }},
                          left->value, right->value);
    }

    Object_ptr VM::perform_logical_or(Object_ptr left, Object_ptr right)
    {
        return std::visit(overloaded{[&](BooleanObject &left, BooleanObject &right)
                                     {
                                         return pool->make_object(left.value | right.value);
                                     },
                                     [&](auto, auto)
                                     {
                                         return pool->make_error_object("_");
                                     }},
                          left->value, right->value);
    }

    // ---------------------------------------------------------
    // Comparison Operations
    // ---------------------------------------------------------

    Object_ptr VM::perform_not_equal(Object_ptr left, Object_ptr right)
    {
        return std::visit(overloaded{[&](IntObject &left, IntObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value != right.value);
                                     },
                                     [&](FloatObject &left, FloatObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value != right.value);
                                     },
                                     [&](StringObject &left, StringObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value != right.value);
                                     },
                                     [&](BooleanObject &left, BooleanObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value != right.value);
                                     },
                                     [&](TupleObject &left, TupleObject &right) -> Object_ptr
                                     {
                                         if (left.values.size() != right.values.size())
                                         {
                                             return pool->get_false_object();
                                         }

                                         int size = left.values.size();

                                         for (int i = 0; i < size; i++)
                                         {
                                             auto left_value = left.values[i];
                                             auto right_value = right.values[i];

                                             if (perform_equal(left_value, right_value))
                                             {
                                                 return pool->get_false_object();
                                             }
                                         }

                                         return pool->get_true_object();
                                     },
                                     [&](ListObject &left, ListObject &right) -> Object_ptr
                                     {
                                         if (left.values.size() != right.values.size())
                                         {
                                             return pool->get_false_object();
                                         }

                                         int size = left.values.size();

                                         for (int i = 0; i < size; i++)
                                         {
                                             auto left_value = left.values[i];
                                             auto right_value = right.values[i];

                                             if (perform_equal(left_value, right_value))
                                             {
                                                 return pool->get_false_object();
                                             }
                                         }

                                         return pool->get_true_object();
                                     },
                                     [&](VariantObject &left, VariantObject &right) -> Object_ptr
                                     {
                                         return perform_not_equal(left.value, right.value);
                                     },

                                     [&](auto, auto) -> Object_ptr
                                     {
                                         return pool->make_error_object("_");
                                     }},
                          left->value, right->value);
    }

    Object_ptr VM::perform_equal(Object_ptr left, Object_ptr right)
    {
        return std::visit(overloaded{[&](IntObject &left, IntObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value == right.value);
                                     },
                                     [&](FloatObject &left, FloatObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value == right.value);
                                     },
                                     [&](StringObject &left, StringObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value == right.value);
                                     },
                                     [&](BooleanObject &left, BooleanObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value == right.value);
                                     },
                                     [&](TupleObject &left, TupleObject &right) -> Object_ptr
                                     {
                                         if (left.values.size() != right.values.size())
                                         {
                                             return pool->get_false_object();
                                         }

                                         int size = left.values.size();

                                         for (int i = 0; i < size; i++)
                                         {
                                             auto left_value = left.values[i];
                                             auto right_value = right.values[i];

                                             if (perform_not_equal(left_value, right_value))
                                             {
                                                 return pool->get_false_object();
                                             }
                                         }

                                         return pool->get_true_object();
                                     },
                                     [&](ListObject &left, ListObject &right) -> Object_ptr
                                     {
                                         if (left.values.size() != right.values.size())
                                         {
                                             return pool->get_false_object();
                                         }

                                         int size = left.values.size();

                                         for (int i = 0; i < size; i++)
                                         {
                                             auto left_value = left.values[i];
                                             auto right_value = right.values[i];

                                             if (perform_not_equal(left_value, right_value))
                                             {
                                                 return pool->get_false_object();
                                             }
                                         }

                                         return pool->get_true_object();
                                     },
                                     [&](VariantObject &left, VariantObject &right) -> Object_ptr
                                     {
                                         return perform_equal(left.value, right.value);
                                     },

                                     [&](auto, auto) -> Object_ptr
                                     {
                                         return pool->make_error_object("_");
                                     }},
                          left->value, right->value);
    }

    Object_ptr VM::perform_lesser_than(Object_ptr left, Object_ptr right)
    {
        return std::visit(overloaded{[&](IntObject &left, IntObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value < right.value);
                                     },
                                     [&](FloatObject &left, FloatObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value < right.value);
                                     },
                                     [&](IntObject &left, FloatObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value < right.value);
                                     },
                                     [&](FloatObject &left, IntObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value < right.value);
                                     },

                                     [&](StringObject &left, StringObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value < right.value);
                                     },

                                     [&](VariantObject &left, VariantObject &right) -> Object_ptr
                                     {
                                         return perform_lesser_than(left.value, right.value);
                                     },

                                     [&](auto, auto) -> Object_ptr
                                     {
                                         return pool->make_error_object("_");
                                     }},
                          left->value, right->value);
    }

    Object_ptr VM::perform_lesser_than_equal(Object_ptr left, Object_ptr right)
    {
        return std::visit(overloaded{[&](IntObject &left, IntObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value <= right.value);
                                     },
                                     [&](FloatObject &left, FloatObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value <= right.value);
                                     },
                                     [&](IntObject &left, FloatObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value <= right.value);
                                     },
                                     [&](FloatObject &left, IntObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value <= right.value);
                                     },

                                     [&](StringObject &left, StringObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value <= right.value);
                                     },

                                     [&](VariantObject &left, VariantObject &right) -> Object_ptr
                                     {
                                         return perform_lesser_than_equal(left.value, right.value);
                                     },

                                     [&](auto, auto) -> Object_ptr
                                     {
                                         return pool->make_error_object("_");
                                     }},
                          left->value, right->value);
    }

    Object_ptr VM::perform_greater_than(Object_ptr left, Object_ptr right)
    {
        return std::visit(overloaded{[&](IntObject &left, IntObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value > right.value);
                                     },
                                     [&](FloatObject &left, FloatObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value > right.value);
                                     },
                                     [&](IntObject &left, FloatObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value > right.value);
                                     },
                                     [&](FloatObject &left, IntObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value > right.value);
                                     },

                                     [&](StringObject &left, StringObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value > right.value);
                                     },

                                     [&](VariantObject &left, VariantObject &right) -> Object_ptr
                                     {
                                         return perform_greater_than(left.value, right.value);
                                     },

                                     [&](auto, auto) -> Object_ptr
                                     {
                                         return pool->make_error_object("_");
                                     }},
                          left->value, right->value);
    }

    Object_ptr VM::perform_greater_than_equal(Object_ptr left, Object_ptr right)
    {
        return std::visit(overloaded{[&](IntObject &left, IntObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value >= right.value);
                                     },
                                     [&](FloatObject &left, FloatObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value >= right.value);
                                     },
                                     [&](IntObject &left, FloatObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value >= right.value);
                                     },
                                     [&](FloatObject &left, IntObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value >= right.value);
                                     },

                                     [&](StringObject &left, StringObject &right) -> Object_ptr
                                     {
                                         return pool->make_object(left.value >= right.value);
                                     },

                                     [&](VariantObject &left, VariantObject &right) -> Object_ptr
                                     {
                                         return perform_greater_than_equal(left.value, right.value);
                                     },

                                     [&](auto, auto) -> Object_ptr
                                     {
                                         return pool->make_error_object("_");
                                     }},
                          left->value, right->value);
    }
}
