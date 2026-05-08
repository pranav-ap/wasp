#pragma once

#include "CFGraph.h"
#include "Objects.h"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>


namespace Wasp
{

    class ConstantPool
    {
    private:
        std::vector<Object_ptr> objects;

        std::unordered_map<int, int> int_cache;
        std::unordered_map<double, int> float_cache;
        std::unordered_map<std::string, int> string_cache;
        std::unordered_map<std::string, int> function_group_cache;

    public:
        ConstantPool();
        ~ConstantPool() = default;

        Object_ptr get(int id) const;
        size_t get_size() const { return objects.size(); }

        Object_ptr get_native_any_type() const;
        Object_ptr get_native_int_type() const;
        Object_ptr get_native_float_type() const;
        Object_ptr get_native_string_type() const;
        Object_ptr get_boolean_type() const;
        Object_ptr get_none_type() const;

        Object_ptr get_int_default() const
        {
            return get(11);
        }
        Object_ptr get_float_default() const
        {
            return get(12);
        }
        Object_ptr get_string_default() const
        {
            return get(13);
        }
        Object_ptr get_boolean_default() const
        {
            return get(14);
        }

        Object_ptr get_true_literal_type() const;
        Object_ptr get_false_literal_type() const;

        Object_ptr get_true_object() const;
        Object_ptr get_false_object() const;

        Object_ptr get_none_object() const;

        Object_ptr make_object(bool value) const;
        Object_ptr make_object(int value) const;
        Object_ptr make_object(double value) const;
        Object_ptr make_object(std::string value) const;
        Object_ptr make_error_object(std::string text) const;

        int allocate(Object_ptr value);
        int allocate(int value);
        int allocate(double value);
        int allocate(std::string value);

        int allocate_function_definition(FunctionBlueprintObject_ptr func_obj);
        int allocate_function_definition(CodeObject code, std::string name);

        int allocate_class_definition(Object_ptr class_obj);

        int allocate_type(Object_ptr value);
    };

    using ConstantPool_ptr = std::shared_ptr<ConstantPool>;
}
