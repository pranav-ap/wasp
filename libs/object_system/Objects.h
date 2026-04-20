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
// Runtime Object Interfaces
// ============================================================================

struct AbstractObject
{
};
struct ScalarObject : public AbstractObject
{
};
struct CompositeObject : public AbstractObject
{
};
struct ActionObject : public AbstractObject
{
};
struct NoneObject : public AbstractObject
{
};

struct IterableAbstractObject : public AbstractObject
{
    virtual Object_ptr get_iter() = 0;
};

// ============================================================================
// Scalar Objects
// ============================================================================

struct IntObject : public ScalarObject
{
    int value;
    IntObject(int value) : value(value) {};
};

struct FloatObject : public ScalarObject
{
    double value;
    FloatObject(double value) : value(value) {};
};

struct StringObject : public ScalarObject, public IterableAbstractObject
{
    std::string value;
    StringObject(std::string value) : value(std::move(value)) {};
    virtual Object_ptr get_iter() override;
};

struct BooleanObject : public ScalarObject
{
    bool value;
    BooleanObject(bool value) : value(value) {};
};

// ============================================================================
// Composite Objects
// ============================================================================

struct IteratorObject : public CompositeObject
{
    ObjectVector vec;
    size_t index;

    IteratorObject(ObjectVector v) : vec(std::move(v)), index(0)
    {
    }

    bool has_next() const;
    std::optional<Object_ptr> get_next();
    void reset_iter();
};

struct ListObject : public CompositeObject, public IterableAbstractObject
{
    ObjectVector values;

    ListObject();
    ListObject(ObjectVector values);

    Object_ptr append(Object_ptr value);
    Object_ptr prepend(Object_ptr value);
    Object_ptr pop_back();
    Object_ptr pop_front();
    Object_ptr get(Object_ptr index);
    Object_ptr set(Object_ptr index, Object_ptr value);

    void clear();
    bool is_empty();
    int get_length();
    virtual Object_ptr get_iter() override;
};

struct TupleObject : public CompositeObject
{
    ObjectVector values;

    TupleObject(ObjectVector values) : values(std::move(values)) {};

    Object_ptr get(Object_ptr index);
    Object_ptr set(Object_ptr index, Object_ptr value);
    Object_ptr set(ObjectVector values);
    int get_length();
};

struct SetObject : public CompositeObject, public IterableAbstractObject
{
    ObjectVector values;

    SetObject(ObjectVector values) : values(std::move(values)) {};

    ObjectVector get();
    Object_ptr set(ObjectVector values);
    virtual Object_ptr get_iter() override;
    int get_length();
};

struct MapObject : public CompositeObject, public IterableAbstractObject
{
    std::map<Object_ptr, Object_ptr> pairs;

    MapObject(std::map<Object_ptr, Object_ptr> pairs) : pairs(std::move(pairs)) {};

    Object_ptr insert(Object_ptr key, Object_ptr value);
    Object_ptr get_pair(Object_ptr key);
    Object_ptr get(Object_ptr key);
    Object_ptr set(Object_ptr key, Object_ptr value);
    virtual Object_ptr get_iter() override;
    int get_size();
};

struct VariantObject : public CompositeObject
{
    Object_ptr value;
    VariantObject(Object_ptr value) : value(std::move(value)) {};
    bool has_value();
};

// ============================================================================
// Action Objects
// ============================================================================

struct BreakObject : public ActionObject
{
};

struct ContinueObject : public ActionObject
{
};

struct RedoObject : public ActionObject
{
};

struct ReturnObject : public ActionObject
{
    std::optional<Object_ptr> value;

    ReturnObject() : value(std::nullopt) {};
    ReturnObject(Object_ptr value) : value(std::optional(std::move(value))) {};
};

struct ErrorObject : public ActionObject
{
    std::string message;

    ErrorObject() : message("") {};
    ErrorObject(std::string message) : message(std::move(message)) {};
};

// ============================================================================
// Function Objects
// ============================================================================

struct FunctionBlueprintObject : public CompositeObject
{
    CodeObject code;
    std::string name;

    FunctionBlueprintObject(CodeObject code) : code(std::move(code))
    {
    }
    FunctionBlueprintObject(CodeObject code, std::string name)
        : code(std::move(code)), name(std::move(name))
    {
    }
};

using FunctionBlueprintObject_ptr = std::shared_ptr<FunctionBlueprintObject>;

struct FunctionRuntimeObject
{
    FunctionBlueprintObject_ptr blueprint;
    ObjectVector upvalues;

    FunctionRuntimeObject(FunctionBlueprintObject_ptr blueprint)
        : blueprint(std::move(blueprint)), upvalues({})
    {
    }

    FunctionRuntimeObject(FunctionBlueprintObject_ptr blueprint, ObjectVector upvalues)
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

struct NativeFunctionObject : public CompositeObject
{
    NativeFnType function;
    int arity;
    std::string name;

    NativeFunctionObject(NativeFnType function, int arity, std::string name)
        : function(std::move(function)), arity(arity), name(name)
    {
    }
};

// ============================================================================
// Overloads
// ============================================================================

struct ObjectOverloadList : public CompositeObject
{
    std::string name;
    ObjectVector overloads;

    ObjectOverloadList(std::string name) : name(std::move(name))
    {
    }

    ObjectOverloadList(ObjectVector overloads) : overloads(std::move(overloads))
    {
    }

    ObjectOverloadList(std::string name, ObjectVector overloads)
        : name(std::move(name)), overloads(std::move(overloads))
    {
    }

