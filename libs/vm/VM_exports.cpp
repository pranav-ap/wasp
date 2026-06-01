#include "Doctor.h"
#include "Objects.h"
#include "VM.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
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
    stack.erase(stack.begin() + static_cast<ptrdiff_t>(bp), stack.end());
    frames.pop_back();

    push_to_stack(module_obj);
}

} // namespace Wasp
