#include "Objects.h"
#include "Doctor.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

std::string Object::to_string() const
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
            [](const Signature_ptr&) -> std::string
            {
                return "signature type";
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
                return "literal type: " + lit.value.get()->to_string();
            },

            // Composite Types
            [](const VariantType&) -> std::string
            {
                return "variant type";
            },

            // User Defined Types

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
            [](const GenericType_ptr& gen) -> std::string
            {
                return "generic type: " + gen.name;
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
                    res += obj->values[i]->to_string();
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
                    res += obj->values[i]->to_string();
                    if (i < obj->values.size() - 1)
                    {
                        res += ", ";
                    }
                }
                return res + ")";
            },
            [](const std::shared_ptr<SetObject>& obj) -> std::string
            {
                return "set";
            },
            [](const std::shared_ptr<MapObject>& obj) -> std::string
            {
                return "map";
            },
            [](const std::shared_ptr<VariantObject>&) -> std::string
            {
                return "<variant>";
            },

            // Callables and Modules
            [](const std::shared_ptr<FunctionBlueprintObject>& func)
                -> std::string
            {
                return "<Static Function " + func->name + ">";
            },
            [](const std::shared_ptr<FunctionRuntimeObject>& func)
                -> std::string
            {
                return "<Runtime function " + func->blueprint->name + ">";
            },
            [](const std::shared_ptr<NativeFunctionRuntimeObject>& func)
                -> std::string
            {
                return "<Native function " + func->name + ">";
            },
            [](const std::shared_ptr<ModuleObject>& mod) -> std::string
            {
                return "<module " + mod->name + ">";
            },
            // Fallback for anything missed
            [](const auto&) -> std::string
            {
                return "<Unknown Object>";
            }
        },
        this->value
    );
}

// ============================================================================
// Utils
// ============================================================================

Object_ptr Object::unwrap_type_alias()
{
    if (this->is<TypeAlias_ptr>())
    {
        return this->as<TypeAlias_ptr>()->underlying_type->unwrap_type_alias();
    }

    return this->shared_from_this();
}

Object_ptr Object::unwrap_completely()
{
    if (this->is<TypeAlias_ptr>())
    {
        return this->as<TypeAlias_ptr>()->underlying_type->unwrap_completely();
    }

    if (this->is<GenericType_ptr>())
    {
        auto& generic = this->as<GenericType_ptr>();

        if (generic->constraint_type)
        {
            return generic->constraint_type->unwrap_completely();
        }
        else
        {
            return this->shared_from_this();
        }
    }

    return this->shared_from_this();
}

bool Object::is_type_object() const
{
    return std::visit(
        overloaded{
            [](AnyType_ptr) -> bool
            {
                return true;
            },
            [](NoneType_ptr) -> bool
            {
                return true;
            },
            [](IntType_ptr) -> bool
            {
                return true;
            },
            [](FloatType_ptr) -> bool
            {
                return true;
            },
            [](StringType_ptr) -> bool
            {
                return true;
            },
            [](BooleanType_ptr) -> bool
            {
                return true;
            },
            [](LiteralType_ptr) -> bool
            {
                return true;
            },
            [](ListType_ptr) -> bool
            {
                return true;
            },
            [](SetType_ptr) -> bool
            {
                return true;
            },
            [](TupleType_ptr) -> bool
            {
                return true;
            },
            [](MapType_ptr) -> bool
            {
                return true;
            },
            [](VariantType_ptr) -> bool
            {
                return true;
            },
            [](IntersectionType_ptr) -> bool
            {
                return true;
            },
            [](GenericType_ptr) -> bool
            {
                return true;
            },
            [](EnumMemberType_ptr) -> bool
            {
                return true;
            },
            [](Signature_ptr) -> bool
            {
                return true;
            },
            [](ClassType_ptr) -> bool
            {
                return true;
            },
            [](TraitType_ptr) -> bool
            {
                return true;
            },
            [](EnumType_ptr) -> bool
            {
                return true;
            },
            [](TypeAlias_ptr) -> bool
            {
                return true;
            },
            [](const auto&) -> bool
            {
                return false;
            }
        },
        value
    );
}

bool Object::is_runtime_object() const
{
    return !is_type_object();
}

std::string Object::mangle_object(const ObjectVector& values)
{
    std::string result;

    for (const auto& value : values)
    {
        result += mangle_object(value);
    }

    return result;
}

