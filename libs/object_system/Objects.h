#pragma once

#include "CFGraph.h"

#include <cstddef>
#include <filesystem>
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

using IntVector = std::vector<int>;
using StringVector = std::vector<std::string>;

static int type_id_counter = 0;

// ============================================================================
// Other
// ============================================================================

struct NoneObject
{
};

struct IterableAbstractObject
{
    virtual ~IterableAbstractObject() = default;
    virtual Object_ptr get_iter() = 0;
};

// ============================================================================
// Types
// ============================================================================

struct TypeType
{
};

struct AnyType
{
};

struct NoneType
{
};

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

// ============================================================================
// Template
// ============================================================================

struct TemplateParameterType
{
    std::string name;
    Object_ptr constraint_type;
};

using TemplateParameterType_ptr = std::shared_ptr<TemplateParameterType>;

struct TemplatableType
{
    ObjectStringMap template_parameter_types;
    StringVector ordered_template_parameter_names;
};

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

struct VariantType
{
    ObjectVector types;
};

struct IntersectionType
{
    ObjectVector types;
};

struct MapType
{
    Object_ptr key_type;
    Object_ptr value_type;
};

// ============================================================================
// Signature
// ============================================================================

struct Signature : public TemplatableType
{
    ObjectVector parameter_types;
    Object_ptr return_type;

    Signature(
        ObjectVector parameter_types,
        Object_ptr return_type,
        ObjectStringMap template_parameter_types,
        StringVector ordered_template_parameter_names
    )
        : TemplatableType(
              std::move(template_parameter_types),
              std::move(ordered_template_parameter_names)
          ),
          parameter_types(std::move(parameter_types)),
          return_type(std::move(return_type))
    {
    }
};

using Signature_ptr = std::shared_ptr<Signature>;

// ============================================================================
// Enum
// ============================================================================

struct EnumMemberType
{
    int enum_type_id;
    int value;
};

struct EnumType
{
    int type_id;

    std::string name;
    StringVector members;

    EnumType(std::string name) : type_id(type_id_counter++), name(std::move(name))
    {
    }

    int get_value(const StringVector& path) const;
};

using EnumType_ptr = std::shared_ptr<EnumType>;

// ============================================================================
// Membered Types
// ============================================================================

struct BaseMemberedType
{
    int type_id;

    std::string name;
    ObjectStringMap member_types;

    explicit BaseMemberedType(std::string name, ObjectStringMap member_types = {})
        : type_id(type_id_counter++), name(std::move(name)), member_types(std::move(member_types))
    {
    }

    virtual ~BaseMemberedType() = default;

    virtual bool contains_member(const std::string& member_name) const;
    virtual Object_ptr get_member(const std::string& member_name) const;
    virtual void set_member(const std::string& member_name, Object_ptr value);
};

struct ModuleType : public BaseMemberedType
{
    std::filesystem::path absolute_filepath;
    StringVector ordered_keys;

    explicit ModuleType(
        std::string name,
        std::filesystem::path absolute_filepath,
        StringVector ordered_keys,
        ObjectStringMap members = {}
    )
        : BaseMemberedType(std::move(name), std::move(members)),
          absolute_filepath(std::move(absolute_filepath)),
          ordered_keys(std::move(ordered_keys))
    {
    }

    using BaseMemberedType::get_member;
    Object_ptr get_member(int member_id) const;
    void set_member(int member_id, Object_ptr value);
    int get_member_index(const std::string& member_name) const;
};

using ModuleType_ptr = std::shared_ptr<ModuleType>;

struct OverloadCoordinate
{
    int member_index;
    int overload_index;

    bool operator<(const OverloadCoordinate& other) const
    {
        if (member_index != other.member_index)
        {
            return member_index < other.member_index;
        }

        return overload_index < other.overload_index;
    }

    bool operator==(const OverloadCoordinate& other) const
    {
        return member_index == other.member_index && overload_index == other.overload_index;
    }
};

// trait coordinate => my coordinate
using ITable = std::map<OverloadCoordinate, OverloadCoordinate>;
// trait type id => I Table
using ITablesMap = std::map<int, ITable>;

