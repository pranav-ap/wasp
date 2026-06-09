#include "NativeRegistry.h"
#include "ConstantPool.h"
#include "Doctor.h"
#include "Objects.h"

#include <cstddef>
#include <functional>
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

static Object_ptr native_print(
    const std::vector<Object_ptr>& args,
    ConstantPool_ptr pool
)
{
    for (const auto& arg : args)
    {
        std::visit(
            overloaded{
                [](IntObject_ptr i)
                {
                    std::cout << i->value;
                },
                [](FloatObject_ptr f)
                {
                    std::cout << f->value;
                },
                [](StringObject_ptr s)
                {
                    std::cout << s->value;
                },
                [](BooleanObject_ptr b)
                {
                    std::cout << (b->value ? "true" : "false");
                },
                [](NoneObject_ptr)
                {
                    std::cout << "none";
                },
                [](const auto&)
                {
                    Doctor::get().fatal(
                        WaspStage::VM,
                        "Native Registry : Unhandled type provided as args to "
                        "print()"
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
    if (!args.empty())
    {
        Doctor::get().assert(
            args.size() == 1,
            WaspStage::VM,
            "Native Registry : input() takes at most one argument (the prompt "
            "string)"
        );

        std::visit(
            overloaded{
                [](StringObject_ptr s)
                {
                    std::cout << s->value << " ";
                },
                [](const auto&)
                {
                    Doctor::get().fatal(
                        WaspStage::VM,
                        "Native Registry : input() argument must be a string"
                    );
                }
            },
            args[0]->value
        );
    }

    std::string input_line;
    std::getline(std::cin, input_line);
    return make_object(std::make_shared<StringObject>(input_line));
}

// String native methods
static Object_ptr native_str_hash(const std::vector<Object_ptr>& args)
{
    auto self = args[0]->as<StringObject_ptr>();
    size_t hash = std::hash<std::string>{}(self->value);
    return make_object(std::make_shared<IntObject>(static_cast<int>(hash)));
}

static Object_ptr native_str_size(const std::vector<Object_ptr>& args)
{
    auto self = args[0]->as<StringObject_ptr>();
    return make_object(
        std::make_shared<IntObject>(static_cast<int>(self->value.size()))
    );
}

static Object_ptr native_str_slice(const std::vector<Object_ptr>& args)
{
    auto self = args[0]->as<StringObject_ptr>();
    auto start = args[1]->as<IntObject_ptr>();
    auto end = args[2]->as<IntObject_ptr>();

    int start_idx = start->value;
    int end_idx = end->value;

    if (start_idx < 0)
    {
        start_idx = 0;
    }
    if (end_idx > static_cast<int>(self->value.size()))
    {
        end_idx = self->value.size();
    }
    if (start_idx >= end_idx)
    {
        return make_object(std::make_shared<StringObject>(""));
    }

    std::string result = self->value.substr(start_idx, end_idx - start_idx);
    return make_object(std::make_shared<StringObject>(result));
}

static Object_ptr native_str_trim(const std::vector<Object_ptr>& args)
{
    auto self = args[0]->as<StringObject_ptr>();
    std::string s = self->value;

    // Trim whitespace from beginning
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos)
    {
        return make_object(std::make_shared<StringObject>(""));
    }

    // Trim whitespace from end
    size_t end = s.find_last_not_of(" \t\n\r");

    std::string result = s.substr(start, end - start + 1);
    return make_object(std::make_shared<StringObject>(result));
}

// List native methods
static Object_ptr native_list_size(const std::vector<Object_ptr>& args)
{
    auto self = args[0]->as<ListObject_ptr>();
    return make_object(std::make_shared<IntObject>(self->get_length()));
}

static Object_ptr native_list_get(const std::vector<Object_ptr>& args)
{
    auto self = args[0]->as<ListObject_ptr>();
    auto index = args[1]->as<IntObject_ptr>();

    int idx = index->value;
    if (idx < 0)
    {
        idx = self->get_length() + idx;
    }

    Doctor::get().assert(
        idx >= 0 && idx < self->get_length(),
        WaspStage::VM,
        "List index out of bounds"
    );

    return self->get(make_object(std::make_shared<IntObject>(idx)));
}

static Object_ptr native_list_set(const std::vector<Object_ptr>& args)
{
    auto self = args[0]->as<ListObject_ptr>();
    auto index = args[1]->as<IntObject_ptr>();
    auto value = args[2];

    int idx = index->value;
    if (idx < 0)
    {
        idx = self->get_length() + idx;
    }

    Doctor::get().assert(
        idx >= 0 && idx < self->get_length(),
        WaspStage::VM,
        "List index out of bounds"
    );

    self->set(make_object(std::make_shared<IntObject>(idx)), value);
    return value;
}

// Set native methods
static Object_ptr native_set_size(const std::vector<Object_ptr>& args)
{
    auto self = args[0]->as<SetObject_ptr>();
    return make_object(std::make_shared<IntObject>(self->get_length()));
}

static Object_ptr native_set_contains(const std::vector<Object_ptr>& args)
{
    auto self = args[0]->as<SetObject_ptr>();
    auto value = args[1];

    for (const auto& elem : self->values)
    {
        if (Object::are_equal_types(elem, value))
        {
            return make_object(std::make_shared<BooleanObject>(true));
        }
    }
    return make_object(std::make_shared<BooleanObject>(false));
}

static Object_ptr native_set_add(const std::vector<Object_ptr>& args)
{
    auto self = args[0]->as<SetObject_ptr>();
    auto value = args[1];

    // Check if already exists
    for (const auto& elem : self->values)
    {
        if (Object::are_equal_types(elem, value))
        {
            return value; // Already present
        }
    }

    self->values.push_back(value);
    return value;
}

static Object_ptr native_set_remove(const std::vector<Object_ptr>& args)
{
    auto self = args[0]->as<SetObject_ptr>();
    auto value = args[1];

    for (auto it = self->values.begin(); it != self->values.end(); ++it)
    {
        if (Object::are_equal_types(*it, value))
        {
            self->values.erase(it);
            break;
        }
    }

    return value;
}

// Map native methods
static Object_ptr native_map_size(const std::vector<Object_ptr>& args)
{
    auto self = args[0]->as<MapObject_ptr>();
    return make_object(std::make_shared<IntObject>(self->get_size()));
}

static Object_ptr native_map_get(const std::vector<Object_ptr>& args)
{
    auto self = args[0]->as<MapObject_ptr>();
    auto key = args[1];

    return self->get(key);
}

static Object_ptr native_map_set(const std::vector<Object_ptr>& args)
{
    auto self = args[0]->as<MapObject_ptr>();
    auto key = args[1];
    auto value = args[2];

    self->set(key, value);
    return value;
}

static Object_ptr native_map_contains_key(const std::vector<Object_ptr>& args)
{
    auto self = args[0]->as<MapObject_ptr>();
    auto key = args[1];

    auto result = self->get(key);
    return make_object(std::make_shared<BooleanObject>(result != nullptr));
}

static Object_ptr native_map_remove(const std::vector<Object_ptr>& args)
{
    auto self = args[0]->as<MapObject_ptr>();
    auto key = args[1];

    auto value = self->get(key);
    if (value)
    {
        // Remove by setting to none or erase
        self->set(key, make_object(std::make_shared<NoneObject>()));
    }

    return value ? value : make_object(std::make_shared<NoneObject>());
}

// Tuple native methods
static Object_ptr native_tuple_get(const std::vector<Object_ptr>& args)
{
    auto self = args[0]->as<TupleObject_ptr>();
    auto index = args[1]->as<IntObject_ptr>();

    int idx = index->value;
    int len = static_cast<int>(self->values.size());

    // Handle negative indexing
    if (idx < 0)
    {
        idx = len + idx;
    }

    Doctor::get().assert(
        idx >= 0 && idx < len,
        WaspStage::VM,
        "Tuple index out of bounds: index " + std::to_string(index->value) +
            ", size " + std::to_string(len)
    );

    return self->values[idx];
}

static Object_ptr native_tuple_size(const std::vector<Object_ptr>& args)
{
    auto self = args[0]->as<TupleObject_ptr>();
    return make_object(std::make_shared<IntObject>(self->values.size()));
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

    auto obj = make_object(
        std::make_shared<NativeFunctionRuntimeObject>(function, name)
    );
    native_objects.push_back(obj);
}

void NativeRegistry::load_stdlib()
{
    // IO
    add_native(
        "libs.core.io.print",
        [this](const std::vector<Object_ptr>& args)
        {
            return native_print(args, this->pool);
        }
    );

    add_native(
        "libs.core.io.input",
        [](const std::vector<Object_ptr>& args)
        {
            return native_input(args);
        }
    );

    // STR
    add_native("libs.core.types.str.hash", native_str_hash);
    add_native("libs.core.types.str.size", native_str_size);
    add_native("libs.core.types.str.slice", native_str_slice);
    add_native("libs.core.types.str.trim", native_str_trim);

    // LIST
    add_native("libs.core.types.list.size", native_list_size);
    add_native("libs.core.types.list.get", native_list_get);
    add_native("libs.core.types.list.set", native_list_set);

    // SET
    add_native("libs.core.types.set.size", native_set_size);
    add_native("libs.core.types.set.contains", native_set_contains);
    add_native("libs.core.types.set.add", native_set_add);
    add_native("libs.core.types.set.remove", native_set_remove);

    // MAP
    add_native("libs.core.types.map.size", native_map_size);
    add_native("libs.core.types.map.get", native_map_get);
    add_native("libs.core.types.map.set", native_map_set);
    add_native("libs.core.types.map.contains_key", native_map_contains_key);
    add_native("libs.core.types.map.remove", native_map_remove);

    // TUPLE
    add_native("libs.core.types.tuple.get", native_tuple_get);
    add_native("libs.core.types.tuple.size", native_tuple_size);
}

} // namespace Wasp