    void add_overload(Object_ptr object)
    {
        overloads.push_back(std::move(object));
    }
};

using ObjectOverloadList_ptr = std::shared_ptr<ObjectOverloadList>;

// ============================================================================
// Membered Objects
// ============================================================================

struct MemberedCompositeObject : public CompositeObject
{
    ObjectVector members;

    MemberedCompositeObject(ObjectVector members) : members(std::move(members))
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
    ClassBlueprintObject(ObjectVector members) : MemberedCompositeObject(std::move(members))
    {
    }
};

struct InstanceObject : public MemberedCompositeObject
{
    InstanceObject(ObjectVector members) : MemberedCompositeObject(std::move(members))
    {
    }
};

// ============================================================================
// Type Interfaces
// ============================================================================

struct AnyType : public AbstractObject
{
};

struct NoneType : public AnyType
{
};

struct ScalarType : public AnyType
{
};

struct LiteralType : public AnyType
{
};

struct CompositeType : public AnyType
{
};

struct NamedDefinitionType : public AnyType
{
    std::string name;
    NamedDefinitionType(std::string name) : name(std::move(name)) {};
};

// ============================================================================
// Scalar & Literal Types
// ============================================================================

struct IntType : public ScalarType
{
};

struct FloatType : public ScalarType
{
};

struct StringType : public ScalarType
{
};

struct BooleanType : public ScalarType
{
};

struct IntLiteralType : public LiteralType
{
    int value;
    IntLiteralType(int value) : value(value) {};
};

struct FloatLiteralType : public LiteralType
{
    double value;
    FloatLiteralType(double value) : value(value) {};
};

struct StringLiteralType : public LiteralType
{
    std::string value;
    StringLiteralType(std::string value) : value(std::move(value)) {};
};

struct BooleanLiteralType : public LiteralType
{
    bool value;
    BooleanLiteralType(bool value) : value(value) {};
};

// ============================================================================
// Composite Types
// ============================================================================

struct ListType : public CompositeType
{
    Object_ptr element_type;
    ListType(Object_ptr element_type) : element_type(std::move(element_type)) {};
};

struct TupleType : public CompositeType
{
    ObjectVector element_types;
    TupleType(ObjectVector element_types) : element_types(std::move(element_types)) {};
};

struct SetType : public CompositeType
{
    Object_ptr element_type;
    SetType(Object_ptr element_type) : element_type(std::move(element_type)) {};
};

struct MapType : public CompositeType
{
    Object_ptr key_type;
    Object_ptr value_type;

    MapType(Object_ptr key_type, Object_ptr value_type)
        : key_type(std::move(key_type)), value_type(std::move(value_type)) {};
};

struct VariantType : public CompositeType
{
    ObjectVector types;
    VariantType(ObjectVector types) : types(std::move(types)) {};
};

// ============================================================================
// Function Types
// ============================================================================

struct Signature : public AnyType
{
    ObjectVector parameter_types;
    Object_ptr return_type;

    Signature(ObjectVector input_types, Object_ptr return_type)
        : parameter_types(std::move(input_types)), return_type(std::move(return_type)) {};
};

struct FunctionType : public Signature
{
    using Signature::Signature;
};

struct MethodType : public Signature
{
    using Signature::Signature;
};

// ============================================================================
// Membered Types
// ============================================================================

struct ModuleType : public CompositeType
{
    std::string name;
    std::filesystem::path absolute_filepath;

    ObjectStringMap members;
    StringVector ordered_keys;

    ModuleType(
        std::string name,
        std::filesystem::path absolute_filepath,
        StringVector ordered_keys,
        ObjectStringMap members
    )
        : name(std::move(name)), absolute_filepath(std::move(absolute_filepath)),
          ordered_keys(std::move(ordered_keys)), members(std::move(members))
    {
    }

    bool contains_member(const std::string& member_name) const;

    Object_ptr get_member(const std::string& member_name) const;
    void set_member(const std::string& member_name, Object_ptr value);

    Object_ptr get_member(int member_id) const;
    void set_member(int member_id, Object_ptr value);

    int get_member_index(const std::string& member_name) const;
};

using ModuleType_ptr = std::shared_ptr<ModuleType>;

struct ClassType : public CompositeType
{
    std::string name;

    StringVector fields;
    StringVector methods;

    ObjectStringMap members;

    ClassType(std::string name, ObjectStringMap members, StringVector fields, StringVector methods)
        : name(std::move(name)), fields(std::move(fields)), methods(std::move(methods)),
          members(std::move(members))
    {
    }

    bool contains_member(const std::string& member_name) const;

    Object_ptr get_member(const std::string& member_name) const;
    void set_member(const std::string& member_name, Object_ptr value);

    int get_member_index(const std::string& member_name) const;
    Object_ptr get_member(int member_id) const;

    void add_overload(const std::string& member_name, Object_ptr overload);
    ObjectVector get_overloads(const std::string& member_name) const;

    ObjectVector get_fields() const;
    ObjectVector get_methods() const;
    ObjectVector get_members() const;
};

using ClassType_ptr = std::shared_ptr<ClassType>;

struct RecordType
{
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

        std::shared_ptr<FunctionType>,
        std::shared_ptr<MethodType>,

        std::shared_ptr<RecordType>,
        std::shared_ptr<ModuleType>,
        std::shared_ptr<ClassType>>;

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
ObjectVector to_vector(std::string text);
bool are_equal_types(Object_ptr left, Object_ptr right);
bool are_equal_types(ObjectVector left_vector, ObjectVector right_vector);
bool are_equal_types_unordered(ObjectVector left_vector, ObjectVector right_vector);
Object_ptr convert_type(Object_ptr type, Object_ptr operand);

} // namespace Wasp
