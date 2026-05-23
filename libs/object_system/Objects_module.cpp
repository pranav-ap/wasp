#include "Doctor.h"
#include "Objects.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>

namespace Wasp
{

// ============================================================================
// Objects
// ===========================================================================

Object_ptr ModuleObject::get_member(int member_id) const
{
    Doctor::get().assert(
        member_id >= 0 && member_id < static_cast<int>(members.size()),
        WaspStage::VM,
        "Member index out of bounds!"
    );
    return members[member_id];
}

void ModuleObject::set_member(int member_id, Object_ptr value)
{
    Doctor::get().assert(
        member_id >= 0 && member_id < static_cast<int>(members.size()),
        WaspStage::VM,
        "Member index out of bounds!"
    );
    members[member_id] = std::move(value);
}

// ============================================================================
// Module Type
// ============================================================================

bool BaseMemberedType::contains_member(const std::string& member_name) const
{
    return member_types.contains(member_name);
}

Object_ptr BaseMemberedType::get_member(const std::string& member_name) const
{
    Doctor::get().assert(
        contains_member(member_name),
        WaspStage::Semantics,
        "Type Member '" + member_name + "' not found!"
    );
    return member_types.at(member_name);
}

void BaseMemberedType::set_member(const std::string& member_name, Object_ptr value)
{
    member_types[member_name] = std::move(value);
}

int ModuleType::get_member_index(const std::string& member_name) const
{
    Doctor::get().assert(
        contains_member(member_name),
        WaspStage::Semantics,
        "Member '" + member_name + "' not found!"
    );

    auto it = std::find(ordered_keys.begin(), ordered_keys.end(), member_name);
    return it != ordered_keys.end() ? std::distance(ordered_keys.begin(), it) : -1;
}

Object_ptr ModuleType::get_member(int member_id) const
{
    Doctor::get().assert(
        member_id >= 0 && member_id < static_cast<int>(ordered_keys.size()),
        WaspStage::Semantics,
        "Type Member index out of bounds!"
    );
    return member_types.at(ordered_keys[member_id]);
}

void ModuleType::set_member(int member_id, Object_ptr value)
{
    Doctor::get().assert(
        member_id >= 0 && member_id < static_cast<int>(ordered_keys.size()),
        WaspStage::Semantics,
        "Type Member index out of bounds!"
    );
    member_types[ordered_keys[member_id]] = std::move(value);
}

} // namespace Wasp
