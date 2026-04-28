#include "NativeRegistry.h"
#include "ConstantPool.h"
#include "Doctor.h"
#include "Objects.h"

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
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

int NativeRegistry::get_native_index(const std::string& name) const
{
    auto it = native_names.find(name);

    Doctor::get().assert(
        it != native_names.end(),
        WaspStage::VM,
        "Native function '" + name + "' not found in registry"
    );

    return it->second;
}

void NativeRegistry::add_native(const std::string& name, NativeFnType function)
{
    int global_index = static_cast<int>(native_objects.size());

    native_names[name] = global_index;

    auto obj = make_object(std::make_shared<NativeFunctionObject>(function, name));
    native_objects.push_back(obj);
}

void NativeRegistry::load_stdlib()
{
    add_native(
        "libs::core::io::print",
        [this](const std::vector<Object_ptr>& args)
        {
            return native_print(args, this->pool);
        }
    );

    add_native(
        "libs::core::io::input",
        [](const std::vector<Object_ptr>& args)
        {
            return native_input(args);
        }
    );

    add_native(
        "libs::core::greet::Greeter::greet",
        [this](const std::vector<Object_ptr>& args)
        {
            std::cout << "Greetings from the C++ Native Registry!" << std::endl;
            return pool->get_none_object();
        }
    );
}
} // namespace Wasp
