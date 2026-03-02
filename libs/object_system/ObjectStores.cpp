#include "ObjectStores.h"
#include <stdexcept>
#include <algorithm>

#ifndef ASSERT
#include <cassert>
#define ASSERT(condition, message) assert((condition) && message)
#endif

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)
#define MAKE_SHARED_OBJECT_VARIANT(Type, ...) std::make_shared<Object>(std::make_shared<Type>(__VA_ARGS__))

namespace Wasp
{

    // ============================================================================
    // ConstantPool
    // ============================================================================

    ConstantPool::ConstantPool()
    {
        objects.push_back(MAKE_OBJECT_VARIANT(AnyType()));                 // 0
        objects.push_back(MAKE_OBJECT_VARIANT(IntType()));                 // 1
        objects.push_back(MAKE_OBJECT_VARIANT(FloatType()));               // 2
        objects.push_back(MAKE_OBJECT_VARIANT(StringType()));              // 3
        objects.push_back(MAKE_OBJECT_VARIANT(BooleanType()));             // 4
        objects.push_back(MAKE_OBJECT_VARIANT(NoneType()));                // 5
        objects.push_back(MAKE_OBJECT_VARIANT(BooleanLiteralType(true)));  // 6
        objects.push_back(MAKE_OBJECT_VARIANT(BooleanLiteralType(false))); // 7
        objects.push_back(MAKE_OBJECT_VARIANT(BooleanObject(true)));       // 8
        objects.push_back(MAKE_OBJECT_VARIANT(BooleanObject(false)));      // 9
    }

    Object_ptr ConstantPool::get(int id) const
    {
        ASSERT(id >= 0 && id < objects.size(), "ID out of bounds in ConstantPool");
        return objects[id];
    }

    Object_ptr ConstantPool::get_any_type() const { return get(0); }
    Object_ptr ConstantPool::get_int_type() const { return get(1); }
    Object_ptr ConstantPool::get_float_type() const { return get(2); }
    Object_ptr ConstantPool::get_string_type() const { return get(3); }
    Object_ptr ConstantPool::get_boolean_type() const { return get(4); }
    Object_ptr ConstantPool::get_none_type() const { return get(5); }
    Object_ptr ConstantPool::get_true_literal_type() const { return get(6); }
    Object_ptr ConstantPool::get_false_literal_type() const { return get(7); }
    Object_ptr ConstantPool::get_true_object() const { return get(8); }
    Object_ptr ConstantPool::get_false_object() const { return get(9); }

    Object_ptr ConstantPool::make_object(bool value) const
    {
        return value ? get_true_object() : get_false_object();
    }

    Object_ptr ConstantPool::make_object(int value) const
    {
        return MAKE_OBJECT_VARIANT(IntObject(value));
    }

    Object_ptr ConstantPool::make_object(double value) const
    {
        return MAKE_OBJECT_VARIANT(FloatObject(value));
    }

    Object_ptr ConstantPool::make_object(std::string value) const
    {
        return MAKE_OBJECT_VARIANT(StringObject(std::move(value)));
    }

    Object_ptr ConstantPool::make_error_object(std::string text) const
    {
        return MAKE_OBJECT_VARIANT(std::make_shared<ErrorObject>(std::move(text)));
    }

    int ConstantPool::allocate(Object_ptr value)
    {
        int id = objects.size();
        objects.push_back(std::move(value));
        return id;
    }

    int ConstantPool::allocate(int number)
    {
        if (auto it = int_cache.find(number); it != int_cache.end())
        {
            return it->second;
        }

        int id = objects.size();
        objects.push_back(MAKE_OBJECT_VARIANT(IntObject(number)));
        int_cache[number] = id;
        return id;
    }

    int ConstantPool::allocate(double number)
    {
        if (auto it = float_cache.find(number); it != float_cache.end())
        {
            return it->second;
        }

        int id = objects.size();
        objects.push_back(MAKE_OBJECT_VARIANT(FloatObject(number)));
        float_cache[number] = id;
        return id;
    }

    int ConstantPool::allocate(std::string text)
    {
        if (auto it = string_cache.find(text); it != string_cache.end())
        {
            return it->second;
        }

        int id = objects.size();
        objects.push_back(MAKE_OBJECT_VARIANT(StringObject(text)));
        string_cache[std::move(text)] = id;
        return id;
    }

    int ConstantPool::allocate_type(Object_ptr new_value)
    {
        auto result = std::find_if(
            objects.begin(), objects.end(),
            [new_value](const auto &obj)
            {
                return are_equal_types(new_value, obj);
            });

        if (result != objects.end())
        {
            return std::distance(objects.begin(), result);
        }

        int id = objects.size();
        objects.push_back(std::move(new_value));
        return id;
    }

    int ConstantPool::allocate_function_definition(CodeObject func_code)
    {
        int id = objects.size();
        objects.push_back(MAKE_SHARED_OBJECT_VARIANT(FunctionObject, std::move(func_code)));
        return id;
    }

    // ============================================================================
    // DefinitionStore
    // ============================================================================

    DefinitionStore::DefinitionStore(size_t expected_size)
    {
        locals.resize(expected_size);
    }

    void DefinitionStore::create(int id, Object_ptr value)
    {
        if (id >= locals.size())
        {
            locals.resize(id + 1);
        }
        ASSERT(locals[id] == nullptr, "ID already exists in DefinitionStore");
        locals[id] = std::move(value);
    }

    void DefinitionStore::set(int id, Object_ptr value)
    {
        if (id >= locals.size())
        {
            locals.resize(id + 1);
        }
        locals[id] = std::move(value);
    }

    Object_ptr DefinitionStore::get(int id) const
    {
        ASSERT(id >= 0 && id < locals.size(), "ID out of bounds in DefinitionStore");
        return locals[id];
    }

    void DefinitionStore::discard(int id)
    {
        ASSERT(id >= 0 && id < locals.size(), "ID out of bounds in DefinitionStore");
        // Null the slot to free the shared_ptr
        // maintains vector length to maintain other IDs
        locals[id] = nullptr;
    }

}
