#include "NativeRegistry.h"
#include "ConstantPool.h"
#include "Doctor.h"
#include "Objects.h"

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
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

namespace
{

static Object_ptr native_print(const std::vector<Object_ptr>& args, ConstantPool_ptr pool)
{
    for (const auto& arg : args)
    {
        std::visit(
            overloaded{
                [](const IntObject& i)
                {
                    std::cout << i.value;
                },
                [](const FloatObject& f)
                {
                    std::cout << f.value;
                },
                [](const StringObject& s)
                {
                    std::cout << s.value;
                },
                [](const BooleanObject& b)
                {
                    std::cout << (b.value ? "true" : "false");
                },
                [](const NoneObject&)
                {
                    std::cout << "none";
                },
                [](const auto&)
                {
                    Doctor::get().fatal(
                        WaspStage::VM,
                        "Native Registry : Unhandled type provided as args to print()"
                    );
                }
            },
            arg->value
        );
    }

    std::cout << std::endl;

    return pool->get_none_object();
}

static Object_ptr native_input(const std::vector<Object_ptr>& args)
{
    if (args.size() > 0)
    {
        Doctor::get().assert(
            args.size() == 1,
            WaspStage::VM,
            "Native Registry : input() takes at most one argument (the prompt string)"
        );

        std::visit(
            overloaded{
                [](const StringObject& s)
                {
                    std::cout << s.value << " ";
                },
                [](const auto&)
                {
                    Doctor::get().fatal(
                        WaspStage::VM,
                        "Native Registry : Unhandled type provided as args to input()"
                    );
                }
            },
            args[0]->value
        );
    }

    std::string input_line;
    std::getline(std::cin, input_line);

    return make_object(StringObject(input_line));
}

} // namespace

Object_ptr NativeRegistry::get_native_object(int index) const
{
    Doctor::get().assert(
        index >= 0 && index < static_cast<int>(native_objects.size()),
        WaspStage::VM,
        "Native function index out of bounds"
    );

    return native_objects[index];
}

Object_ptr NativeRegistry::get_native_object_type(int index) const
{
    Doctor::get().assert(
        index >= 0 && index < static_cast<int>(native_object_types.size()),
        WaspStage::VM,
        "Native function index out of bounds"
    );

    return native_object_types[index];
}

int NativeRegistry::get_native_index(const std::string& name) const
{
    auto it = native_names.find(name);

    Doctor::get()
        .assert(it != native_names.end(), WaspStage::VM, "Native function not found" + name);

    return it->second;
}

void NativeRegistry::add_native(
    const std::string& name,
    int arity,
    NativeFnType function,
    ObjectVector input_types,
    Object_ptr return_type
)
{
    int global_index = static_cast<int>(native_objects.size());

    native_names[name] = global_index;

    auto obj = make_object(std::make_shared<NativeFunctionObject>(function, arity, name));
    native_objects.push_back(obj);

    auto type_obj = make_object(
        std::make_shared<FunctionType>(std::move(input_types), std::move(return_type))
    );

    native_object_types.push_back(type_obj);
}

void NativeRegistry::load_stdlib()
{
    add_native(
        "print",
        1,
        [this](const std::vector<Object_ptr>& args)
        {
            return native_print(args, this->pool);
        },
        {pool->get_any_type()},
        pool->get_none_type()
    );

    add_native(
        "input",
        1,
        [](const std::vector<Object_ptr>& args)
        {
            return native_input(args);
        },
        {pool->get_string_type()},
        pool->get_string_type()
    );
}
} // namespace Wasp
