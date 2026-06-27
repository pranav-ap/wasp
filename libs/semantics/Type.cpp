#include "Type.h"
#include "Doctor.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <sstream>
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

// ============================================================================
// FieldMap
// ============================================================================

int FieldMap::get_index(const std::string& name) const
{
    auto it = std::find(ordered_keys.begin(), ordered_keys.end(), name);

    Doctor::semantics().check(
        it != ordered_keys.end(),
        "Bag does not contain member '" + name + "'."
    );

    return static_cast<int>(std::distance(ordered_keys.begin(), it));
}

Type_ptr FieldMap::get_type(const std::string& name) const
{
    auto it = types.find(name);

    Doctor::semantics().check(
        it != types.end(),
        "Bag does not contain member '" + name + "'."
    );

    return it->second;
}

Type_ptr FieldMap::get_type(int index) const
{
    Doctor::semantics().check(
        index >= 0 && index < static_cast<int>(ordered_keys.size()),
        "Invalid field index: " + std::to_string(index)
    );

    const auto& name = ordered_keys[index];
    return types.at(name);
}

bool FieldMap::contains(const std::string& name) const
{
    return types.find(name) != types.end();
}

TypeVector FieldMap::get_ordered_types() const
{
    TypeVector ordered_types;

    for (const auto& name : ordered_keys)
    {
        ordered_types.push_back(types.at(name));
    }

    return ordered_types;
}

// ============================================================================
// MethodMap
// ============================================================================

int MethodMap::get_index(const std::string& name) const
{
    auto it = std::find(ordered_keys.begin(), ordered_keys.end(), name);

    Doctor::semantics().check(
        it != ordered_keys.end(),
        "Bag does not contain member '" + name + "'."
    );

    return static_cast<int>(std::distance(ordered_keys.begin(), it));
}

MethodTypeVector MethodMap::get_type(const std::string& name) const
{
    auto it = method_overload_types.find(name);

    Doctor::semantics().check(
        it != method_overload_types.end(),
        "Method Map does not contain member '" + name + "'."
    );

    return it->second;
}

bool MethodMap::contains(const std::string& name) const
{
    return method_overload_types.find(name) != method_overload_types.end();
}

MethodTypeVector MethodMap::add(const std::string& function_name)
{
    Doctor::captain().check(
        !contains(function_name),
        "Method Map already contains member '" + function_name + "'."
    );

    ordered_keys.push_back(function_name);
    method_overload_types[function_name] = {};

    return method_overload_types[function_name];
}

// ============================================================================
// Overload Coordinate
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

// ============================================================================
// ModuleType
// ============================================================================

int ModuleType::get_member_index(const std::string& member_name) const
{
    auto it = std::find(ordered_keys.begin(), ordered_keys.end(), member_name);

    Doctor::semantics().check(
        it != ordered_keys.end(),
        "Module '" + name + "' does not contain member '" + member_name + "'."
    );

    return static_cast<int>(std::distance(ordered_keys.begin(), it));
}

Type_ptr ModuleType::get_member(const std::string& member_name) const
{
    auto it = member_types.find(member_name);

    Doctor::semantics().check(
        it != member_types.end(),
        "Module '" + name + "' does not contain member '" + member_name + "'."
    );

    return it->second;
}

// ============================================================================
// Template Type
// ============================================================================

bool TemplateType::empty() const
{
    return template_parameters.empty();
}

std::vector<std::pair<std::string, Type_ptr>> TemplateType::
    get_ordered_generics() const
{
    std::vector<std::pair<std::string, Type_ptr>> generics;

    for (const auto& name : ordered_parameter_names)
    {
        generics.emplace_back(name, template_parameters.at(name));
    }

    return generics;
}

Type_ptr TemplateType::get_generic_type(int index) const
{
    Doctor::semantics().check(
        index >= 0 && index < static_cast<int>(ordered_parameter_names.size()),
        "Invalid generic index: " + std::to_string(index)
    );

    const auto& name = ordered_parameter_names[index];
    return template_parameters.at(name);
}

Type_ptr TemplateType::get_generic_type(const std::string& name) const
{
    auto it = template_parameters.find(name);

    Doctor::semantics().check(
        it != template_parameters.end(),
        "Template does not contain generic '" + name + "'."
    );

    return it->second;
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

    Doctor::semantics().fatal(
        "Enum '" + name + "' does not contain '" + search_path
    );
}

// ============================================================================
// Utils
// ============================================================================

std::string Type::to_string() const
{
    Doctor::semantics().fatal_if_nullptr(
        this,
        "Attempted to stringify a null object pointer"
    );

    return std::visit(
        overloaded{
            [](std::monostate) -> std::string
            {
                return "uninitialized";
            },

            [](AnyType_ptr) -> std::string
            {
                return "any type";
            },
            [](NoneType_ptr) -> std::string
            {
                return "none type";
            },

            [](IntType_ptr) -> std::string
            {
                return "int";
            },
            [](FloatType_ptr) -> std::string
            {
                return "float";
            },
            [](StringType_ptr) -> std::string
            {
                return "str";
            },
            [](BooleanType_ptr) -> std::string
            {
                return "bool";
            },

            [](ListType_ptr) -> std::string
            {
                return "list type";
            },
            [](SetType_ptr) -> std::string
            {
                return "set type";
            },
            [](TupleType_ptr) -> std::string
            {
                return "tuple type";
            },
            [](MapType_ptr) -> std::string
            {
                return "map type";
            },

            [](VariantType_ptr) -> std::string
            {
                return "variant type";
            },
            [](IntersectionType_ptr) -> std::string
            {
                return "intersection type";
            },

            [](EnumType_ptr enum_type) -> std::string
            {
                return "enum type: " + enum_type->name;
            },
            [](EnumMemberType_ptr) -> std::string
            {
                return "enum member";
            },

            [](GenericType_ptr gen) -> std::string
            {
                return "generic type: " + gen->name;
            },

            [](ModuleType_ptr module) -> std::string
            {
                return "module type: " + module->name;
            },

            [](FunctionType_ptr) -> std::string
            {
                return "function type";
            },

            [](MethodType_ptr) -> std::string
            {
                return "method type";
            },

            [](ClassType_ptr cls) -> std::string
            {
                return "class type: " + cls->name;
            },
            [](TraitType_ptr trt) -> std::string
            {
                return "trait type: " + trt->name;
            },
            [](PrimitiveType_ptr prim) -> std::string
            {
                return "primitive type: " + prim->name;
            },

            [](TypeAlias_ptr alias) -> std::string
            {
                return "type alias: " + alias->name;
            },

            [](const auto&) -> std::string
            {
                return "<Unknown Object>";
            }
        },
        this->data
    );
}

Type_ptr Type::unwrap_alias()
{
    if (is<TypeAlias_ptr>())
    {
        auto alias = as<TypeAlias_ptr>();

        if (alias->underlying_type)
        {
            return alias->underlying_type->unwrap_alias();
        }
    }

    return shared_from_this();
}

} // namespace Wasp
