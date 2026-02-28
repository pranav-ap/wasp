#pragma once

#include "Objects.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>

namespace Wasp
{

    class ConstantPool
    {
    private:
        std::vector<Object_ptr> objects;

        std::unordered_map<int, int> int_cache;
        std::unordered_map<double, int> float_cache;
        std::unordered_map<std::string, int> string_cache;

    public:
        ConstantPool();
        ~ConstantPool() = default;

        Object_ptr get(int id) const;

        Object_ptr get_any_type() const;
        Object_ptr get_int_type() const;
        Object_ptr get_float_type() const;
        Object_ptr get_string_type() const;
        Object_ptr get_boolean_type() const;
        Object_ptr get_none_type() const;

        Object_ptr get_true_literal_type() const;
        Object_ptr get_false_literal_type() const;

        Object_ptr get_true_object() const;
        Object_ptr get_false_object() const;

        Object_ptr make_object(bool value) const;
        Object_ptr make_object(int value) const;
        Object_ptr make_object(double value) const;
        Object_ptr make_object(std::string value) const;
        Object_ptr make_error_object(std::string text) const;

        int allocate(Object_ptr value);
        int allocate(int value);
        int allocate(double value);
        int allocate(std::string value);
        int allocate_type(Object_ptr value);
    };

    class DefinitionStore
    {
    private:
        std::vector<Object_ptr> locals;

    public:
        DefinitionStore(size_t expected_size = 0);
        ~DefinitionStore() = default;

        void create(int id, Object_ptr value);
        void set(int id, Object_ptr value);
        Object_ptr get(int id) const;
        void discard(int id);
    };

    using ConstantPool_ptr = std::shared_ptr<ConstantPool>;
    using DefinitionStore_ptr = std::shared_ptr<DefinitionStore>;

}
