#pragma once

#include "CFGraph.h"
#include "Doctor.h"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <iterator>
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

// Base Classes

struct AbstractObject
{
};

struct IterableAbstractObject : public AbstractObject
{
    virtual Object_ptr get_iter() = 0;
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

struct MemberedCompositeObject : public CompositeObject
{
    ObjectStringMap members;

    MemberedCompositeObject(ObjectStringMap members) : members(std::move(members)) {}

    bool contains_member(const std::string& member_name) const
    {
        return members.contains(member_name);
    }

    Object_ptr get_member(const std::string& member_name) const
    {
        Doctor::get().assert(
            contains_member(member_name),
            WaspStage::VM,
            "Member '" + member_name + "' not found!"
        );

        return members.at(member_name);
    }

    void set_member(const std::string& member_name, Object_ptr value)
    {
        members[member_name] = std::move(value);
    }

    Object_ptr get_member(int member_id) const
    {
        Doctor::get().assert(
            member_id >= 0 && member_id < static_cast<int>(members.size()),
            WaspStage::VM,
            "Member index out of bounds!");

        auto it = members.begin();
        std::advance(it, member_id);
        return it->second;
    }

    void set_member(int member_id, Object_ptr value)
    {
        Doctor::get().assert(
            member_id >= 0 && member_id < static_cast<int>(members.size()),
            WaspStage::VM,
            "Member index out of bounds!");

        auto it = members.begin();
        std::advance(it, member_id);
        it->second = std::move(value);
    }

    int get_member_index(const std::string& member_name) const
    {
        Doctor::get().assert(
            contains_member(member_name),
            WaspStage::VM,
            "Member '" + member_name + "' not found!"
        );

        auto it = members.find(member_name);
        return static_cast<int>(std::distance(members.begin(), it));
    }
};

struct DefinitionObject : public AbstractObject
{
    std::string name;
    DefinitionObject(std::string name) : name(std::move(name)) {};
};

// Objects

// Scalar Objects

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

// Composite Objects

struct IteratorObject : public CompositeObject
{
    ObjectVector vec;
    size_t index;

    IteratorObject(ObjectVector v) : vec(std::move(v)), index(0) {}

    std::optional<Object_ptr> get_next();
    void reset_iter();
};

struct ListObject : public CompositeObject, public IterableAbstractObject
{
    std::vector<Object_ptr> values;

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

    ListObject(ObjectVector values);
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

struct StaticFunctionObject : public CompositeObject
{
    CodeObject code;

    std::vector<int> parameter_symbol_ids;

    std::string name;
    std::map<int, std::string> symbol_id_to_name_map;
    std::map<int, std::string> upvalue_index_to_name_map;

    StaticFunctionObject(CodeObject code) : code(std::move(code)) {}

    StaticFunctionObject(
        CodeObject code,
        std::vector<int> parameter_symbol_ids,
        std::string name,
        std::map<int, std::string> symbol_id_to_name_map,
        std::map<int, std::string> upvalue_index_to_name_map)
        : code(std::move(code)), name(std::move(name)), parameter_symbol_ids(parameter_symbol_ids),
          symbol_id_to_name_map(std::move(symbol_id_to_name_map)),
          upvalue_index_to_name_map(std::move(upvalue_index_to_name_map))
    {
    }

    std::string get_name_for_symbol_id(int index) const
    {
        Doctor::get().assert(
            symbol_id_to_name_map.contains(index),
            WaspStage::Compiler,
            "Invalid symbol ID");

        return symbol_id_to_name_map.at(index);
    }

    std::string get_name_for_upvalue_index(int index) const
    {
        Doctor::get().assert(
            upvalue_index_to_name_map.contains(index),
            WaspStage::Compiler,
            "Invalid Upvalue Index");

        return upvalue_index_to_name_map.at(index);
    }
};

using StaticFunctionObject_ptr = std::shared_ptr<StaticFunctionObject>;

struct RuntimeFunctionObject
{
    StaticFunctionObject_ptr blueprint;
    ObjectVector upvalues;

