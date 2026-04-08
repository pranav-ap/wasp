#include "ConstantPool.h"
#include "Objects.h"
#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <variant>

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)
#define VOID std::make_shared<Object>(std::make_shared<ReturnObject>())
#define THROW(message) return std::make_shared<Object>(std::make_shared<ErrorObject>(message));
#define THROW_IF(condition, message)                                                               \
    if (!(condition)) {                                                                            \
        return std::make_shared<Object>(std::make_shared<ErrorObject>(message));                   \
    }

template <class... Ts>
struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

    // ============================================================================
    // Vector Converters
    // ============================================================================

    ObjectVector to_vector(std::string text)
    {
        ObjectVector vec;
        vec.reserve(text.size());

        for (char ch : text)
        {
            vec.push_back(MAKE_OBJECT_VARIANT(StringObject(std::string(1, ch))));
        }

        return vec;
    }

    // ============================================================================
    // Equality Checks
    // ============================================================================

    bool are_equal_types(ObjectVector left_vector, ObjectVector right_vector)
    {
        return std::equal(left_vector.begin(), left_vector.end(),
                          right_vector.begin(), right_vector.end(),
                          [](Object_ptr l, Object_ptr r)
                          { return are_equal_types(l, r); });
    }

    bool are_equal_types_unordered(ObjectVector left_vector, ObjectVector right_vector)
    {
        return std::is_permutation(left_vector.begin(), left_vector.end(),
                                   right_vector.begin(), right_vector.end(),
                                   [](Object_ptr l, Object_ptr r)
                                   { return are_equal_types(l, r); });
    }

    bool are_equal_types(Object_ptr left, Object_ptr right)
    {
        if (!left || !right)
            return false;

        // Quick exit if they point to the exact same memory
        if (left == right)
            return true;

        return std::visit(
            overloaded{
                [](const IntLiteralType& l, const IntLiteralType& r)
                {
                    return l.value == r.value;
                },
                [](const FloatLiteralType& l, const FloatLiteralType& r)
                {
                    return l.value == r.value;
                },
                [](const BooleanLiteralType& l, const BooleanLiteralType& r)
                {
                    return l.value == r.value;
                },
                [](const StringLiteralType& l, const StringLiteralType& r)
                {
                    return l.value == r.value;
                },

                [](const ListType& l, const ListType& r)
                {
                    return are_equal_types(l.element_type, r.element_type);
                },
                [](const TupleType& l, const TupleType& r)
                {
                    return are_equal_types(l.element_types, r.element_types);
                },
                [](const SetType& l, const SetType& r)
                {
                    return are_equal_types(l.element_type, r.element_type);
                },
                [](const VariantType& l, const VariantType& r)
                {
                    return are_equal_types_unordered(l.types, r.types);
                },
                [](const MapType& l, const MapType& r)
                {
                    return are_equal_types(l.key_type, r.key_type) &&
                           are_equal_types(l.value_type, r.value_type);
                },

                [](const std::shared_ptr<FunctionType>& l, const std::shared_ptr<FunctionType>& r)
                {
                    return are_equal_types(l->input_types, r->input_types);
                },
                [](const NamedDefinitionType& l, const NamedDefinitionType& r)
                {
                    return l.name == r.name;
                },

                // Catch-all for identical types that don't need manual value checking
                // e.g., IntObject, FloatType
                []<typename T>(const T&, const T&)
                {
                    return true;
                },

                // Default - Types don't match
                [](const auto&, const auto&)
                {
                    return false;
                }

            },
            left->value,
            right->value
        );
    }

    // ============================================================================
    // Utils
    // ============================================================================

    Object_ptr convert_type(Object_ptr type, Object_ptr operand)
    {
        return nullptr;
    }

    std::string stringify_object(Object_ptr value)
    {
        if (!value)
            return "null";

        return std::visit(
            overloaded{
                [](const std::monostate&) -> std::string
                {
                    return "uninitialized";
                },

                // Base Types
                [](const AnyType&) -> std::string
                {
                    return "any type";
                },
                [](const NamedDefinitionType& obj) -> std::string
                {
                    return "named definition: " + obj.name;
                },
                [](const std::shared_ptr<FunctionType>&) -> std::string
                {
                    return "function type";
                },

                // Scalar Objects
                [](const NoneObject&) -> std::string
                {
                    return "none";
                },
                [](const IntObject& obj) -> std::string
                {
                    return std::to_string(obj.value);
                },
                [](const FloatObject& obj) -> std::string
                {
                    return std::to_string(obj.value);
                },
                [](const StringObject& obj) -> std::string
                {
                    return "\"" + obj.value + "\"";
                },
                [](const BooleanObject& obj) -> std::string
                {
                    return obj.value ? "true" : "false";
                },

                // Literal Types
                [](const IntLiteralType&) -> std::string
                {
                    return "int literal type";
                },
                [](const FloatLiteralType&) -> std::string
                {
                    return "float literal type";
                },
                [](const StringLiteralType&) -> std::string
                {
                    return "string literal type";
                },
                [](const BooleanLiteralType&) -> std::string
                {
                    return "boolean literal type";
                },

                // Scalar Types
                [](const IntType&) -> std::string
                {
                    return "int type";
                },
                [](const FloatType&) -> std::string
                {
                    return "float type";
                },
                [](const StringType&) -> std::string
                {
                    return "string type";
                },
                [](const BooleanType&) -> std::string
                {
                    return "bool type";
                },

                // Composite Types
                [](const ListType&) -> std::string
                {
                    return "list type";
                },
                [](const TupleType&) -> std::string
                {
                    return "tuple type";
                },
                [](const SetType&) -> std::string
                {
                    return "set type";
                },
                [](const MapType&) -> std::string
                {
                    return "map type";
                },
                [](const VariantType&) -> std::string
                {
                    return "variant type";
                },

                // Composite Objects
                [](const std::shared_ptr<IteratorObject>&) -> std::string
                {
                    return "<iterator>";
                },

                [](const std::shared_ptr<ListObject>& obj) -> std::string
                {
                    std::string res = "[";
                    for (size_t i = 0; i < obj->values.size(); ++i)
                    {
                        res += stringify_object(obj->values[i]);
                        if (i < obj->values.size() - 1)
                            res += ", ";
                    }
                    return res + "]";
                },

                [](const std::shared_ptr<TupleObject>& obj) -> std::string
                {
                    std::string res = "(";
                    for (size_t i = 0; i < obj->values.size(); ++i)
                    {
                        res += stringify_object(obj->values[i]);
                        if (i < obj->values.size() - 1)
                            res += ", ";
                    }
                    return res + ")";
                },

                [](const std::shared_ptr<SetObject>& obj) -> std::string
                {
                    std::string res = "{";
                    auto it = obj->values.begin();
                    while (it != obj->values.end())
                    {
                        res += stringify_object(*it);
                        if (++it != obj->values.end())
                            res += ", ";
                    }
                    return res + "}";
                },

                [](const std::shared_ptr<MapObject>& obj) -> std::string
                {
                    std::string res = "{";
                    auto it = obj->pairs.begin();
                    while (it != obj->pairs.end())
                    {
                        res += stringify_object(it->first) + ": " + stringify_object(it->second);
                        if (++it != obj->pairs.end())
                            res += ", ";
                    }
                    return res + "}";
                },

                [](const std::shared_ptr<VariantObject>&) -> std::string
                {
                    return "<variant>";
                },

                [](const std::shared_ptr<FunctionBlueprintObject>& func) -> std::string
                {
                    return "<Static Function " + func->name + ">";
                },

                [](const std::shared_ptr<FunctionRuntimeObject>& func) -> std::string
                {
                    return "<Runtime function " + func->blueprint->name + ">";
                },

                [](const std::shared_ptr<NativeFunctionObject>& func) -> std::string
                {
                    return "<Native function " + func->name + ">";
                },

                [](const std::shared_ptr<ModuleObject>& mod) -> std::string
                {
                    return "<module " + mod->name + ">";
                },

                // Action Objects
                [](const std::shared_ptr<ReturnObject>&) -> std::string
                {
                    return "return";
                },
                [](const std::shared_ptr<ErrorObject>& obj) -> std::string
                {
                    return "error: " + obj->message;
                },
                [](const std::shared_ptr<RedoObject>&) -> std::string
                {
                    return "redo";
                },
                [](const std::shared_ptr<BreakObject>&) -> std::string
                {
                    return "break";
                },
                [](const std::shared_ptr<ContinueObject>&) -> std::string
                {
                    return "continue";
                },

                // Fallback for anything missed
                [](const auto&) -> std::string
                {
                    return "<Unknown Object>";
                }

            },
            value->value
        );
    }
}