struct OopsType : public BaseMemberedType, public TemplatableType
{
    StringVector fields;
    StringVector methods;
    StringVector pures;
    StringVector statics;

    ObjectVector traits;

    ITablesMap itables;

    explicit OopsType(
        std::string name,
        ObjectStringMap members = {},
        StringVector fields = {},
        StringVector methods = {},
        StringVector pures = {},
        StringVector statics = {},
        ObjectStringMap template_parameter_types = {},
        StringVector ordered_template_parameter_names = {},
        ObjectVector traits = {}
    )
        : BaseMemberedType(std::move(name), std::move(members)),
          TemplatableType(
              std::move(template_parameter_types),
              std::move(ordered_template_parameter_names)
          ),
          fields(std::move(fields)), methods(std::move(methods)),
          pures(std::move(pures)), statics(std::move(statics)),
          traits(std::move(traits))
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

    Object_ptr get_member(const std::string& member_name) const override;
    void set_member(const std::string& member_name, Object_ptr value) override;

    bool contains_member(const std::string& member_name) const override;
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

struct TypeAlias : public TemplatableType
{
    std::string name;
    Object_ptr underlying_type;

    TypeAlias(
        std::string name,
        Object_ptr underlying_type = nullptr,
        ObjectStringMap template_parameters = {},
        StringVector ordered_template_parameter_names = {}
    )
        : TemplatableType(
              std::move(template_parameters),
              std::move(ordered_template_parameter_names)
          ),
          name(std::move(name)), underlying_type(std::move(underlying_type))
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
    std::optional<Object_ptr> get_next();
    void reset_iter();
};

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
    Object_ptr append(Object_ptr value);
    Object_ptr prepend(Object_ptr value);
    Object_ptr pop_back();
    Object_ptr pop_front();
    Object_ptr get(Object_ptr index);
    Object_ptr set(Object_ptr index, Object_ptr value);
    void clear();
    bool is_empty();
    Object_ptr get_iter() override;
};

struct TupleObject : public BaseArrayObject
{
    using BaseArrayObject::BaseArrayObject;
    Object_ptr get(Object_ptr index);
    Object_ptr set(Object_ptr index, Object_ptr value);
    Object_ptr set(ObjectVector new_values);
};

struct SetObject : public BaseArrayObject, public IterableAbstractObject
{
    using BaseArrayObject::BaseArrayObject;
    ObjectVector get();
    Object_ptr set(ObjectVector new_values);
    Object_ptr get_iter() override;
};

struct MapObject : public IterableAbstractObject
{
    std::map<Object_ptr, Object_ptr> pairs;

    MapObject(std::map<Object_ptr, Object_ptr> pairs = {})
        : pairs(std::move(pairs))
    {
    }

    Object_ptr insert(Object_ptr key, Object_ptr value);
    Object_ptr get_pair(Object_ptr key);
    Object_ptr get(Object_ptr key);
    Object_ptr set(Object_ptr key, Object_ptr value);
    int get_size();
    Object_ptr get_iter() override;
};

struct VariantObject
{
    Object_ptr value;
    VariantObject(Object_ptr value) : value(std::move(value)) {};
    bool has_value();
};

// ============================================================================
// Action Objects
// ============================================================================

struct ReturnObject
{
    std::optional<Object_ptr> value;

    ReturnObject(std::optional<Object_ptr> value = std::nullopt)
        : value(std::move(value))
    {
    }
};

struct ErrorObject
{
    std::string message;

    ErrorObject(std::string message = "") : message(std::move(message))
    {
    }
};

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

// ============================================================================
// Overloads
// ============================================================================

struct ObjectOverloadList
{
    ObjectVector overloads;

    void add_overload(Object_ptr object)
    {
        overloads.push_back(std::move(object));
    }
};

using ObjectOverloadList_ptr = std::shared_ptr<ObjectOverloadList>;

struct ClassBlueprintObject
{
    ObjectVector methods;
    ITablesMap itables;

    ClassBlueprintObject() = default;

