#include "Type.h"
#include "Doctor.h"

#include <algorithm>
#include <iterator>
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

// ============================================================================
// FieldMap
// ============================================================================

int FieldMap::get_index(const std::string& name) const
{
    auto it = std::find(ordered_keys.begin(), ordered_keys.end(), name);

    Doctor::get().assert(
        it != ordered_keys.end(),
        WaspStage::Semantics,
        "Bag does not contain member '" + name + "'."
    );

    return static_cast<int>(std::distance(ordered_keys.begin(), it));
}

Type_ptr FieldMap::get(const std::string& name) const
{
    auto it = types.find(name);

    Doctor::get().assert(
        it != types.end(),
        WaspStage::Semantics,
        "Bag does not contain member '" + name + "'."
    );

    return it->second;
}

bool FieldMap::contains(const std::string& name) const
{
    return types.find(name) != types.end();
}

// ============================================================================
// MethodMap
// ============================================================================

int MethodMap::get_index(const std::string& name) const
{
    auto it = std::find(ordered_keys.begin(), ordered_keys.end(), name);

    Doctor::get().assert(
        it != ordered_keys.end(),
        WaspStage::Semantics,
        "Bag does not contain member '" + name + "'."
    );

    return static_cast<int>(std::distance(ordered_keys.begin(), it));
}

SignatureSet_ptr MethodMap::get(const std::string& name) const
{
    auto it = signatures.find(name);

    Doctor::get().assert(
        it != signatures.end(),
        WaspStage::Semantics,
        "Bag does not contain member '" + name + "'."
    );

    return it->second;
}

bool MethodMap::contains(const std::string& name) const
{
    return signatures.find(name) != signatures.end();
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
    Doctor::get().assert(
        index >= 0 &&
            index < static_cast<int>(ordered_parameter_names.size()),
        WaspStage::Semantics,
        "Invalid generic index: " + std::to_string(index)
    );

    const auto& name = ordered_parameter_names[index];
    return template_parameters.at(name);
}

Type_ptr TemplateType::get_generic_type(const std::string& name) const
{
    auto it = template_parameters.find(name);

    Doctor::get().assert(
        it != template_parameters.end(),
        WaspStage::Semantics,
        "Template does not contain generic '" + name + "'."
    );

    return it->second;
}

// ============================================================================
// Signature Set
// ============================================================================

void SignatureSet::add(Signature_ptr signature)
{
    signatures.push_back(signature);
}

Signature_ptr SignatureSet::get(int index) const
{
    Doctor::get().assert(
        index >= 0 && index < static_cast<int>(signatures.size()),
        WaspStage::Semantics,
        "Invalid signature index: " + std::to_string(index)
    );

    return signatures[index];
}

// ============================================================================
// Utils
// ============================================================================

std::string Type::to_string() const
{
    Doctor::get().fatal_if_nullptr(
        this,
        WaspStage::VM,
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

            [](Signature_ptr) -> std::string
            {
                return "signature type";
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
