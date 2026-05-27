#include "Doctor.h"
#include "Objects.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace Wasp
{

bool OverloadCoordinate::operator<(const OverloadCoordinate& other) const
{
    if (member_index != other.member_index)
    {
        return member_index < other.member_index;
    }
    return overload_index < other.overload_index;
}

bool OverloadCoordinate::operator==(const OverloadCoordinate& other) const
{
    return member_index == other.member_index &&
           overload_index == other.overload_index;
}

// ============================================================================
// Enum Type
// ============================================================================

int EnumType::get_value(const std::vector<std::string>& path) const
{
    std::stringstream ss;
    for (size_t i = 0; i < path.size(); ++i)
    {
        ss << path[i] << (i == path.size() - 1 ? "" : ".");
    }

    std::string search_path = ss.str();
    auto it = std::find(members.begin(), members.end(), search_path);

    if (it != members.end())
    {
        return static_cast<int>(std::distance(members.begin(), it));
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Enum '" + name + "' does not contain member '" + search_path + "'."
    );
}

// ============================================================================
// Record Type
// ============================================================================

int RecordType::get_field_index(const std::string& field_name) const
{
    auto it = std::find(ordered_keys.begin(), ordered_keys.end(), field_name);
    Doctor::get().assert(
        it != ordered_keys.end(),
        WaspStage::Semantics,
        "Record does not contain field '" + field_name + "'."
    );
    return static_cast<int>(std::distance(ordered_keys.begin(), it));
}

Object_ptr RecordType::get_field(const std::string& field_name) const
{
    auto it = field_types.find(field_name);
    Doctor::get().assert(
        it != field_types.end(),
        WaspStage::Semantics,
        "Record does not contain field '" + field_name + "'."
    );
    return it->second;
}

bool RecordType::contains_field(const std::string& field_name) const
{
    return field_types.contains(field_name);
}

// ============================================================================
// Bag Type
// ============================================================================

int BagType::get_overloads_index(const std::string& function_name) const
{
    auto it = std::find(
        ordered_keys.begin(),
        ordered_keys.end(),
        function_name
    );
    Doctor::get().assert(
        it != ordered_keys.end(),
        WaspStage::Semantics,
        "Bag does not contain member '" + function_name + "'."
    );
    return static_cast<int>(std::distance(ordered_keys.begin(), it));
}

Object_ptr BagType::get_overloads(const std::string& function_name) const
{
    auto it = overload_types.find(function_name);
    Doctor::get().assert(
        it != overload_types.end(),
        WaspStage::Semantics,
        "Bag does not contain member '" + function_name + "'."
    );
    return it->second;
}

// ============================================================================
// Oops Type
// ============================================================================

bool OopsType::contains_member(const std::string& member_name) const
{
    return record_type.field_types.find(member_name) !=
               record_type.field_types.end() ||
           bag_type.overload_types.find(member_name) !=
               bag_type.overload_types.end();
}

// ============================================================================
// Module Type
// ============================================================================

int ModuleType::get_member_index(const std::string& member_name) const
{
    auto it = std::find(ordered_keys.begin(), ordered_keys.end(), member_name);
    Doctor::get().assert(
        it != ordered_keys.end(),
        WaspStage::Semantics,
        "Module '" + name + "' does not contain member '" + member_name + "'."
    );
    return static_cast<int>(std::distance(ordered_keys.begin(), it));
}

Object_ptr ModuleType::get_member(const std::string& member_name) const
{
    auto it = member_types.find(member_name);
    Doctor::get().assert(
        it != member_types.end(),
        WaspStage::Semantics,
        "Module '" + name + "' does not contain member '" + member_name + "'."
    );
    return it->second;
}

} // namespace Wasp