    ClassBlueprintObject(ObjectVector methods, ITablesMap itables)
        : methods(std::move(methods)), itables(std::move(itables))
    {
    }

    Object_ptr get_method(int member_id) const;
};

using ClassBlueprintObject_ptr = std::shared_ptr<ClassBlueprintObject>;

struct ClassInstanceObject
{
    ClassBlueprintObject_ptr blueprint;
    ObjectVector fields;

    ClassInstanceObject(ClassBlueprintObject_ptr blueprint, ObjectVector fields)
        : blueprint(std::move(blueprint)), fields(std::move(fields))
    {
    }

    Object_ptr get_field(int member_id) const;
    void set_field(int member_id, Object_ptr value);
};

using ClassInstanceObject_ptr = std::shared_ptr<ClassInstanceObject>;

struct TraitInstanceObject
{
    Object_ptr class_instance;
    ITable itable;

    TraitInstanceObject(Object_ptr class_instance, ITable itable)
        : class_instance(std::move(class_instance)), itable(std::move(itable))
    {
    }
};

using TraitInstanceObject_ptr = std::shared_ptr<TraitInstanceObject>;

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

// ============================================================================
// The Core Object Variant
// ============================================================================

struct Object
{
    using UnderlyingVariant = std::variant<
        std::monostate,

        NoneObject,
        IntObject,
        FloatObject,
        StringObject,
        BooleanObject,

        std::shared_ptr<IteratorObject>,

        std::shared_ptr<ListObject>,
        std::shared_ptr<TupleObject>,
        std::shared_ptr<SetObject>,
        std::shared_ptr<MapObject>,
        std::shared_ptr<VariantObject>,

        std::shared_ptr<FunctionBlueprintObject>,
        std::shared_ptr<FunctionRuntimeObject>,
        std::shared_ptr<NativeFunctionObject>,

        std::shared_ptr<ModuleObject>,
        std::shared_ptr<ClassBlueprintObject>,
        std::shared_ptr<ClassInstanceObject>,
        std::shared_ptr<TraitInstanceObject>,

        std::shared_ptr<ObjectOverloadList>,

        std::shared_ptr<ReturnObject>,
        std::shared_ptr<ErrorObject>,

        TypeType,

        NoneType,
        LiteralType,

        AnyType,
        IntType,
        FloatType,
        StringType,
        BooleanType,

        ListType,
        TupleType,
        SetType,
        MapType,
        VariantType,
        IntersectionType,
        EnumMemberType,
        Signature_ptr,

        ModuleType_ptr,
        ClassType_ptr,
        TraitType_ptr,
        EnumType_ptr,
        TypeAlias_ptr,

        TemplateParameterType_ptr>;

    UnderlyingVariant value;

    Object() = default;

    bool is_type_object() const;
    bool is_runtime_value() const;
    bool is_callable() const;

    IterableAbstractObject* as_iterable();

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

    template <typename T> const T* try_as() const
    {
        return std::get_if<T>(&value);
    }

    template <typename T> T* try_as()
    {
        return std::get_if<T>(&value);
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

template <typename T> inline T try_unwrap_ptr(Object_ptr obj)
{
    if (!obj || !obj->is<T>())
    {
        return nullptr;
    }
    return obj->as<T>();
}

inline bool is_numeric_type(Object_ptr type)
{
    return type && type->is_any_of<IntType, FloatType>();
}

// ============================================================================
// Utils
// ============================================================================

std::string stringify_object(Object_ptr value);
std::string mangle_object(Object_ptr value);
std::string mangle_object(const ObjectVector& values);

bool are_equal_types(Object_ptr left, Object_ptr right);
bool are_equal_types(ObjectVector left_vector, ObjectVector right_vector);
bool are_equal_types_unordered(
    ObjectVector left_vector,
    ObjectVector right_vector
);

Object_ptr convert_type(Object_ptr type, Object_ptr operand);
Object_ptr unwrap_type_alias(Object_ptr type);
Object_ptr unwrap_completely(Object_ptr type);

std::string get_canonical_trait_name(const ObjectVector& traits);

} // namespace Wasp
