#include "Doctor.h"
#include "Objects.h"
#include "VM.h"

#include <cstddef>
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

void VM::execute_GET_IMPORTED_MEMBER(CallFrame* frame)
{
    int member_index = static_cast<int>(frame->consume_byte());

    Object_ptr obj = pop_from_stack();
    Doctor::get()
        .fatal_if_nullptr(obj, WaspStage::VM, "Cannot read property of null.");

    auto member_value = std::visit(
        overloaded{
            [&](ModuleObject_ptr mod) -> Object_ptr
            {
                return mod->get_member(member_index);
            },
            [&](auto&) -> Object_ptr
            {
                Doctor::get().fatal(
                    WaspStage::VM,
                    "Object of this type does not support reading properties."
                );
            }
        },
        obj->value
    );

    Doctor::get().fatal_if_nullptr(
        member_value,
        WaspStage::VM,
        "Member value is null. This should not happen."
    );

    push_to_stack(member_value);
}

void VM::execute_SET_IMPORTED_MEMBER(CallFrame* frame)
{
    int member_index = static_cast<int>(frame->consume_byte());

    Object_ptr value = pop_from_stack();
    Doctor::get().fatal_if_nullptr(value, WaspStage::VM);

    Object_ptr obj = pop_from_stack();
    Doctor::get().fatal_if_nullptr(obj, WaspStage::VM);

    std::visit(
        overloaded{
            [&](ModuleObject_ptr mod)
            {
                mod->set_member(member_index, value);
            },
            [&](auto&)
            {
                Doctor::get().fatal(
                    WaspStage::VM,
                    "Object does not support setting properties."
                );
            }
        },
        obj->value
    );

    push_to_stack(value);
}

void VM::execute_UNPACK_MODULE_MEMBERS(CallFrame* frame)
{
    int module_slot = static_cast<int>(
        std::to_integer<int>(frame->consume_byte())
    );

    int count = static_cast<int>(std::to_integer<int>(frame->consume_byte()));

    Object_ptr module_obj = stack[frame->base_pointer + module_slot];

    // Loop through the remaining bytes to fetch each member
    for (int i = 0; i < count; i++)
    {
        int member_index = static_cast<int>(
            std::to_integer<int>(frame->consume_byte())
        );

        Doctor::get().assert(
            module_obj->is<ModuleObject_ptr>(),
            WaspStage::VM,
            "Expected a module object for UNPACK_MODULE_MEMBERS"
        );

        auto m = module_obj->as<ModuleObject_ptr>();
        auto member_value = m->get_member(member_index);

        Doctor::get().fatal_if_nullptr(
            member_value,
            WaspStage::VM,
            "Member value is null. This should not happen."
        );

        push_to_stack(member_value);
    }
}

} // namespace Wasp
