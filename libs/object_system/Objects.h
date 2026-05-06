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

using StringVector = std::vector<std::string>;

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

struct NamedDefinitionType
{
    std::string name;
    NamedDefinitionType(std::string name) : name(std::move(name)) {};
};

// ============================================================================
// Scalar & Literal Types
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

struct IntLiteralType
{
    int value;
};

struct FloatLiteralType
{
    double value;
};

struct StringLiteralType
{
    std::string value;
};

struct BooleanLiteralType
{
    bool value;
};

// ============================================================================
// Template Base (Constraints & Generics)
// ============================================================================

struct GenericType
{
    std::string name;
    Object_ptr constraint_type;
};

using GenericType_ptr = std::shared_ptr<GenericType>;

struct TemplatableType
{
    ObjectStringMap generics;
    StringVector expected_generic_names_order;
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

struct MapType
{
    Object_ptr key_type;
    Object_ptr value_type;
};

// ============================================================================
// Function Types
// ============================================================================

struct Signature : public TemplatableType
{
    ObjectVector parameter_types;
    Object_ptr return_type;

    Signature(
        ObjectVector parameter_types,
        Object_ptr return_type,
        ObjectStringMap generics,
        StringVector ordered_generic_names
    )
        : TemplatableType(std::move(generics), std::move(ordered_generic_names)),
          parameter_types(std::move(parameter_types)),
          return_type(std::move(return_type))
    {
    }
};

using Signature_ptr = std::shared_ptr<Signature>;

// ============================================================================
// Membered Types
// ============================================================================

struct BaseMemberedType
{
    std::string name;
    ObjectStringMap member_types;

    BaseMemberedType(std::string name, ObjectStringMap member_types = {})
        : name(std::move(name)), member_types(std::move(member_types))
    {
    }

    bool contains_member(const std::string& member_name) const;
    Object_ptr get_member(const std::string& member_name) const;
    void set_member(const std::string& member_name, Object_ptr value);
};

struct ModuleType : public BaseMemberedType
{
    std::filesystem::path absolute_filepath;
    StringVector ordered_keys;

