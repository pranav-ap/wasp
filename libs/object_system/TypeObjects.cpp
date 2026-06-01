#include "Doctor.h"
#include "Objects.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace Wasp
{

// ============================================================================
// Overload Coordinate
// ============================================================================

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
// Template Type
// ============================================================================

int TemplateType::generics_count() const
{
    return static_cast<int>(ordered_parameter_names.size());
}

bool TemplateType::exists() const
{
    return !ordered_parameter_names.empty();
}

std::pair<std::string, Object_ptr> TemplateType::get_generic(size_t index) const
{
    Doctor::get().assert(
        index < ordered_parameter_names.size(),
        WaspStage::Semantics,
        "Template does not have generic at index " + std::to_string(index)
    );

    const std::string& name = ordered_parameter_names[index];
    Object_ptr constraint_type = template_parameters.at(name);
    return {name, constraint_type};
}

std::vector<std::pair<std::string, Object_ptr>> TemplateType::
    get_ordered_generics() const
{
    std::vector<std::pair<std::string, Object_ptr>> generics;

    for (const auto& name : ordered_parameter_names)
    {
        generics.emplace_back(name, template_parameters.at(name));
    }

    return generics;
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

int RecordType::get_index(const std::string& field_name) const
{
    auto it = std::find(ordered_keys.begin(), ordered_keys.end(), field_name);
    Doctor::get().assert(
        it != ordered_keys.end(),
        WaspStage::Semantics,
        "Record does not contain field '" + field_name + "'."
    );
    return static_cast<int>(std::distance(ordered_keys.begin(), it));
}

Object_ptr RecordType::get_type(const std::string& field_name) const
{
    auto it = types.find(field_name);
    Doctor::get().assert(
        it != types.end(),
        WaspStage::Semantics,
        "Record does not contain field '" + field_name + "'."
    );
    return it->second;
}

bool RecordType::contains(const std::string& field_name) const
{
    return types.contains(field_name);
}

// ============================================================================
// Pocket Type
// ============================================================================

Object_ptr SignaturesSet::SignaturesSet::get_signature(int overload_index) const
{
    Doctor::get().assert(
        overload_index >= 0 && overload_index < static_cast<int>(types.size()),
        WaspStage::Semantics,
        "Invalid overload index: " + std::to_string(overload_index)
    );

    return types[overload_index];
}

// ============================================================================
// Bag Type
// ============================================================================

int BagType::get_index(const std::string& function_name) const
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

Object_ptr BagType::get_signatures(const std::string& function_name) const
{
    auto it = types.find(function_name);
    Doctor::get().assert(
        it != types.end(),
        WaspStage::Semantics,
        "Bag does not contain member '" + function_name + "'."
    );
    return it->second;
}

bool BagType::contains(const std::string& field_name) const
{
    return types.contains(field_name);
}

// ============================================================================
// Oops Type
// ============================================================================

bool OopsType::contains_member(const std::string& member_name) const
{
    return record_type->types.find(member_name) != record_type->types.end() ||
           bag_type->types.find(member_name) != bag_type->types.end();
}

StringVector OopsType::get_ordered_names() const
{
    StringVector names = record_type->ordered_keys;
    names.insert(
        names.end(),
        bag_type->ordered_keys.begin(),
        bag_type->ordered_keys.end()
    );
    return names;
}

bool OopsType::is_field(const std::string& member_name) const
{
    return record_type->types.find(member_name) != record_type->types.end();
}

bool OopsType::is_method(const std::string& member_name) const
{
    return bag_type->types.find(member_name) != bag_type->types.end();
}

int OopsType::get_flat_index(const std::string& member_name) const
{
    auto field_it = record_type->types.find(member_name);
    if (field_it != record_type->types.end())
    {
        return record_type->get_index(member_name);
    }

    int size = static_cast<int>(record_type->ordered_keys.size());

    auto method_it = bag_type->types.find(member_name);
    if (method_it != bag_type->types.end())
    {
        return bag_type->get_index(member_name) + size;
    }

    Doctor::get().fatal(
        WaspStage::Semantics,
        "Type '" + name + "' does not contain member '" + member_name + "'."
    );
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
