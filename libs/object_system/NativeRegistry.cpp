#include "NativeRegistry.h"
#include "Doctor.h"
#include "Objects.h"

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)
#define MAKE_SHARED_OBJECT_VARIANT(Type, ...)                                                      \
    std::make_shared<Object>(std::make_shared<Type>(__VA_ARGS__))

template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp {
Object_ptr NativeRegistry::get_native_object(int index) const {
    Doctor::get().assert_true(
        index >= 0 && index < static_cast<int>(native_objects.size()),
        WaspStage::VM,
        "Native function index out of bounds"
    );

    return native_objects[index];
}

Object_ptr NativeRegistry::get_native_object_type(int index) const {
    Doctor::get().assert_true(
        index >= 0 && index < static_cast<int>(native_object_types.size()),
        WaspStage::VM,
        "Native function index out of bounds"
    );

    return native_object_types[index];
}

int NativeRegistry::get_native_index(const std::string& name) const {
    auto it = native_names.find(name);

    Doctor::get().assert_true(
        it != native_names.end(), WaspStage::VM, "Native function not found" + name
    );

    return it->second;
}

void NativeRegistry::add_native(
    const std::string& name,
    int arity,
    NativeFnType function,
    ObjectVector input_types,
    Object_ptr return_type
) {
    int global_index = static_cast<int>(native_objects.size());

    native_names[name] = global_index;

    auto obj = MAKE_SHARED_OBJECT_VARIANT(NativeFunctionObject, function, arity, name);
    native_objects.push_back(obj);

    auto type_obj =
        MAKE_OBJECT_VARIANT(FunctionType(std::move(input_types), std::move(return_type)));

    native_object_types.push_back(type_obj);
}

void NativeRegistry::load_stdlib() {
    add_native(
        "print",
        // Arity -1 means it can take any number of arguments!
        -1,
        [&](const std::vector<Object_ptr>& args) -> Object_ptr {
            for (const auto& arg : args) {
                std::visit(
                    overloaded{
                        [](const IntObject& i) { std::cout << i.value; },
                        [](const StringObject& s) { std::cout << s.value; },
                        [](const BooleanObject& b) { std::cout << (b.value ? "true" : "false"); },
                        [](const auto&) { std::cout << "<object>"; }
                    },
                    arg->value
                );
            }

            std::cout << std::endl;

            return pool->get_none_object();
        },
        // Input Types
        {pool->get_any_type()},
        // Return Type
        pool->get_none_type()
    );

    add_native(
        "input",
        1,
        [&](const std::vector<Object_ptr>& args) -> Object_ptr {
            std::visit(
                overloaded{
                    [](const StringObject& s) { std::cout << s.value << " "; },
                    [](const auto&) { std::cout << "<prompt>"; }
                },
                args[0]->value
            );

            std::string input_line;
            std::getline(std::cin, input_line);

            return MAKE_OBJECT_VARIANT(StringObject(input_line));
        },
        // Input Types
        {pool->get_string_type()},
        // Return Type
        pool->get_string_type()
    );
}
} // namespace Wasp