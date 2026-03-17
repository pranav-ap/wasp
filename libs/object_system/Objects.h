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

namespace Wasp {

struct Object;
using Object_ptr = std::shared_ptr<Object>;
using ObjectVector = std::vector<Object_ptr>;
using StringVector = std::vector<std::string>;

// Base Classes

struct AbstractObject {};

struct IterableAbstractObject : public AbstractObject {
    virtual Object_ptr get_iter() = 0;
};

struct ScalarObject : public AbstractObject {};
struct CompositeObject : public AbstractObject {};
struct ActionObject : public AbstractObject {};
struct NoneObject : public AbstractObject {};

struct DefinitionObject : public AbstractObject {
    std::string name;
    DefinitionObject(std::string name) : name(std::move(name)) {};
};

// Objects

// Scalar Objects

struct IntObject : public ScalarObject {
    int value;
    IntObject(int value) : value(value) {};
};

struct FloatObject : public ScalarObject {
    double value;
    FloatObject(double value) : value(value) {};
};

struct StringObject : public ScalarObject, public IterableAbstractObject {
    std::string value;
    StringObject(std::string value) : value(std::move(value)) {};
    virtual Object_ptr get_iter() override;
};

struct BooleanObject : public ScalarObject {
    bool value;
    BooleanObject(bool value) : value(value) {};
};

// Composite Objects

struct IteratorObject : public CompositeObject {
    ObjectVector vec;
    size_t index;

    IteratorObject(ObjectVector v) : vec(std::move(v)), index(0) {}

    std::optional<Object_ptr> get_next();
    void reset_iter();
};

struct ListObject : public CompositeObject, public IterableAbstractObject {
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

struct TupleObject : public CompositeObject {
    ObjectVector values;

    TupleObject(ObjectVector values) : values(std::move(values)) {};

    Object_ptr get(Object_ptr index);
    Object_ptr set(Object_ptr index, Object_ptr value);
    Object_ptr set(ObjectVector values);

    int get_length();
};

struct SetObject : public CompositeObject, public IterableAbstractObject {
    ObjectVector values;

    SetObject(ObjectVector values) : values(std::move(values)) {};

    ObjectVector get();
    Object_ptr set(ObjectVector values);

    virtual Object_ptr get_iter() override;

    int get_length();
};

struct MapObject : public CompositeObject, public IterableAbstractObject {
    std::map<Object_ptr, Object_ptr> pairs;

    Object_ptr insert(Object_ptr key, Object_ptr value);
    Object_ptr get_pair(Object_ptr key);
    Object_ptr get(Object_ptr key);
    Object_ptr set(Object_ptr key, Object_ptr value);

    virtual Object_ptr get_iter() override;

    int get_size();
};

struct VariantObject : public CompositeObject {
    Object_ptr value;

    VariantObject(Object_ptr value) : value(std::move(value)) {};

    bool has_value();
};

struct FunctionObject : public CompositeObject {
    CodeObject code;

    std::string name;
    std::map<int, std::string> exports_map;

    FunctionObject(CodeObject code) : code(std::move(code)), name(""), exports_map({}) {}

    FunctionObject(CodeObject code, std::string name, std::map<int, std::string> exports_map)
        : code(std::move(code)), name(std::move(name)), exports_map(std::move(exports_map)) {}

    std::string get_name_for_index(int index) const {
        auto it = exports_map.find(index);
        return (it != exports_map.end()) ? it->second : "";
    }
};

struct FunctionVMObject : public FunctionObject {
    ObjectVector upvalues;

    FunctionVMObject(CodeObject code) : FunctionObject(code), upvalues({}) {}

    FunctionVMObject(
        ObjectVector upvalues,
        CodeObject code,
        std::string name,
        std::map<int, std::string> exports_map
    )
        : FunctionObject(code, name, exports_map), upvalues(std::move(upvalues)) {}
};

using NativeFnType = std::function<Object_ptr(const std::vector<Object_ptr>&)>;

struct NativeFunctionObject : public CompositeObject {
    NativeFnType function;
    int arity;
    std::string name;

    NativeFunctionObject(NativeFnType function, int arity, std::string name)
        : function(std::move(function)), arity(arity), name(name) {}
};

struct ModuleObject : public CompositeObject {
    std::string name;
    std::map<std::string, Object_ptr> members;

