#pragma once

#include "Objects.h"
#include "ConstantPool.h"

#include <unordered_map>
#include <string>
#include <vector>
#include <iostream>

namespace Wasp
{
    class NativeRegistry
    {
    public:
        // For the VM: The actual list of C++ function objects
        std::vector<Object_ptr> native_objects;

        // For the Compiler: Maps the string "print" to its integer index in the array
        std::unordered_map<std::string, int> native_names;

        void add_native(const std::string &name, int arity, NativeFnType function);
        void load_stdlib(ConstantPool_ptr pool);
    };
}