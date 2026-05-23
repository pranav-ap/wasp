#include "Doctor.h"
#include "Objects.h"

#include <string>
#include <utility>

namespace Wasp
{

Object_ptr ClassBlueprintObject::get_method(int member_id) const
{
    Doctor::get().assert(
        member_id >= 0 && member_id < static_cast<int>(methods.size()),
        WaspStage::VM,
        "Member index out of bounds!"
    );

    return methods.at(member_id);
}

Object_ptr ClassInstanceObject::get_field(int member_id) const
{
    Doctor::get().assert(
        member_id >= 0 && member_id < static_cast<int>(fields.size()),
        WaspStage::VM,
        "Member index out of bounds!"
    );

    return fields.at(member_id);
}

void ClassInstanceObject::set_field(int member_id, Object_ptr value)
{
    Doctor::get().assert(
        member_id >= 0 && member_id < static_cast<int>(fields.size()),
        WaspStage::VM,
        "Member index out of bounds!"
    );

    fields[member_id] = std::move(value);
}

} // namespace Wasp
