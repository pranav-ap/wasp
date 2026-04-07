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

void VM::execute_instantiate(CallFrame* frame)
{
    // Read the number of arguments passed to the constructor
    int arg_count = static_cast<int>(std::to_integer<int>(frame->consume_byte()));

    ObjectVector args = pop_n_from_stack(arg_count);

    Object_ptr blueprint_obj = pop_from_stack();

    Doctor::get().assert(
        blueprint_obj->is<ClassType>(),
        WaspStage::VM,
        "INSTANTIATE expects a ClassType blueprint on the stack!"
    );

    auto& blueprint = blueprint_obj->as<ClassType>();

    Doctor::get().assert(
        arg_count == blueprint.declaration_order.size(),
        WaspStage::VM,
        "Arity mismatch for class " + blueprint.class_name
    );

    auto instance = make_object(std::make_shared<InstanceObject>(std::move(args)));

    push_to_stack(instance);
}
} // namespace Wasp
