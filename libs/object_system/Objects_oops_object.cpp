#include "Doctor.h"
#include "Objects.h"

#include <string>
#include <utility>

namespace Wasp
{

Object_ptr MemberedCompositeObject::get_member(int member_id) const
{
    Doctor::get().assert(
        member_id >= 0 && member_id < static_cast<int>(members.size()),
        WaspStage::VM,
        "Member index out of bounds!"
    );
    return members[member_id];
}

void MemberedCompositeObject::set_member(int member_id, Object_ptr value)
{
    Doctor::get().assert(
        member_id >= 0 && member_id < static_cast<int>(members.size()),
        WaspStage::VM,
        "Member index out of bounds!"
    );
    members[member_id] = std::move(value);
}

int MemberedCompositeObject::get_member_count() const
{
    return members.size();
}

} // namespace Wasp
