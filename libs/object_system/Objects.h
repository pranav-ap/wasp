#pragma once

#include "CFGraph.h"

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Wasp
{

struct Object;
using Object_ptr = std::shared_ptr<Object>;
using ObjectVector = std::vector<Object_ptr>;
using ObjectStringMap = std::map<std::string, Object_ptr>;
using ObjectIntMap = std::map<int, Object_ptr>;
using OptionalObject = std::optional<Object_ptr>;

using IntVector = std::vector<int>;
using StringVector = std::vector<std::string>;

inline int get_next_type_id()
{
    static int type_id_counter = 0;
    return type_id_counter++;
}

// ============================================================================
// Other
// ============================================================================

struct NoneObject
{
};

using NoneObject_ptr = std::shared_ptr<NoneObject>;

struct IterableAbstractObject
{
    virtual ~IterableAbstractObject() = default;
    virtual Object_ptr get_iter() = 0;
};

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
    Object_ptr value;

    explicit LiteralType(Object_ptr value) : value(std::move(value))
    {
    }
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
    Object_ptr constraint_type;
};

using GenericType_ptr = std::shared_ptr<GenericType>;
using GenericTypeMap = std::map<std::string, GenericType_ptr>;

struct Template
{
    GenericTypeMap template_parameters;
    StringVector ordered_parameter_names;
};

using TemplateType_ptr = std::shared_ptr<Template>;
using OptionalTemplateType = std::optional<TemplateType_ptr>;

// ============================================================================
// Composite Types
// ============================================================================

struct ListType
{
    Object_ptr element_type;
};

struct SetType
{
    Object_ptr element_type;
};

struct TupleType
{
    ObjectVector element_types;
};

struct MapType
{
    Object_ptr key_type;
    Object_ptr value_type;
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
    ObjectVector types;
};

struct IntersectionType
{
    ObjectVector types;
};

using VariantType_ptr = std::shared_ptr<VariantType>;
using IntersectionType_ptr = std::shared_ptr<IntersectionType>;

// ============================================================================
// Signature
// ============================================================================

struct Signature
{
    ObjectVector parameter_types;
    Object_ptr return_type;
    OptionalTemplateType template_type;

    Signature(
        ObjectVector parameter_types,
        Object_ptr return_type,
        OptionalTemplateType template_type = std::nullopt
    )
        : parameter_types(std::move(parameter_types)),
          return_type(std::move(return_type)),
          template_type(std::move(template_type))
    {
    }
};

using Signature_ptr = std::shared_ptr<Signature>;

// ============================================================================
// Enum Type
// ============================================================================

struct EnumType
{
    int type_id;

    std::string name;
    StringVector members;

    EnumType(std::string name)
        : type_id(get_next_type_id()), name(std::move(name))
    {
    }

    int get_value(const StringVector& path) const;
};

using EnumType_ptr = std::shared_ptr<EnumType>;

struct EnumMemberType
{
    EnumType_ptr enum_type;
    int value;
};

using EnumMemberType_ptr = std::shared_ptr<EnumMemberType>;

// ============================================================================
// Oops Types
// ============================================================================

struct OverloadCoordinate
{
    int member_index;
    int overload_index;

    bool operator<(const OverloadCoordinate& other) const;
    bool operator==(const OverloadCoordinate& other) const;
};

// trait coordinate => my coordinate
using ITable = std::map<OverloadCoordinate, OverloadCoordinate>;
// trait type id => I Table
using ITablesMap = std::map<int, ITable>;

struct OopsType
{
    int type_id;

    std::string name;
    ObjectStringMap member_types;

    StringVector fields;
    StringVector methods;
    StringVector pures;
    StringVector statics;

    ObjectVector traits;
    ITablesMap itables;

    OptionalTemplateType template_type;

    explicit OopsType(
        std::string name,
        ObjectStringMap members = {},
        StringVector fields = {},
        StringVector methods = {},
        StringVector pures = {},
        StringVector statics = {},
        ObjectVector traits = {},
        OptionalTemplateType template_type = std::nullopt

    )
        : type_id(get_next_type_id()), name(std::move(name)),
          member_types(std::move(members)), fields(std::move(fields)),
          methods(std::move(methods)), pures(std::move(pures)),
          statics(std::move(statics)), traits(std::move(traits)),
          template_type(std::move(template_type))
    {
    }

    void add_overload(const std::string& member_name, Object_ptr overload);
    ObjectVector get_overloads(const std::string& member_name) const;

    ObjectVector get_fields() const;
    ObjectVector get_methods() const;
    ObjectVector get_pures() const;
    ObjectVector get_statics() const;
    ObjectVector get_members() const;

    ObjectVector get_traits() const;
    bool implements_trait(const std::string& trait_name) const;

    bool is_pure(const std::string& member_name) const;
    bool is_static(const std::string& member_name) const;

