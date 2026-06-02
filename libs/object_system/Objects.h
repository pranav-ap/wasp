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

    GenericType() = default;

    GenericType(std::string name, Object_ptr constraint_type)
        : name(std::move(name)), constraint_type(std::move(constraint_type))
    {
    }
};

using GenericType_ptr = std::shared_ptr<GenericType>;

struct TemplateType
{
    ObjectStringMap template_parameters;
    StringVector ordered_parameter_names;

    TemplateType() = default;

    TemplateType(
        ObjectStringMap template_parameters,
        StringVector ordered_parameter_names
    )
        : template_parameters(std::move(template_parameters)),
          ordered_parameter_names(std::move(ordered_parameter_names))
    {
    }

    int generics_count() const;
    bool exists() const;
    std::pair<std::string, Object_ptr> get_generic(size_t index) const;
    std::vector<std::pair<std::string, Object_ptr>> get_ordered_generics() const;
};

using TemplateType_ptr = std::shared_ptr<TemplateType>;

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
// Signature
// ============================================================================

struct Signature
{
    ObjectVector parameter_types;
    Object_ptr return_type;
    TemplateType_ptr template_type;

    Signature(
        ObjectVector parameter_types,
        Object_ptr return_type,
        TemplateType_ptr template_type = std::make_shared<TemplateType>()
    )
        : parameter_types(std::move(parameter_types)),
          return_type(std::move(return_type)),
          template_type(std::move(template_type))
    {
    }
};

using Signature_ptr = std::shared_ptr<Signature>;

struct SignaturesSet
{
    ObjectVector types;

    SignaturesSet(ObjectVector types = {}) : types(std::move(types))
    {
    }

    Object_ptr get_signature(int overload_index) const;
    void add_signature(const Object_ptr signature);
};

using SignaturesSet_ptr = std::shared_ptr<SignaturesSet>;

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

struct RecordType
{
    ObjectStringMap types;
    StringVector ordered_keys;

    RecordType(ObjectStringMap types = {}, StringVector ordered_keys = {})
        : types(std::move(types)), ordered_keys(std::move(ordered_keys))
    {
    }

    int get_index(const std::string& field_name) const;
    Object_ptr get_type(const std::string& field_name) const;
    bool contains(const std::string& field_name) const;
};

using RecordType_ptr = std::shared_ptr<RecordType>;

struct BagType
{
    ObjectStringMap types;
    StringVector ordered_keys;
    ITablesMap itables;

    BagType(
        ObjectStringMap types = {},
        StringVector ordered_keys = {},
        ITablesMap itables = {}
    )
        : types(std::move(types)), ordered_keys(std::move(ordered_keys)),
          itables(std::move(itables))
    {
    }

    int get_index(const std::string& function_name) const;
    Object_ptr get_signatures(const std::string& function_name) const;
    bool contains(const std::string& field_name) const;
};

using BagType_ptr = std::shared_ptr<BagType>;

struct OopsType
{
    int type_id;
    std::string name;

    RecordType_ptr record_type;
    BagType_ptr bag_type;

    ObjectVector traits;

    TemplateType_ptr template_type;

    explicit OopsType(
        std::string name,
        RecordType_ptr record_type = std::make_shared<RecordType>(),
        BagType_ptr bag_type = std::make_shared<BagType>(),
        TemplateType_ptr template_type = std::make_shared<TemplateType>()
    )
        : type_id(get_next_type_id()), name(std::move(name)),
          record_type(std::move(record_type)), bag_type(std::move(bag_type)),
          traits({}), template_type(std::move(template_type))
    {
    }

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

using OopsType_ptr = std::shared_ptr<OopsType>;
using ClassType_ptr = std::shared_ptr<ClassType>;
using TraitType_ptr = std::shared_ptr<TraitType>;

// ============================================================================
// Alias
// ============================================================================

struct TypeAlias
{
    std::string name;
    Object_ptr underlying_type;
    TemplateType_ptr template_type;

