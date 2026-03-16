#pragma once

#include "ConstantPool.h"
#include "Objects.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wasp {
class NativeRegistry {
    ConstantPool_ptr pool;

    std::vector<Object_ptr> native_objects;
    std::vector<Object_ptr> native_object_types;
    std::unordered_map<std::string, int> native_names;

    void load_stdlib();

    void add_native(
        const std::string& name,
        int arity,
        NativeFnType function,
        ObjectVector input_types,
        Object_ptr return_type
    );

public:
    NativeRegistry(ConstantPool_ptr pool) : pool(pool) { load_stdlib(); };

    Object_ptr get_native_object(int index) const;
    Object_ptr get_native_object_type(int index) const;

    int get_native_index(const std::string& name) const;

    std::unordered_map<std::string, int> get_all_native_names() const { return native_names; }

    int get_size() const { return static_cast<int>(native_objects.size()); }
};

using NativeRegistry_ptr = std::shared_ptr<NativeRegistry>;
} // namespace Wasp
