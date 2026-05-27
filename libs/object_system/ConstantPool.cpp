#include "ConstantPool.h"
#include "CFGraph.h"
#include "Doctor.h"
#include "Objects.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Wasp
{

// ============================================================================
// ConstantPool
// ============================================================================

ConstantPool::ConstantPool()
{
    objects.push_back(std::make_shared<Object>(std::make_shared<AnyType>()));
    objects.push_back(
        std::make_shared<Object>(std::make_shared<BooleanObject>(true))
    );
    objects.push_back(
        std::make_shared<Object>(std::make_shared<BooleanObject>(false))
    );
    objects.push_back(std::make_shared<Object>(std::make_shared<NoneObject>()));
}

Object_ptr ConstantPool::get(int id) const
{
    Doctor::get().assert(
        id >= 0 && id < static_cast<int>(objects.size()),
        WaspStage::VM,
        "ID out of bounds in ConstantPool"
    );
    return objects[id];
}

Object_ptr ConstantPool::get_any_type() const
{
    return get(0);
}

Object_ptr ConstantPool::get_true_object() const
{
    return get(1);
}

Object_ptr ConstantPool::get_false_object() const
{
    return get(2);
}

Object_ptr ConstantPool::get_none_object() const
{
    return get(3);
}

Object_ptr ConstantPool::get_int_type() const
{
    return std::make_shared<Object>(std::make_shared<IntType>());
}

Object_ptr ConstantPool::get_float_type() const
{
    return std::make_shared<Object>(std::make_shared<FloatType>());
}

Object_ptr ConstantPool::get_string_type() const
{
    return std::make_shared<Object>(std::make_shared<StringType>());
}

Object_ptr ConstantPool::get_boolean_type() const
{
    return std::make_shared<Object>(std::make_shared<BooleanType>());
}

Object_ptr ConstantPool::get_none_type() const
{
    return std::make_shared<Object>(std::make_shared<NoneType>());
}

Object_ptr ConstantPool::make_object(bool value) const
{
    return value ? get_true_object() : get_false_object();
}

Object_ptr ConstantPool::make_object(int value) const
{
    return std::make_shared<Object>(std::make_shared<IntObject>(value));
}

Object_ptr ConstantPool::make_object(double value) const
{
    return std::make_shared<Object>(std::make_shared<FloatObject>(value));
}

Object_ptr ConstantPool::make_object(std::string value) const
{
    return std::make_shared<Object>(
        std::make_shared<StringObject>(std::move(value))
    );
}

int ConstantPool::allocate(Object_ptr value)
{
    int id = static_cast<int>(objects.size());
    objects.push_back(std::move(value));
    return id;
}

int ConstantPool::allocate(int number)
{
    if (auto it = int_cache.find(number); it != int_cache.end())
    {
        return it->second;
    }

    int id = static_cast<int>(objects.size());
    objects.push_back(
        std::make_shared<Object>(std::make_shared<IntObject>(number))
    );
    int_cache[number] = id;
    return id;
}

int ConstantPool::allocate(double number)
{
    if (auto it = float_cache.find(number); it != float_cache.end())
    {
        return it->second;
    }

    int id = static_cast<int>(objects.size());
    objects.push_back(
        std::make_shared<Object>(std::make_shared<FloatObject>(number))
    );
    float_cache[number] = id;
    return id;
}

int ConstantPool::allocate(std::string text)
{
    if (auto it = string_cache.find(text); it != string_cache.end())
    {
        return it->second;
    }

    int id = static_cast<int>(objects.size());
    objects.push_back(
        std::make_shared<Object>(std::make_shared<StringObject>(text))
    );
    string_cache[std::move(text)] = id;
    return id;
}

int ConstantPool::allocate_type(Object_ptr new_value)
{
    auto result = std::find_if(
        objects.begin(),
        objects.end(),
        [new_value](const auto& obj)
        {
            return Object::are_equal_types(new_value, obj);
        }
    );

    if (result != objects.end())
    {
        return static_cast<int>(std::distance(objects.begin(), result));
    }

    int id = static_cast<int>(objects.size());
    objects.push_back(std::move(new_value));
    return id;
}

int ConstantPool::allocate_function_definition(
    CodeObject code,
    std::string name
)
{
    auto func_obj = std::make_shared<FunctionBlueprintObject>(
        std::move(code),
        std::move(name)
    );
    int id = static_cast<int>(objects.size());
    objects.push_back(std::make_shared<Object>(func_obj));
    return id;
}

} // namespace Wasp
