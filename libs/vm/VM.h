#pragma once

#include "CFGraph.h"
#include "Objects.h"
#include "OpCode.h"
#include "Workspace.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Wasp {
struct CallFrame {
    FunctionVMObject_ptr function;
    size_t ip = 0;
    // Where locals start on the VM stack
    size_t base_pointer = 0;

    std::vector<size_t> scope_bases;

    CallFrame(FunctionVMObject_ptr func, size_t bp) : function(std::move(func)), base_pointer(bp) {}

    std::byte consume_byte() { return function->code.data()[ip++]; }
};

using CallFrame_ptr = std::shared_ptr<CallFrame>;

class VM {
    ObjectVector stack;
    std::vector<CallFrame> frames;
    Workspace_ptr workspace;

    void push_to_stack(Object_ptr value);
    Object_ptr pop_from_stack();
    ObjectVector pop_n_from_stack(size_t n);
    Object_ptr peek_tos(size_t distance = 0) const;

    void execute_binary_op(OpCode op);
    void execute_unary_op(OpCode op);
    void execute_stack_op(OpCode op);

    void execute_constant(OpCode op, CallFrame* frame);
    void execute_control_flow(OpCode op, CallFrame* frame);
    void execute_variable(OpCode op, CallFrame* frame);

    void execute_make_function(CallFrame* frame);
    void execute_call(CallFrame* frame);
    void execute_return(CallFrame* frame);

    void execute_import(CallFrame* frame);
    void execute_exit_module();

    void execute_member(OpCode op, CallFrame* frame);
    Object_ptr perform_get_member(Object_ptr obj, const std::string& name);
    void perform_set_member(Object_ptr obj, const std::string& name, Object_ptr value);

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
    VM(Workspace_ptr workspace) : workspace(workspace) { stack.reserve(256); }

    void run(FunctionObject_ptr main_function);
};
} // namespace Wasp