    int get_member_index(const std::string& member_name) const;
    Object_ptr get_member(int member_id) const;

    Object_ptr get_member(const std::string& member_name) const;
    void set_member(const std::string& member_name, Object_ptr value);

    bool contains_member(const std::string& member_name) const;
};

using OopsType_ptr = std::shared_ptr<OopsType>;

struct ClassType : public OopsType
{
    using OopsType::OopsType;
};

using ClassType_ptr = std::shared_ptr<ClassType>;

struct TraitType : public OopsType
{
    using OopsType::OopsType;
};

using TraitType_ptr = std::shared_ptr<TraitType>;

// ============================================================================
// Alias
// ============================================================================

struct TypeAlias
{
    std::string name;
    Object_ptr underlying_type;
    OptionalTemplateType template_type;

    TypeAlias(
        std::string name,
        Object_ptr underlying_type,
        OptionalTemplateType template_type = std::nullopt
    )
        : name(std::move(name)), underlying_type(std::move(underlying_type)),
          template_type(std::move(template_type))
    {
    }
};

using TypeAlias_ptr = std::shared_ptr<TypeAlias>;

// ============================================================================
// Scalar Objects
// ============================================================================

struct IntObject
{
    int value;
};

struct FloatObject
{
    double value;
};

struct BooleanObject
{
    bool value;
};

struct StringObject : public IterableAbstractObject
{
    std::string value;

    StringObject(std::string value) : value(std::move(value))
    {
    }

    Object_ptr get_iter() override;
};

using IntObject_ptr = std::shared_ptr<IntObject>;
using FloatObject_ptr = std::shared_ptr<FloatObject>;
using BooleanObject_ptr = std::shared_ptr<BooleanObject>;
using StringObject_ptr = std::shared_ptr<StringObject>;

// ============================================================================
// Composite Objects
// ============================================================================

struct IteratorObject
{
    ObjectVector vec;
    size_t index = 0;

    IteratorObject(ObjectVector v) : vec(std::move(v))
    {
    }

    bool has_next() const;
    OptionalObject get_next();
    void reset_iter();
};

using IteratorObject_ptr = std::shared_ptr<IteratorObject>;

struct BaseArrayObject
{
    ObjectVector values;

    BaseArrayObject(ObjectVector values = {}) : values(std::move(values))
    {
    }

    virtual ~BaseArrayObject() = default;

    int get_length() const
    {
        return static_cast<int>(values.size());
    }
};

struct ListObject : public BaseArrayObject, public IterableAbstractObject
{
    using BaseArrayObject::BaseArrayObject;

    void append(Object_ptr value);
    void prepend(Object_ptr value);
    Object_ptr pop_back();
    Object_ptr pop_front();
    Object_ptr get(Object_ptr index);
    void set(Object_ptr index, Object_ptr value);
    void clear();
    bool is_empty();
    Object_ptr get_iter() override;
};

struct TupleObject : public BaseArrayObject
{
    using BaseArrayObject::BaseArrayObject;

    Object_ptr get(Object_ptr index);
    void set(Object_ptr index, Object_ptr value);
    void set(ObjectVector new_values);
};

struct SetObject : public BaseArrayObject, public IterableAbstractObject
{
    using BaseArrayObject::BaseArrayObject;

    ObjectVector get();
    void set(ObjectVector new_values);
    Object_ptr get_iter() override;
};

struct MapObject : public IterableAbstractObject
{
    std::map<Object_ptr, Object_ptr> pairs;

    MapObject(std::map<Object_ptr, Object_ptr> pairs = {})
        : pairs(std::move(pairs))
    {
    }

    void insert(Object_ptr key, Object_ptr value);
    Object_ptr get(Object_ptr key);
    void set(Object_ptr key, Object_ptr value);
    int get_size();
    Object_ptr get_iter() override;
};

struct VariantObject
{
    Object_ptr value;
    VariantObject(Object_ptr value) : value(std::move(value)) {};
    bool has_value();
};

using ListObject_ptr = std::shared_ptr<ListObject>;
using TupleObject_ptr = std::shared_ptr<TupleObject>;
using SetObject_ptr = std::shared_ptr<SetObject>;
using MapObject_ptr = std::shared_ptr<MapObject>;
using VariantObject_ptr = std::shared_ptr<VariantObject>;

// ============================================================================
// Function Objects
// ============================================================================

struct FunctionBlueprintObject
{
    CodeObject code;
    std::string name;

    FunctionBlueprintObject(CodeObject code, std::string name = "")
        : code(std::move(code)), name(std::move(name))
    {
    }
};

using FunctionBlueprintObject_ptr = std::shared_ptr<FunctionBlueprintObject>;

struct FunctionRuntimeObject
{
    FunctionBlueprintObject_ptr blueprint;
    ObjectVector upvalues;

