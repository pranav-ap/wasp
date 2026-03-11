#pragma once

#include "ConstantPool.h"
#include "NativeRegistry.h"
#include "Objects.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace Wasp {
struct CallFrame {
    std::shared_ptr<FunctionObject> function;
    size_t ip = 0;
    // Where locals start on the VM stack
    size_t base_pointer = 0;

    std::vector<size_t> scope_bases;

    CallFrame(std::shared_ptr<FunctionObject> func, size_t bp)
        : function(std::move(func)), base_pointer(bp) {}

    std::byte consume_byte() { return function->code.data()[ip++]; }
};

using CallFrame_ptr = std::shared_ptr<CallFrame>;

class VM {
    ObjectVector stack;
    std::vector<CallFrame> frames;
    ConstantPool_ptr pool;
    NativeRegistry_ptr native_registry;

    void push_to_stack(Object_ptr value);
    Object_ptr pop_from_stack();
    ObjectVector pop_n_from_stack(size_t n);
    Object_ptr peek_tos(size_t distance = 0) const;

    void execute();

    // Unary Ops

    Object_ptr perform_unary_negative(Object_ptr obj);
    Object_ptr perform_unary_not(Object_ptr obj);

    // Binary Ops

    Object_ptr perform_add(Object_ptr left, Object_ptr right);
    Object_ptr perform_subtract(Object_ptr left, Object_ptr right);
    Object_ptr perform_multiply(Object_ptr left, Object_ptr right);
    Object_ptr perform_divide(Object_ptr left, Object_ptr right);
    Object_ptr perform_reminder(Object_ptr left, Object_ptr right);
    Object_ptr perform_power(Object_ptr left, Object_ptr right);

    // Logical Ops

    Object_ptr perform_logical_and(Object_ptr left, Object_ptr right);
    Object_ptr perform_logical_or(Object_ptr left, Object_ptr right);

    // Comaprison Ops

    Object_ptr perform_equal(Object_ptr left, Object_ptr right);
    Object_ptr perform_not_equal(Object_ptr left, Object_ptr right);
    Object_ptr perform_lesser_than(Object_ptr left, Object_ptr right);
    Object_ptr perform_lesser_than_equal(Object_ptr left, Object_ptr right);
    Object_ptr perform_greater_than(Object_ptr left, Object_ptr right);
    Object_ptr perform_greater_than_equal(Object_ptr left, Object_ptr right);

    // Utils

    bool is_truthy(Object_ptr obj) const;

public:
    VM(std::shared_ptr<ConstantPool> pool, NativeRegistry_ptr native_registry)
        : pool(std::move(pool)), native_registry(native_registry) {
        stack.reserve(256);
    }

    void run(std::shared_ptr<FunctionObject> main_module) {
        // Push the initial frame for the entry point
        frames.emplace_back(main_module, 0);
        execute();
    }
};
} // namespace Wasp
