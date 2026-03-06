#pragma once

#include "Natives.h"
#include "Objects.h"

#include <unordered_map>
#include <string>
#include <vector>
#include <iostream>
#include <variant>

#ifndef ASSERT
#include <cassert>
#define ASSERT(condition, message) assert((condition) && message)
#endif

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)
#define FATAL(message) throw std::runtime_error(message)
#define NULL_CHECK(x) ASSERT(x != nullptr, "Oh shit! A nullptr")

template <class... Ts>
struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{
    void NativeRegistry::add_native(const std::string &name, int arity, NativeFnType function)
    {
        auto obj = MAKE_OBJECT_VARIANT(NativeFunctionObject(function, arity, name));

        int global_index = static_cast<int>(native_objects.size());

        native_objects.push_back(obj);
        native_names[name] = global_index;
    }

    void NativeRegistry::load_stdlib(ConstantPool_ptr pool)
    {
        add_native(
            "print",
            // Arity -1 means it can take any number of arguments!
            -1,
            [pool](const std::vector<Object_ptr> &args) -> Object_ptr
            {
                for (const auto &arg : args)
                {
                    std::visit(overloaded{[](const IntObject &i)
                                          { std::cout << i.value; },
                                          [](const StringObject &s)
                                          { std::cout << s.value; },
                                          [](const BooleanObject &b)
                                          { std::cout << (b.value ? "true" : "false"); },
                                          [](const auto &)
                                          { std::cout << "<object>"; }},
                               arg->value);
                }

                std::cout << std::endl;

                return pool->get_none_object();
            });
    }
}