#include "Doctor.h"
#include "Objects.h"
#include "VM.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void VM::execute_instantiate(CallFrame* frame)
{
    // Data + Methods
    int total_size = static_cast<int>(std::to_integer<int>(frame->consume_byte()));

    ObjectVector memory = pop_n_from_stack(total_size);
    Object_ptr blueprint_obj = pop_from_stack();

    Doctor::get().assert(
        blueprint_obj->is<ClassType>(),
        WaspStage::VM,
        "OpCode::INSTANTIATE expects a ClassType blueprint on the stack!"
    );

    auto& blueprint = blueprint_obj->as<ClassType>();

    Doctor::get().assert(
        total_size == blueprint.declaration_order.size(),
        WaspStage::VM,
        "Arity mismatch for class " + blueprint.class_name
    );

    auto instance = make_object(std::make_shared<InstanceObject>(std::move(memory)));

    push_to_stack(instance);
}
} // namespace Wasp
