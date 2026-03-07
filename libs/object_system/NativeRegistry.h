#pragma once

#include "Objects.h"
#include "ConstantPool.h"

#include <unordered_map>
#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <map>

namespace Wasp
{
    class NativeRegistry
    {
        ConstantPool_ptr pool;

        std::vector<Object_ptr> native_objects;
        std::vector<Object_ptr> native_object_types;
        std::unordered_map<std::string, int> native_names;

    public:
        NativeRegistry(ConstantPool_ptr pool) : pool(pool) {};

        Object_ptr get_native_object(int index) const;
        Object_ptr get_native_object_type(int index) const;

        int get_native_index(const std::string &name) const;

        std::unordered_map<std::string, int> get_all_native_names() const
        {
            return native_names;
        }

        void add_native(
            const std::string &name,
            int arity,
            NativeFnType function,
            ObjectVector input_types,
            Object_ptr return_type);

        void load_stdlib();
    };

    using NativeRegistry_ptr = std::shared_ptr<NativeRegistry>;
}