    FunctionRuntimeObject(
        FunctionBlueprintObject_ptr blueprint,
        ObjectVector upvalues = {}
    )
        : blueprint(std::move(blueprint)), upvalues(std::move(upvalues))
    {
    }
};

using FunctionRuntimeObject_ptr = std::shared_ptr<FunctionRuntimeObject>;

using NativeFnType = std::function<Object_ptr(const ObjectVector&)>;

struct NativeFunctionObject
{
    NativeFnType function;
    std::string name;

    NativeFunctionObject(NativeFnType function, std::string name)
        : function(std::move(function)), name(std::move(name))
    {
    }
};

using NativeFunctionObject_ptr = std::shared_ptr<NativeFunctionObject>;

struct FunctionOverloadsObject
{
    ObjectVector overloads;

    void add_overload(Object_ptr overload);
    Object_ptr get_overload(int overload_id) const;
};

using FunctionOverloadsObject_ptr = std::shared_ptr<FunctionOverloadsObject>;

// ============================================================================
// Oops
// ============================================================================

struct RecordObject
{
    ObjectVector fields;

    RecordObject(ObjectVector fields) : fields(std::move(fields))
    {
    }

    Object_ptr get_field(int member_id) const;
    void set_field(int member_id, Object_ptr value);
};

using RecordObject_ptr = std::shared_ptr<RecordObject>;

struct ImplObject
{
    ObjectVector overloads;
    ITablesMap itables;

    ImplObject(ObjectVector overloads, ITablesMap itables)
        : overloads(std::move(overloads)), itables(std::move(itables))
    {
    }

    Object_ptr get_overload(int member_id) const;
};

using ImplObject_ptr = std::shared_ptr<ImplObject>;

struct BoxObject
{
    RecordObject_ptr record;
    ImplObject_ptr impl;

    ITable itable;

    BoxObject(RecordObject_ptr record, ImplObject_ptr impl, ITable itable)
        : record(std::move(record)), impl(std::move(impl)),
          itable(std::move(itable))
    {
    }
};

using BoxObject_ptr = std::shared_ptr<BoxObject>;

// ============================================================================
// Module
// ============================================================================

struct ModuleObject
{
    std::string name;
    ObjectVector members;

    ModuleObject(std::string name, ObjectVector members)
        : name(std::move(name)), members(std::move(members))
    {
    }

    Object_ptr get_member(int member_id) const;
    void set_member(int member_id, Object_ptr value);
    int get_member_count() const;
};

using ModuleObject_ptr = std::shared_ptr<ModuleObject>;

// ============================================================================
// The Core Object Variant
// ============================================================================

struct Object : public std::enable_shared_from_this<Object>
{
    using UnderlyingVariant = std::variant<
        std::monostate,

        // Primitives
        NoneObject_ptr,
        IntObject_ptr,
        FloatObject_ptr,
        StringObject_ptr,
        BooleanObject_ptr,

        // Composites
        IteratorObject_ptr,
        ListObject_ptr,
        TupleObject_ptr,
        SetObject_ptr,
        MapObject_ptr,
        VariantObject_ptr,

        // Functions
        FunctionBlueprintObject_ptr,
        FunctionRuntimeObject_ptr,
        NativeFunctionObject_ptr,
        FunctionOverloadsObject_ptr,

        ModuleObject_ptr,

        RecordObject_ptr,
        ImplObject_ptr,
        BoxObject_ptr,

        // Types

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

        GenericType_ptr,
        EnumMemberType_ptr,

        Signature_ptr,
        ClassType_ptr,
        TraitType_ptr,
        EnumType_ptr,
        TypeAlias_ptr>;

    UnderlyingVariant value;

    Object() = default;

    std::string to_string() const;

    bool is_type_object() const;
    bool is_runtime_object() const;

    Object_ptr unwrap_type_alias();
    Object_ptr unwrap_completely();

    template <typename T> Object(T&& val) : value(std::forward<T>(val))
    {
    }

    template <typename T> bool is() const
    {
        return std::holds_alternative<T>(value);
    }

    template <typename T> const T& as() const
    {
        return std::get<T>(value);
    }

    template <typename T> T& as()
    {
        return std::get<T>(value);
    }

    template <typename... Ts> bool is_any_of() const
    {
        return (is<Ts>() || ...);
    }
};

template <typename T> inline Object_ptr make_object(T&& val)
{
    return std::make_shared<Object>(std::forward<T>(val));
}

// ============================================================================
// Utils
// ============================================================================

std::string mangle_object(Object_ptr value);
std::string mangle_object(const ObjectVector& values);

bool are_equal_types(Object_ptr left, Object_ptr right);
bool are_equal_types(ObjectVector left_vector, ObjectVector right_vector);
bool are_equal_types_unordered(
    ObjectVector left_vector,
    ObjectVector right_vector
);

std::string get_canonical_trait_name(const ObjectVector& traits);

} // namespace Wasp
