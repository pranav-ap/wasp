#include "Doctor.h"
#include "Objects.h"
#include "VM.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void VM::execute_import_module(CallFrame* frame)
{
    int module_index = static_cast<int>(frame->consume_byte());

    auto target_module = workspace->get_module(module_index);

    Doctor::get().fatal_if_nullptr(
        target_module,
        WaspStage::VM,
        "Module not found at registry index: " + std::to_string(module_index)
    );

    void* blueprint_ptr = target_module->blueprint.get();
    if (evaluated_modules.contains(blueprint_ptr))
    {
        push_to_stack(evaluated_modules.at(blueprint_ptr));
        return;
    }

    auto module_func = std::make_shared<FunctionRuntimeObject>(
        target_module->blueprint
    );

    size_t stack_base_pointer = stack.size();
    frames.emplace_back(module_func, stack_base_pointer);
}

void VM::execute_exit_module(CallFrame* frame)
{
    int export_count = static_cast<int>(frame->consume_byte());
    ObjectVector exported_members(export_count);

    for (int i = export_count - 1; i >= 0; i--)
    {
        exported_members[i] = pop_from_stack();
    }

    auto exports = std::make_shared<ModuleObject>(
        frame->function->blueprint->name,
        std::move(exported_members)
    );

    auto module_obj = make_object(exports);

    void* blueprint_ptr = frame->function->blueprint.get();
    evaluated_modules[blueprint_ptr] = module_obj;

    size_t bp = frame->base_pointer;
    stack.erase(stack.begin() + static_cast<int>(bp), stack.end());
    frames.pop_back();

    push_to_stack(module_obj);
}

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
    auto mod = module_obj->as<ModuleObject_ptr>();

    // Loop through the remaining bytes to fetch each member
    for (int i = 0; i < count; i++)
    {
        int member_index = static_cast<int>(
            std::to_integer<int>(frame->consume_byte())
        );

        auto member = mod->get_member(member_index);
        Doctor::get().fatal_if_nullptr(member, WaspStage::VM);

        push_to_stack(member);
    }
}

} // namespace Wasp