    RuntimeFunctionObject(StaticFunctionObject_ptr blueprint, ObjectVector upvalues = {})
        : blueprint(std::move(blueprint)), upvalues(std::move(upvalues))
    {
    }

    void set_upvalue(int index, Object_ptr value) { upvalues[index] = std::move(value); }
};

using RuntimeFunctionObject_ptr = std::shared_ptr<RuntimeFunctionObject>;

using NativeFnType = std::function<Object_ptr(const std::vector<Object_ptr>&)>;

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

struct OverloadsSet : public CompositeObject
{
    ObjectVector overloads;

    OverloadsSet(ObjectVector overloads) : overloads(std::move(overloads)) {};

    void add_overload(Object_ptr object) { overloads.push_back(std::move(object)); }
};

struct OverloadedObjectsSet : public OverloadsSet
{
    OverloadedObjectsSet(ObjectVector overloads) : OverloadsSet(std::move(overloads)) {};
};

struct OverloadedTypesSet : public OverloadsSet
{
    OverloadedTypesSet(ObjectVector overloads) : OverloadsSet(std::move(overloads)) {};
};

struct ModuleObject : public MemberedCompositeObject
{
    std::string name;

    ModuleObject(std::string name, ObjectStringMap members)
        : name(std::move(name)), MemberedCompositeObject(std::move(members))
    {
    }
};

// Action Objects

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

// Type Objects

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

// Scalar Types
struct IntType : public ScalarType
{
    std::string name = "int";
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

// Literal Types
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

// Composite Types
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

struct FunctionType : public AnyType
{
    ObjectVector input_types;
    std::optional<Object_ptr> return_type;

    FunctionType(ObjectVector input_types)
        : input_types(std::move(input_types)), return_type(std::nullopt) {};

    FunctionType(ObjectVector input_types, Object_ptr return_type)
        : input_types(std::move(input_types)),
          return_type(std::make_optional(std::move(return_type))) {};
};

struct RecordType : public MemberedCompositeObject
{
    RecordType(ObjectStringMap members) : MemberedCompositeObject{std::move(members)} {}
};

struct ModuleType : public MemberedCompositeObject
{
    std::string module_name;
    std::filesystem::path absolute_filepath;

    ModuleType(
        std::string module_name,
        std::filesystem::path absolute_filepath,
        ObjectStringMap members)
        : module_name(std::move(module_name)), absolute_filepath(std::move(absolute_filepath)),
          MemberedCompositeObject(std::move(members))
    {
    }
};

// Object STRUCT

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

        std::shared_ptr<StaticFunctionObject>,
        std::shared_ptr<RuntimeFunctionObject>,
        std::shared_ptr<NativeFunctionObject>,
        std::shared_ptr<ModuleObject>,
        std::shared_ptr<OverloadedObjectsSet>,
        std::shared_ptr<OverloadedTypesSet>,

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
        FunctionType,
        RecordType,
        ModuleType>;

    UnderlyingVariant value;

    Object() = default;

    template <typename T> Object(T&& val) : value(std::forward<T>(val)) {}

    template <typename T> bool is() const { return std::holds_alternative<T>(value); }

    template <typename T> const T& as() const { return std::get<T>(value); }
    template <typename T> T& as() { return std::get<T>(value); }

    template <typename T> const T* try_as() const { return std::get_if<T>(&value); }
    template <typename T> T* try_as() { return std::get_if<T>(&value); }
};

template <typename T> inline Object_ptr make_object(T&& val)
{
    return std::make_shared<Object>(std::forward<T>(val));
}

// Utils

std::string stringify_object(Object_ptr value);

ObjectVector to_vector(std::string text);

bool are_equal_types(Object_ptr left, Object_ptr right);
bool are_equal_types(ObjectVector left_vector, ObjectVector right_vector);
bool are_equal_types_unordered(ObjectVector left_vector, ObjectVector right_vector);

Object_ptr convert_type(Object_ptr type, Object_ptr operand);
} // namespace Wasp
