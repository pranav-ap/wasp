#include "Type.h"
#include "Doctor.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

// ============================================================================
// BagType
// ============================================================================

int BagType::get_index(const std::string& name) const
{
    auto it = std::find(ordered_keys.begin(), ordered_keys.end(), name);

    Doctor::get().assert(
        it != ordered_keys.end(),
        WaspStage::Semantics,
        "Bag does not contain member '" + name + "'."
    );

    return static_cast<int>(std::distance(ordered_keys.begin(), it));
}

Type_ptr BagType::get_type(const std::string& name) const
{
    auto it = types.find(name);

    Doctor::get().assert(
        it != types.end(),
        WaspStage::Semantics,
        "Bag does not contain member '" + name + "'."
    );

    return it->second;
}

bool BagType::contains(const std::string& name) const
{
    return types.find(name) != types.end();
}

// ============================================================================
// OverloadCoordinate
// ============================================================================

bool MethodCoordinate::operator<(const MethodCoordinate& other) const
{
    if (member_index != other.member_index)
    {
        return member_index < other.member_index;
    }

    return overload_index < other.overload_index;
}

bool MethodCoordinate::operator==(const MethodCoordinate& other) const
{
    return member_index == other.member_index &&
           overload_index == other.overload_index;
}

// ============================================================================
// OopsType
// ============================================================================

bool OopsType::contains_member(const std::string& member_name) const
{
    return fields->contains(member_name) || methods->contains(member_name);
}

StringVector OopsType::get_ordered_names() const
{
    StringVector names = fields->ordered_keys;

    names.insert(
        names.end(),
        methods->ordered_keys.begin(),
        methods->ordered_keys.end()
    );

    return names;
}

bool OopsType::is_field(const std::string& member_name) const
{
    return fields->contains(member_name);
}

bool OopsType::is_method(const std::string& member_name) const
{
    return methods->contains(member_name);
}

int OopsType::get_flat_index(const std::string& member_name) const
{
    auto field_it = fields->types.find(member_name);
    if (field_it != fields->types.end())
    {
        return fields->get_index(member_name);
    }

    int size = static_cast<int>(fields->ordered_keys.size());

    auto method_it = methods->types.find(member_name);
    if (method_it != methods->types.end())
    {
        return methods->get_index(member_name) + size;
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Type '" + name + "' does not contain member '" + member_name + "'."
    );
}

// ============================================================================
// ModuleType
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

Type_ptr ModuleType::get_member(const std::string& member_name) const
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