    ModuleType(
        std::string name,
        std::filesystem::path absolute_filepath,
        StringVector ordered_keys,
        ObjectStringMap members
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

struct BaseOOPType : public BaseMemberedType, public TemplatableType
{
    StringVector methods;
    StringVector pures;
    StringVector statics;

    BaseOOPType(
        std::string name,
        ObjectStringMap members,
        StringVector methods,
        StringVector pures,
        StringVector statics,
        ObjectStringMap generics,
        StringVector ordered_generic_names
    )
        : BaseMemberedType(std::move(name), std::move(members)),
          TemplatableType(std::move(generics), std::move(ordered_generic_names)),
          methods(std::move(methods)), pures(std::move(pures)),
          statics(std::move(statics))
    {
    }

    void add_overload(const std::string& member_name, Object_ptr overload);
    ObjectVector get_overloads(const std::string& member_name) const;
    ObjectVector get_methods() const;
    ObjectVector get_pures() const;
    ObjectVector get_statics() const;

    bool is_pure(std::string member_name) const;
    bool is_static(std::string member_name) const;
};

struct ClassType : public BaseOOPType
{
    StringVector fields;

    ClassType(
        std::string name,
        ObjectStringMap members,
        StringVector fields,
        StringVector methods,
        StringVector pures,
        StringVector statics,
        ObjectStringMap generics,
        StringVector ordered_generic_names
    )
        : BaseOOPType(
              std::move(name),
              std::move(members),
              std::move(methods),
              std::move(pures),
              std::move(statics),
              std::move(generics),
              std::move(ordered_generic_names)
          ),
          fields(std::move(fields))
    {
    }

    using BaseMemberedType::get_member;
    int get_member_index(const std::string& member_name) const;
    Object_ptr get_member(int member_id) const;
    ObjectVector get_fields() const;
    ObjectVector get_members() const;
};

using ClassType_ptr = std::shared_ptr<ClassType>;

struct TraitType : public BaseOOPType
{
    using BaseOOPType::BaseOOPType;

    using BaseMemberedType::get_member;
    int get_member_index(const std::string& member_name) const;
    Object_ptr get_member(int member_id) const;
    ObjectVector get_members() const;
};

using TraitType_ptr = std::shared_ptr<TraitType>;

struct EnumType
{
    std::string name;
    std::map<std::string, int> members;
    std::map<std::string, std::shared_ptr<EnumType>> nested_enums;

    EnumType(std::string name) : name(std::move(name))
    {
    }
};

using EnumType_ptr = std::shared_ptr<EnumType>;

// ============================================================================
// Alias
// ============================================================================

struct TypeAlias : public TemplatableType
{
    std::string name;
    Object_ptr underlying_type;

    TypeAlias(
        std::string name,
        Object_ptr underlying_type,
        ObjectStringMap generics,
        StringVector ordered_generic_names
    )
        : TemplatableType(std::move(generics), std::move(ordered_generic_names)),
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
    StringObject(std::string value) : value(std::move(value)) {};
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

    MapObject(std::map<Object_ptr, Object_ptr> pairs = {}) : pairs(std::move(pairs))
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

struct BreakObject
{
};

struct ContinueObject
{
};

struct RedoObject
{
};

struct ReturnObject
{
    std::optional<Object_ptr> value;

    ReturnObject(std::optional<Object_ptr> value = std::nullopt) : value(std::move(value))
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

    void set_upvalue(int index, Object_ptr value)
    {
        upvalues[index] = std::move(value);
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

// ============================================================================
// Membered Objects
// ============================================================================

struct MemberedCompositeObject
{
    ObjectVector members;

    MemberedCompositeObject(ObjectVector members = {}) : members(std::move(members))
    {
    }

    Object_ptr get_member(int member_id) const;
    void set_member(int member_id, Object_ptr value);
    int get_member_count() const;
};

struct ModuleObject : public MemberedCompositeObject
{
    std::string name;

    ModuleObject(std::string name, ObjectVector members)
        : name(std::move(name)), MemberedCompositeObject(std::move(members))
    {
    }
};

struct ClassBlueprintObject : public MemberedCompositeObject
{
    int fields_count;

    ClassBlueprintObject(ObjectVector members = {}, int fields_count = 0)
        : MemberedCompositeObject(std::move(members)), fields_count(fields_count)
    {
    }
};

struct InstanceObject : public MemberedCompositeObject
{
    using MemberedCompositeObject::MemberedCompositeObject;
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
        std::shared_ptr<InstanceObject>,

        std::shared_ptr<ObjectOverloadList>,

        std::shared_ptr<BreakObject>,
        std::shared_ptr<ContinueObject>,
        std::shared_ptr<RedoObject>,
        std::shared_ptr<ReturnObject>,
        std::shared_ptr<ErrorObject>,

        TypeType,
        AnyType,
        NoneType,
        NamedDefinitionType,
        IntType,
        FloatType,
        StringType,
        BooleanType,
        IntLiteralType,
        FloatLiteralType,
        StringLiteralType,
        BooleanLiteralType,

        ListType,
        TupleType,
        SetType,
        MapType,
        VariantType,

        Signature_ptr,

        ModuleType_ptr,
        ClassType_ptr,
        TraitType_ptr,
        EnumType_ptr,
        TypeAlias_ptr,

        GenericType_ptr>;

    UnderlyingVariant value;

    Object() = default;

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
};

template <typename T> inline Object_ptr make_object(T&& val)
{
    return std::make_shared<Object>(std::forward<T>(val));
}

// ============================================================================
// Utils
// ============================================================================

std::string stringify_object(Object_ptr value);
std::string mangle_object(Object_ptr value);
std::string mangle_object(const ObjectVector& values);

ObjectVector to_vector(std::string text);

bool are_equal_types(Object_ptr left, Object_ptr right);
bool are_equal_types(ObjectVector left_vector, ObjectVector right_vector);
bool are_equal_types_unordered(ObjectVector left_vector, ObjectVector right_vector);

Object_ptr convert_type(Object_ptr type, Object_ptr operand);

} // namespace Wasp