std::string Object::mangle_object(Object_ptr value)
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
            [](const Signature_ptr&) -> std::string
            {
                return "S";
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
                return "Os" + obj.value;
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
            [](const VariantType&) -> std::string
            {
                return "v";
            },
            [](const ClassType_ptr& cls) -> std::string
            {
                return "C" + cls->name;
            },
            [](const TraitType_ptr& trt) -> std::string
            {
                return "T" + trt->name;
            },
            [](const EnumType_ptr& enum_type) -> std::string
            {
                return "E" + enum_type->name;
            },
            [](const TypeAlias_ptr& alias) -> std::string
            {
                return "a" + alias->name;
            },
            [](const GenericType_ptr& gen) -> std::string
            {
                return "G" + gen->name;
            },

            [](const auto&) -> std::string
            {
                return "U";
            }
        },
        value->value
    );
}

std::string Object::get_canonical_trait_name(const ObjectVector& traits)
{
    StringVector names;

    for (const auto& trait_obj : traits)
    {
        Doctor::get().assert(
            trait_obj->is<TraitType_ptr>(),
            WaspStage::Semantics,
            "Expected trait object in get_canonical_trait_name"
        );

        auto trait = trait_obj->as<TraitType_ptr>();
        names.push_back(trait->name);
    }

    if (names.empty())
    {
        return "";
    }

    std::sort(names.begin(), names.end());

    std::string canonical_name = names[0];
    for (size_t i = 1; i < names.size(); ++i)
    {
        canonical_name += "&" + names[i];
    }

    return canonical_name;
}

// ============================================================================
// Equality Checks
// ============================================================================

bool Object::are_equal_types(
    const ObjectVector& left_vector,
    const ObjectVector& right_vector
)
{
    if (left_vector.size() != right_vector.size())
    {
        return false;
    }

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

bool Object::are_equal_types_unordered(
    const ObjectVector& left_vector,
    const ObjectVector& right_vector
)
{
    if (left_vector.size() != right_vector.size())
    {
        return false;
    }

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

bool Object::are_equal_types(Object_ptr left, Object_ptr right)
{
    if (!left || !right)
    {
        return false;
    }

    auto left_type = left->unwrap_type_alias();
    auto right_type = right->unwrap_type_alias();

    if (left_type == right_type)
    {
        return true;
    }

    return std::visit(
        overloaded{
            [](const LiteralType& l, const LiteralType& r) -> bool
            {
                return std::visit(
                    overloaded{
                        [](const IntObject& a, const IntObject& b) -> bool
                        {
                            return a.value == b.value;
                        },
                        [](const FloatObject& a, const FloatObject& b) -> bool
                        {
                            return a.value == b.value;
                        },
                        [](const BooleanObject& a,
                           const BooleanObject& b) -> bool
                        {
                            return a.value == b.value;
                        },
                        [](const StringObject& a, const StringObject& b) -> bool
                        {
                            return a.value == b.value;
                        },
                        [](const auto&, const auto&) -> bool
                        {
                            return false;
                        }
                    },
                    l.value->value,
                    r.value->value
                );
            },

            // Collections
            [](const VariantType& l, const VariantType& r)
            {
                return are_equal_types_unordered(l.types, r.types);
            },
            [](const IntersectionType& l, const IntersectionType& r)
            {
                return are_equal_types_unordered(l.types, r.types);
            },

            [](const Signature_ptr& l, const Signature_ptr& r)
            {
                return are_equal_types(
                           l->parameter_types,
                           r->parameter_types
                       ) &&
                       are_equal_types(l->return_type, r->return_type);
            },

            [](const ClassType_ptr& l, const ClassType_ptr& r)
            {
                return l->type_id == r->type_id;
            },
            [](const TraitType_ptr& l, const TraitType_ptr& r)
            {
                return l->type_id == r->type_id;
            },

            [](const EnumType_ptr& l, const EnumType_ptr& r) -> bool
            {
                auto get_root = [](const std::string& name)
                {
                    size_t pos = name.find('.');
                    return pos == std::string::npos ? name
                                                    : name.substr(0, pos);
                };

                return get_root(l->name) == get_root(r->name);
            },

            [](const TypeAlias_ptr& l, const TypeAlias_ptr& r)
            {
                return are_equal_types(l->underlying_type, r->underlying_type);
            },

            [](const GenericType_ptr& l, const GenericType_ptr& r)
            {
                return l->name == r->name;
            },

            // Catch-all identical Primitive Types (IntType, FloatType, etc.)
            []<typename T>(const T&, const T&)
            {
                return true;
            },

            // Default - Mismatch
            [](const auto&, const auto&)
            {
                return false;
            }
        },
        left_type->value,
        right_type->value
    );
}

} // namespace Wasp
