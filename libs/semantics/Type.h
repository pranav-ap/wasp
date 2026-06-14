#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Wasp
{

struct Type;
using Type_ptr = std::shared_ptr<Type>;
using TypeVector = std::vector<Type_ptr>;
using TypeStringMap = std::map<std::string, Type_ptr>;
using TypeIntMap = std::map<int, Type_ptr>;
using OptionalType = std::optional<Type_ptr>;

using IntVector = std::vector<int>;
using StringVector = std::vector<std::string>;

// ============================================================================
// Utils
// ============================================================================

inline int get_next_type_id()
{
    static int type_id_counter = 0;
    return type_id_counter++;
}

// ============================================================================
// Types
// ============================================================================

struct AnyType
{
};

struct NoneType
{
};

using AnyType_ptr = std::shared_ptr<AnyType>;
using NoneType_ptr = std::shared_ptr<NoneType>;

// ============================================================================
// Scalar Types
// ============================================================================

struct IntType
{
};

struct FloatType
{
};

struct StringType
{
};

struct BooleanType
{
};

struct LiteralType
{
    Type_ptr value;
};

using IntType_ptr = std::shared_ptr<IntType>;
using FloatType_ptr = std::shared_ptr<FloatType>;
using StringType_ptr = std::shared_ptr<StringType>;
using BooleanType_ptr = std::shared_ptr<BooleanType>;
using LiteralType_ptr = std::shared_ptr<LiteralType>;

// ============================================================================
// Template
// ============================================================================

struct GenericType
{
    std::string name;
    Type_ptr constraint_type;
    bool is_variadic;
};

using GenericType_ptr = std::shared_ptr<GenericType>;

struct TemplateType
{
    TypeStringMap template_parameters;
    StringVector ordered_parameter_names;
    std::optional<std::string> variadic_name;
};

using TemplateType_ptr = std::shared_ptr<TemplateType>;

// ============================================================================
// Composite Types
// ============================================================================

struct ListType
{
    Type_ptr element_type;
};

struct SetType
{
    Type_ptr element_type;
};

struct TupleType
{
    TypeVector element_types;
};

struct MapType
{
    Type_ptr key_type;
    Type_ptr value_type;
};

using ListType_ptr = std::shared_ptr<ListType>;
using SetType_ptr = std::shared_ptr<SetType>;
using TupleType_ptr = std::shared_ptr<TupleType>;
using MapType_ptr = std::shared_ptr<MapType>;

// ============================================================================
// Algebraic Types
// ===========================================================================

struct VariantType
{
    TypeVector types;
};

struct IntersectionType
{
    TypeVector types;
};

using VariantType_ptr = std::shared_ptr<VariantType>;
using IntersectionType_ptr = std::shared_ptr<IntersectionType>;

// ============================================================================
// Enum Type
// ============================================================================

struct EnumType
{
    int type_id;

    std::string name;
    StringVector members;
};

using EnumType_ptr = std::shared_ptr<EnumType>;

struct EnumMemberType
{
    EnumType_ptr enum_type;
    int value;
};

using EnumMemberType_ptr = std::shared_ptr<EnumMemberType>;

// ============================================================================
// Signature
// ============================================================================

struct Signature
{
    TypeVector parameter_types;
    Type_ptr return_type;
    TemplateType_ptr template_type;

    bool is_static_method;
};

using Signature_ptr = std::shared_ptr<Signature>;

// ============================================================================
// Oops Types
// ============================================================================

struct MethodCoordinate
{
    int member_index;
    int overload_index;

    bool operator<(const MethodCoordinate& other) const;
    bool operator==(const MethodCoordinate& other) const;
};

// trait coordinate => my coordinate
using ITable = std::map<MethodCoordinate, MethodCoordinate>;
// trait type id => I Table
using ITablesMap = std::map<int, ITable>;

struct BagType
{
    TypeStringMap types;
    StringVector ordered_keys;

    int get_index(const std::string& function_name) const;
    Type_ptr get_type(const std::string& function_name) const;
    bool contains(const std::string& field_name) const;
};

using BagType_ptr = std::shared_ptr<BagType>;

struct OopsType
{
    int type_id;
    std::string name;

    BagType_ptr fields;
    BagType_ptr methods;
    ITablesMap itables;

    TypeVector traits;

    TemplateType_ptr template_type;

    virtual ~OopsType() = default;

    bool contains_member(const std::string& member_name) const;
    StringVector get_ordered_names() const;

    bool is_field(const std::string& member_name) const;
    bool is_method(const std::string& member_name) const;
    int get_flat_index(const std::string& member_name) const;
};

struct ClassType : public OopsType
{
    using OopsType::OopsType;
};

struct TraitType : public OopsType
{
    using OopsType::OopsType;
};

struct PrimitiveType : public OopsType
{
    using OopsType::OopsType;
};

using OopsType_ptr = std::shared_ptr<OopsType>;
using ClassType_ptr = std::shared_ptr<ClassType>;
using TraitType_ptr = std::shared_ptr<TraitType>;
using PrimitiveType_ptr = std::shared_ptr<PrimitiveType>;

// ============================================================================
// Alias
// ============================================================================

struct TypeAlias
{
    std::string name;
    Type_ptr underlying_type;
    TemplateType_ptr template_type;
};

using TypeAlias_ptr = std::shared_ptr<TypeAlias>;

// ============================================================================
// Module Type
// ============================================================================

struct ModuleType
{
    int type_id;
    std::string name;

    TypeStringMap member_types;
    StringVector ordered_keys;

    int get_member_index(const std::string& member_name) const;
    Type_ptr get_member(const std::string& member_name) const;
};

using ModuleType_ptr = std::shared_ptr<ModuleType>;

// ============================================================================
// The Core Type Variant
// ============================================================================

using TypeVariant = std::variant<
    std::monostate,

    AnyType_ptr,
    NoneType_ptr,

    IntType_ptr,
    FloatType_ptr,
    StringType_ptr,
    BooleanType_ptr,
    LiteralType_ptr,

    ListType_ptr,
    SetType_ptr,
    TupleType_ptr,
    MapType_ptr,

    VariantType_ptr,
    IntersectionType_ptr,

    EnumType_ptr,
    EnumMemberType_ptr,

    GenericType_ptr,

    ModuleType_ptr,

    Signature_ptr,

    ClassType_ptr,
    TraitType_ptr,
    PrimitiveType_ptr,

    TypeAlias_ptr>;

struct Type : public std::enable_shared_from_this<Type>
{
    TypeVariant data;

    Type() = default;

    template <typename T> Type(T&& val) : data(std::forward<T>(val))
    {
    }

    virtual ~Type() = default;

    template <typename T> bool is() const
    {
        return std::holds_alternative<T>(data);
    }

    template <typename T> const T& as() const
    {
        return std::get<T>(data);
    }

    template <typename T> T& as()
    {
        return std::get<T>(data);
    }

    template <typename... Ts> bool is_any_of() const
    {
        return (is<Ts>() || ...);
    }

    std::string to_string() const;
    Type_ptr unwrap_alias();
};

template <typename T> inline Type_ptr make_type(T&& val)
{
    return std::make_shared<Type>(std::forward<T>(val));
}

template <typename T, typename... Args>
Type_ptr make_shared_type(Args&&... args)
{
    return make_type(std::make_shared<T>(std::forward<Args>(args)...));
}

} // namespace Wasp
