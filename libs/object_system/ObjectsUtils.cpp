#include "Doctor.h"
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
    if (!(condition))                                                                              \
    {                                                                                              \
        return std::make_shared<Object>(std::make_shared<ErrorObject>(message));                   \
    }

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

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
    if (left_vector.size() != right_vector.size())
        return false;

    return std::equal(
        left_vector.begin(),
        left_vector.end(),
        right_vector.begin(),
        right_vector.end(),
        [](Object_ptr l, Object_ptr r)
        {
            return are_equal_types(l, r);
        }
    );
}

bool are_equal_types_unordered(ObjectVector left_vector, ObjectVector right_vector)
{
    if (left_vector.size() != right_vector.size())
        return false;

    return std::is_permutation(
        left_vector.begin(),
        left_vector.end(),
        right_vector.begin(),
        right_vector.end(),
        [](Object_ptr l, Object_ptr r)
        {
            return are_equal_types(l, r);
        }
    );
}

bool are_equal_types(Object_ptr left, Object_ptr right)
{
    if (!left || !right)
    {
        return false;
    }

    if (left == right)
    {
        return true;
    }

    return std::visit(
        overloaded{
            [](const LiteralType& l, const LiteralType& r)
            {
                return std::visit(
                    overloaded{
                        [](const IntObject& v1, const IntObject& v2)
                        {
                            return v1.value == v2.value;
                        },
                        [](const FloatObject& v1, const FloatObject& v2)
                        {
                            return v1.value == v2.value;
                        },
                        [](const BooleanObject& v1, const BooleanObject& v2)
                        {
                            return v1.value == v2.value;
                        },
                        [](const StringObject& v1, const StringObject& v2)
                        {
                            return v1.value == v2.value;
                        },
                        // Mismatched literal types
                        [](const auto&, const auto&)
                        {
                            return false;
                        }
                    },
                    l.value->value,
                    r.value->value
                );
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

            [](const Signature_ptr& l, const Signature_ptr& r)
            {
                return are_equal_types(
                           l->parameter_types,
                           r->parameter_types
                       ) &&
                       are_equal_types(l->return_type, r->return_type);
            },

            [](const NamedDefinitionType& l, const NamedDefinitionType& r)
            {
                return l.name == r.name;
            },
            [](const ModuleType_ptr& l, const ModuleType_ptr& r)
            {
                return l->name == r->name;
            },
            [](const ClassType_ptr& l, const ClassType_ptr& r)
            {
                return l->name == r->name;
            },
            [](const TraitType_ptr& l, const TraitType_ptr& r)
            {
                return l->name == r->name;
            },
            [](const EnumType_ptr& l, const EnumType_ptr& r)
            {
                return l->name == r->name;
            },
            [](const TypeAlias_ptr& l, const TypeAlias_ptr& r)
            {
                return l->name == r->name;
            },
            [](const TemplateParameterType_ptr& l,
               const TemplateParameterType_ptr& r)
            {
                return l->name == r->name;
            },

            // Catch-all for identical types that don't need manual value
            // checking
            // e.g., IntType, FloatType, AnyType
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
    Doctor::get().fatal(WaspStage::VM, "convert_type is not implemented yet");
}

std::string stringify_object(Object_ptr value)
{
    Doctor::get().fatal_if_nullptr(
        value,
        WaspStage::VM,
        "Attempted to stringify a null object pointer"
    );

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
            [](const Signature_ptr&) -> std::string
            {
                return "signature type";
            },
            [](const NoneType&) -> std::string
            {
                return "none type";
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

            // Unified Literal Type
            [](const LiteralType& lit) -> std::string
            {
                return "literal type: " + stringify_object(lit.value);
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

            // User Defined Types
            [](const ModuleType_ptr& mod) -> std::string
            {
                return "module type: " + mod->name;
            },
            [](const ClassType_ptr& cls) -> std::string
            {
                return "class type: " + cls->name;
            },
            [](const TraitType_ptr& trt) -> std::string
            {
                return "trait type: " + trt->name;
            },
            [](const EnumType_ptr& enum_type) -> std::string
            {
                return "enum type: " + enum_type->name;
            },
            [](const TypeAlias_ptr& alias) -> std::string
            {
                return "type alias: " + alias->name;
            },
            [](const TemplateParameterType_ptr& gen) -> std::string
            {
                return "generic type: " + gen->name;
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
                    {
                        res += ", ";
                    }
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
                    {
                        res += ", ";
                    }
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
                    {
                        res += ", ";
                    }
                }
                return res + "}";
            },
            [](const std::shared_ptr<MapObject>& obj) -> std::string
            {
                std::string res = "{";
                auto it = obj->pairs.begin();
                while (it != obj->pairs.end())
                {
                    res += stringify_object(it->first) + ": " +
                           stringify_object(it->second);
                    if (++it != obj->pairs.end())
                    {
                        res += ", ";
                    }
                }
                return res + "}";
            },
            [](const std::shared_ptr<VariantObject>&) -> std::string
            {
                return "<variant>";
            },

            // Callables and Modules
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
            [](const std::shared_ptr<ClassBlueprintObject>&) -> std::string
            {
                return "<class blueprint>";
            },

            // Overload Groups
            [](const std::shared_ptr<ObjectOverloadList>&) -> std::string
            {
                return "<overloaded objects>";
            },

            // Instances
            [](const std::shared_ptr<InstanceObject>&) -> std::string
            {
                return "<instance object>";
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

std::string mangle_object(const ObjectVector& values)
{
    std::string result;

    for (const auto& value : values)
    {
        result += mangle_object(value);
    }

    return result;
}

std::string mangle_object(Object_ptr value)
{
    Doctor::get().fatal_if_nullptr(
        value,
        WaspStage::VM,
        "Attempted to mangle a null object pointer"
    );

    return std::visit(
        overloaded{
            [](const std::monostate&) -> std::string
            {
                return "_";
            },

            [](const AnyType&) -> std::string
            {
                return "A";
            },
            [](const NamedDefinitionType& obj) -> std::string
            {
                return "D" + std::to_string(obj.name.length()) + obj.name;
            },
            [](const Signature_ptr&) -> std::string
            {
                return "S";
            },
            [](const NoneType&) -> std::string
            {
                return "N";
            },

            [](const NoneObject&) -> std::string
            {
                return "On";
            },
            [](const IntObject& obj) -> std::string
            {
                return "Oi" + std::to_string(obj.value);
            },
            [](const FloatObject& obj) -> std::string
            {
                return "Of" + std::to_string(obj.value);
            },
            [](const StringObject& obj) -> std::string
            {
                return "Os" + std::to_string(obj.value.length()) + obj.value;
            },
            [](const BooleanObject& obj) -> std::string
            {
                return obj.value ? "Ob1" : "Ob0";
            },

            [](const LiteralType& lit) -> std::string
            {
                return std::visit(
                    overloaded{
                        [](const IntObject&) -> std::string
                        {
                            return "li";
                        },
                        [](const FloatObject&) -> std::string
                        {
                            return "lf";
                        },
                        [](const StringObject&) -> std::string
                        {
                            return "ls";
                        },
                        [](const BooleanObject&) -> std::string
                        {
                            return "lb";
                        },
                        [](const auto&) -> std::string
                        {
                            return "lu";
                        } // Unknown literal
                    },
                    lit.value->value
                );
            },

            [](const IntType&) -> std::string
            {
                return "i";
            },
            [](const FloatType&) -> std::string
            {
                return "f";
            },
            [](const StringType&) -> std::string
            {
                return "s";
            },
            [](const BooleanType&) -> std::string
            {
                return "b";
            },

            [](const ListType&) -> std::string
            {
                return "l";
            },
            [](const TupleType&) -> std::string
            {
                return "t";
            },
            [](const SetType&) -> std::string
            {
                return "e";
            },
            [](const MapType&) -> std::string
            {
                return "m";
            },
            [](const VariantType&) -> std::string
            {
                return "v";
            },

            [](const ModuleType_ptr& mod) -> std::string
            {
                return "M" + std::to_string(mod->name.length()) + mod->name;
            },
            [](const ClassType_ptr& cls) -> std::string
            {
                return "C" + std::to_string(cls->name.length()) + cls->name;
            },
            [](const TraitType_ptr& trt) -> std::string
            {
                return "T" + std::to_string(trt->name.length()) + trt->name;
            },
            [](const EnumType_ptr& enum_type) -> std::string
            {
                return "E" + std::to_string(enum_type->name.length()) +
                       enum_type->name;
            },
            [](const TypeAlias_ptr& alias) -> std::string
            {
                return "a" + std::to_string(alias->name.length()) + alias->name;
            },
            [](const TemplateParameterType_ptr& gen) -> std::string
            {
                return "G" + std::to_string(gen->name.length()) + gen->name;
            },

            [](const auto&) -> std::string
            {
                return "U";
            }
        },
        value->value
    );
}

Object_ptr unwrap_type_alias(Object_ptr type)
{
    Doctor::get().fatal_if_nullptr(
        type,
        WaspStage::Semantics,
        "Attempted to unwrap a null type pointer"
    );

    while (type->is<TypeAlias_ptr>())
    {
        type = type->as<TypeAlias_ptr>()->underlying_type;
    }

    return type;
}

} // namespace Wasp
