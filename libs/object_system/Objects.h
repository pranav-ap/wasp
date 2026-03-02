#pragma once

#include "CFGraph.h"
#include <optional>
#include <string>
#include <vector>
#include <map>
#include <utility>
#include <memory>
#include <variant>
#include <functional>

namespace Wasp
{

    struct Object;
    using Object_ptr = std::shared_ptr<Object>;
    using ObjectVector = std::vector<Object_ptr>;
    using StringVector = std::vector<std::string>;

    // Base Classes

    struct AbstractObject
    {
    };

    struct IterableAbstractObject : virtual public AbstractObject
    {
        virtual Object_ptr get_iter() = 0;
    };

    struct ScalarObject : virtual public AbstractObject
    {
    };
    struct CompositeObject : virtual public AbstractObject
    {
    };
    struct ActionObject : virtual public AbstractObject
    {
    };
    struct NoneObject : virtual public AbstractObject
    {
    };

    struct DefinitionObject : virtual public AbstractObject
    {
        std::string name;
        DefinitionObject(std::string name)
            : name(std::move(name)) {};
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
        // TODO: You will need a custom transparent comparator here instead of default std::less
        // otherwise this map compares shared_ptr memory addresses, not the object values!
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

    struct FunctionObject : public CompositeObject
    {
        CodeObject code;
        std::map<int, std::string> local_names;

        FunctionObject(CodeObject code, std::map<int, std::string> local_names = {})
            : code(std::move(code)), local_names(std::move(local_names)) {};
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

    struct AnyType : virtual public AbstractObject
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

        NamedDefinitionType(std::string name)
            : name(std::move(name)) {};
    };

    // Scalar Types
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
            : input_types(std::move(input_types)), return_type(std::make_optional(std::move(return_type))) {};
    };

    struct RecordType : public CompositeType
    {
        std::map<std::string, Object_ptr> members;
        RecordType(std::map<std::string, Object_ptr> members)
            : members(std::move(members)) {};
    };

    // Object STRUCT

    struct Object
    {
        using UnderlyingVariant = std::variant<
            std::monostate,

            NoneObject,
            IntObject, FloatObject, StringObject, BooleanObject,

            std::shared_ptr<IteratorObject>,

            std::shared_ptr<ListObject>,
            std::shared_ptr<TupleObject>,
            std::shared_ptr<SetObject>,
            std::shared_ptr<MapObject>,
            std::shared_ptr<VariantObject>,

            std::shared_ptr<FunctionObject>,

            std::shared_ptr<BreakObject>,
            std::shared_ptr<ContinueObject>,
            std::shared_ptr<RedoObject>,
            std::shared_ptr<ReturnObject>,
            std::shared_ptr<ErrorObject>,

            AnyType,
            NoneType,
            NamedDefinitionType,
            IntType, FloatType, StringType, BooleanType,
            IntLiteralType, FloatLiteralType, StringLiteralType, BooleanLiteralType,
            ListType, TupleType, SetType, MapType, VariantType, FunctionType, RecordType>;

        UnderlyingVariant value;

        Object() = default;

        template <typename T>
        Object(T &&val) : value(std::forward<T>(val)) {}

        template <typename T>
        bool is() const
        {
            return std::holds_alternative<T>(value);
        }

        template <typename T>
        const T &as() const
        {
            return std::get<T>(value);
        }

        template <typename T>
        const T *try_as() const
        {
            return std::get_if<T>(&value);
        }
    };

    // Utils

    std::string stringify_object(Object_ptr value);

    ObjectVector to_vector(std::string text);

    bool are_equal_types(Object_ptr left, Object_ptr right);
    bool are_equal_types(ObjectVector left_vector, ObjectVector right_vector);
    bool are_equal_types_unordered(ObjectVector left_vector, ObjectVector right_vector);

    Object_ptr convert_type(Object_ptr type, Object_ptr operand);
}