    ModuleObject(std::string name, std::map<std::string, Object_ptr> members)
        : name(std::move(name)), members(std::move(members)) {}

    Object_ptr get_member(const std::string& member_name);
    void set_member(const std::string& member_name, Object_ptr value);
};

// Action Objects

struct BreakObject : public ActionObject {};
struct ContinueObject : public ActionObject {};
struct RedoObject : public ActionObject {};

struct ReturnObject : public ActionObject {
    std::optional<Object_ptr> value;

    ReturnObject() : value(std::nullopt) {};
    ReturnObject(Object_ptr value) : value(std::optional(std::move(value))) {};
};

struct ErrorObject : public ActionObject {
    std::string message;

    ErrorObject() : message("") {};
    ErrorObject(std::string message) : message(std::move(message)) {};
};

// Type Objects

struct AnyType : public AbstractObject {};

struct NoneType : public AnyType {};

struct ScalarType : public AnyType {};
struct LiteralType : public AnyType {};
struct CompositeType : public AnyType {};

struct NamedDefinitionType : public AnyType {
    std::string name;

    NamedDefinitionType(std::string name) : name(std::move(name)) {};
};

// Scalar Types
struct IntType : public ScalarType {
    std::string name = "int";
};

struct FloatType : public ScalarType {};
struct StringType : public ScalarType {};
struct BooleanType : public ScalarType {};

// Literal Types
struct IntLiteralType : public LiteralType {
    int value;
    IntLiteralType(int value) : value(value) {};
};

struct FloatLiteralType : public LiteralType {
    double value;
    FloatLiteralType(double value) : value(value) {};
};

struct StringLiteralType : public LiteralType {
    std::string value;
    StringLiteralType(std::string value) : value(std::move(value)) {};
};

struct BooleanLiteralType : public LiteralType {
    bool value;
    BooleanLiteralType(bool value) : value(value) {};
};

// Composite Types
struct ListType : public CompositeType {
    Object_ptr element_type;
    ListType(Object_ptr element_type) : element_type(std::move(element_type)) {};
};

struct TupleType : public CompositeType {
    ObjectVector element_types;
    TupleType(ObjectVector element_types) : element_types(std::move(element_types)) {};
};

struct SetType : public CompositeType {
    Object_ptr element_type;
    SetType(Object_ptr element_type) : element_type(std::move(element_type)) {};
};

struct MapType : public CompositeType {
    Object_ptr key_type;
    Object_ptr value_type;

    MapType(Object_ptr key_type, Object_ptr value_type)
        : key_type(std::move(key_type)), value_type(std::move(value_type)) {};
};

struct VariantType : public CompositeType {
    ObjectVector types;
    VariantType(ObjectVector types) : types(std::move(types)) {};
};

struct FunctionType : public AnyType {
    ObjectVector input_types;
    std::optional<Object_ptr> return_type;

    FunctionType(ObjectVector input_types)
        : input_types(std::move(input_types)), return_type(std::nullopt) {};

    FunctionType(ObjectVector input_types, Object_ptr return_type)
        : input_types(std::move(input_types)),
          return_type(std::make_optional(std::move(return_type))) {};
};

struct MemberedType : public CompositeType {
    std::map<std::string, Object_ptr> members;

    MemberedType(std::map<std::string, Object_ptr> members) : members(std::move(members)) {}

    bool contains_member(const std::string& member_name) const {
        return members.find(member_name) != members.end();
    }

    Object_ptr get_member_type(const std::string& member_name) const {
        auto it = members.find(member_name);
        if (it != members.end()) {
            return it->second;
        }

        return nullptr;
    }
};

struct RecordType : public MemberedType {
    RecordType(std::map<std::string, Object_ptr> members) : MemberedType(std::move(members)) {};
};

struct ModuleType : public MemberedType {
    ModuleType(std::map<std::string, Object_ptr> members) : MemberedType(std::move(members)) {}
};

// Object STRUCT

struct Object {
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

        std::shared_ptr<FunctionObject>,
        std::shared_ptr<FunctionVMObject>,
        std::shared_ptr<NativeFunctionObject>,
        std::shared_ptr<ModuleObject>,

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

template <typename T> inline Object_ptr make_object(T&& val) {
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
