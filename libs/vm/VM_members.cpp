#include "Doctor.h"
#include "Objects.h"
#include "OpCode.h"
#include "VM.h"

#include <iostream>
#include <memory>
#include <string>
#include <variant>

template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp {
void VM::execute_member(OpCode op, CallFrame* frame) {
    //  Get the property name from the constant pool
    int name_index = static_cast<int>(frame->consume_byte());
    Object_ptr name_obj = workspace->pool->get(name_index);

    Doctor::get().assert(
        std::holds_alternative<StringObject>(name_obj->value),
        WaspStage::VM,
        "Member name in constant pool must be a string."
    );

    std::string member_name = std::get<StringObject>(name_obj->value).value;

    // Perform the operation
    if (op == OpCode::GET_MEMBER) {
        Object_ptr obj = pop_from_stack();
        Doctor::get().fatal_if_nullptr(
            obj, WaspStage::VM, "Cannot read property '" + member_name + "' of null."
        );

        push_to_stack(perform_get_member(obj, member_name));
    } else if (op == OpCode::SET_MEMBER) {
        Object_ptr val = pop_from_stack(); // Pushed second
        Object_ptr obj = pop_from_stack(); // Pushed first
        perform_set_member(obj, member_name, val);
    }
}

Object_ptr VM::perform_get_member(Object_ptr obj, const std::string& name) {
    return std::visit(
        overloaded{
            [&](std::shared_ptr<ModuleObject>& module_obj) -> Object_ptr {
                Object_ptr result = module_obj->get_member(name);
                Doctor::get().fatal_if_nullptr(
                    result,
                    WaspStage::VM,
                    "Module '" + module_obj->name + "' has no member named '" + name
                );
                return result;
            },
            [&](auto&) -> Object_ptr {
                std::cout << "[VM BUG] perform_get_member expected a ModuleObject, but got Variant "
                             "Index: "
                          << obj->value.index() << "\n";

                Doctor::get().fatal(
                    WaspStage::VM, "Object of variant type does not support reading properties."
                );
                return nullptr;
            }
        },
        obj->value
    );
}

void VM::perform_set_member(Object_ptr obj, const std::string& name, Object_ptr value) {
    std::visit(
        overloaded{
            [&](std::shared_ptr<ModuleObject>& module_obj) { module_obj->set_member(name, value); },
            [&](auto&) {
                Doctor::get().fatal(WaspStage::VM, "Object does not support setting properties.");
            }
        },
        obj->value
    );
}
} // namespace Wasp