    TypeAlias(
        std::string name,
        TemplateType_ptr template_type = std::make_shared<TemplateType>(),
        Object_ptr underlying_type = nullptr
    )
        : name(std::move(name)), underlying_type(std::move(underlying_type)),
          template_type(std::move(template_type))
    {
    }
};

using TypeAlias_ptr = std::shared_ptr<TypeAlias>;

// ============================================================================
// Module Type
// ============================================================================

struct ModuleType
{
    int type_id;
    std::string name;

    ObjectStringMap member_types;
    StringVector ordered_keys;

    explicit ModuleType(
        std::string name,
        ObjectStringMap member_types = {},
        StringVector ordered_keys = {}
    )
        : type_id(get_next_type_id()), name(std::move(name)),
          member_types(std::move(member_types)),
          ordered_keys(std::move(ordered_keys))
    {
    }

    int get_member_index(const std::string& member_name) const;
    Object_ptr get_member(const std::string& member_name) const;
};

using ModuleType_ptr = std::shared_ptr<ModuleType>;

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

struct NativeFunctionRuntimeObject
{
    NativeFnType function;
    std::string name;

    NativeFunctionRuntimeObject(NativeFnType function, std::string name)
        : function(std::move(function)), name(std::move(name))
    {
    }
};

using NativeFunctionRuntimeObject_ptr = std::shared_ptr<
    NativeFunctionRuntimeObject>;

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

struct OverloadsSet
{
    ObjectVector overloads;

    void add_overload(Object_ptr overload);
    Object_ptr get_overload(int overload_id) const;
};

using OverloadsSet_ptr = std::shared_ptr<OverloadsSet>;
using OverloadsSetVector = std::vector<OverloadsSet_ptr>;

struct BagObject
{
    OverloadsSetVector overloads_set_vector;
    ITablesMap itables;

    BagObject(
        OverloadsSetVector overloads_set_vector = {},
        ITablesMap itables = {}
    )
        : overloads_set_vector(std::move(overloads_set_vector)),
          itables(std::move(itables))
    {
    }

    OverloadsSet_ptr get_overloads_set(int member_id) const;
};

using BagObject_ptr = std::shared_ptr<BagObject>;

struct ClassBlueprint
{
    BagObject_ptr bag;

    ClassBlueprint() = default;

    ClassBlueprint(BagObject_ptr bag) : bag(std::move(bag))
    {
    }
};

using ClassBlueprint_ptr = std::shared_ptr<ClassBlueprint>;

struct ClassInstance
{
    RecordObject_ptr record;
    BagObject_ptr bag;

    ClassInstance() = default;

    ClassInstance(RecordObject_ptr record, BagObject_ptr bag)
        : record(std::move(record)), bag(std::move(bag))
    {
    }
};

using ClassInstance_ptr = std::shared_ptr<ClassInstance>;

struct TraitObject
{
    ClassInstance_ptr class_instance;
    std::vector<int> trait_ids;

    TraitObject(ClassInstance_ptr class_instance, std::vector<int> trait_ids)
        : class_instance(std::move(class_instance)),
          trait_ids(std::move(trait_ids))
    {
    }
};

using TraitObject_ptr = std::shared_ptr<TraitObject>;

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
        NativeFunctionRuntimeObject_ptr,
        OverloadsSet_ptr,

        ModuleObject_ptr,

        RecordObject_ptr,
        BagObject_ptr,
        TraitObject_ptr,

        ClassBlueprint_ptr,
        ClassInstance_ptr,

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

        ModuleType_ptr,

        RecordType_ptr,
        SignaturesSet_ptr,
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

    static bool are_equal_types(Object_ptr left, Object_ptr right);

    static bool are_equal_types(
        const ObjectVector& left,
        const ObjectVector& right
    );

    static bool are_equal_types_unordered(
        const ObjectVector& left,
        const ObjectVector& right
    );

    static std::string mangle_object(Object_ptr value);
    static std::string mangle_object(const ObjectVector& values);

    static std::string get_canonical_trait_name(const ObjectVector& traits);

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

template <typename T, typename... Args>
Object_ptr make_shared_object(Args&&... args)
{
    return make_object(std::make_shared<T>(std::forward<Args>(args)...));
}

} // namespace Wasp
