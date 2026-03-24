#include "Doctor.h"
#include "Objects.h"
#include "OpCode.h"
#include "VM.h"

#include <memory>
#include <string>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void VM::execute_member(OpCode op, CallFrame* frame)
{
    int member_index = static_cast<int>(frame->consume_byte());

    if (op == OpCode::GET_MEMBER)
    {
        Object_ptr obj = pop_from_stack();

        Doctor::get().fatal_if_nullptr(obj, WaspStage::VM, "Cannot read property of null.");

        push_to_stack(perform_get_member(obj, member_index));
    }
    else if (op == OpCode::SET_MEMBER)
    {
        Object_ptr val = pop_from_stack();
        Object_ptr obj = pop_from_stack();

        Doctor::get().fatal_if_nullptr(obj, WaspStage::VM, "Cannot set property on null.");

        perform_set_member(obj, member_index, val);

        // Put the value back on the stack so expression cleanups (POP) work correctly
        push_to_stack(val);
    }
}

Object_ptr VM::perform_get_member(Object_ptr obj, int member_index)
{
    return std::visit(
        overloaded{
            [&](std::shared_ptr<ModuleObject>& module_obj) -> Object_ptr
            {
                Doctor::get().assert(
                    member_index >= 0 && member_index < module_obj->members.size(),
                    WaspStage::VM,
                    "Member index out of bounds!");

                return module_obj->members[member_index];
            },
            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::VM,
                    "Object of this type does not support reading properties.");
                return nullptr;
            }},
        obj->value);
}

void VM::perform_set_member(Object_ptr obj, int member_index, Object_ptr value)
{
    std::visit(
        overloaded{
            [&](std::shared_ptr<ModuleObject>& module_obj)
            {
                Doctor::get().assert(
                    member_index >= 0 && member_index < module_obj->members.size(),
                    WaspStage::VM,
                    "Member index out of bounds!");

                module_obj->members[member_index] = value;
            },
            [&](auto&)
            { Doctor::get().fatal(WaspStage::VM, "Object does not support setting properties."); }},
        obj->value);
}

} // namespace Wasp
