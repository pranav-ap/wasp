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
        this,
        WaspStage::VM,
        "Attempted to stringify a null object pointer"
    );

    return std::visit(
        overloaded{
            [](std::monostate) -> std::string
            {
                return "uninitialized";
            },

            // Base Types
            [](AnyType_ptr) -> std::string
            {
                return "any type";
            },
            [](NoneType_ptr) -> std::string
            {
                return "none type";
            },
            [](Signature_ptr) -> std::string
            {
                return "signature type";
            },

            // Scalar Types
            [](IntType_ptr) -> std::string
            {
                return "int";
            },
            [](FloatType_ptr) -> std::string
            {
                return "float";
            },
            [](StringType_ptr) -> std::string
            {
                return "str";
            },
            [](BooleanType_ptr) -> std::string
            {
                return "bool";
            },

            // Scalar Objects
            [](NoneObject_ptr) -> std::string
            {
                return "none";
            },
            [](IntObject_ptr obj) -> std::string
            {
                return std::to_string(obj->value);
            },
            [](FloatObject_ptr obj) -> std::string
            {
                return std::to_string(obj->value);
            },
            [](StringObject_ptr obj) -> std::string
            {
                return "\"" + obj->value + "\"";
            },
            [](BooleanObject_ptr obj) -> std::string
            {
                return obj->value ? "true" : "false";
            },

            // Unified Literal Type
            [](LiteralType_ptr lit) -> std::string
            {
                return "literal type: " + lit->value->to_string();
            },

            // Composite Types
            [](ListType_ptr) -> std::string
            {
                return "list type";
            },
            [](SetType_ptr) -> std::string
            {
                return "set type";
            },
            [](TupleType_ptr) -> std::string
            {
                return "tuple type";
            },
            [](MapType_ptr) -> std::string
            {
                return "map type";
            },
            [](VariantType_ptr) -> std::string
            {
                return "variant type";
            },
            [](IntersectionType_ptr) -> std::string
            {
                return "intersection type";
            },
            [](GenericType_ptr gen) -> std::string
            {
                return "generic type: " + gen->name;
            },
            [](EnumMemberType_ptr) -> std::string
            {
                return "enum member";
            },

            // User Defined Types
            [](ClassType_ptr cls) -> std::string
            {
                return "class type: " + cls->name;
            },
            [](TraitType_ptr trt) -> std::string
            {
                return "trait type: " + trt->name;
            },
            [](EnumType_ptr enum_type) -> std::string
            {
                return "enum type: " + enum_type->name;
            },
            [](TypeAlias_ptr alias) -> std::string
            {
                return "type alias: " + alias->name;
            },

            // Composite Objects
            [](IteratorObject_ptr) -> std::string
            {
                return "<iterator>";
            },
            [](ListObject_ptr obj) -> std::string
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
            [](TupleObject_ptr obj) -> std::string
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
            [](SetObject_ptr) -> std::string
            {
                return "set";
            },
            [](MapObject_ptr) -> std::string
            {
                return "map";
            },
            [](VariantObject_ptr) -> std::string
            {
                return "<variant>";
            },
            [](RecordObject_ptr) -> std::string
            {
                return "<record>";
            },
            [](BagObject_ptr) -> std::string
            {
                return "<bag>";
            },
            [](TraitObject_ptr) -> std::string
            {
                return "<trait object>";
            },

            // Callables and Modules
            [](FunctionBlueprintObject_ptr func) -> std::string
            {
                return "<Static Function " + func->name + ">";
            },
            [](FunctionRuntimeObject_ptr func) -> std::string
            {
                return "<Runtime function " + func->blueprint->name + ">";
            },
            [](NativeFunctionRuntimeObject_ptr func) -> std::string
            {
                return "<Native function " + func->name + ">";
            },
            [](OverloadsSet_ptr) -> std::string
            {
                return "<overload set>";
            },
            [](ModuleObject_ptr mod) -> std::string
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
        auto alias = this->as<TypeAlias_ptr>();
        if (alias->underlying_type)
        {
            return alias->underlying_type->unwrap_type_alias();
        }
    }

    return this->shared_from_this();
}

Object_ptr Object::unwrap_completely()
{
    if (this->is<TypeAlias_ptr>())
    {
        auto alias = this->as<TypeAlias_ptr>();
        if (alias->underlying_type)
        {
            return alias->underlying_type->unwrap_completely();
        }
    }

    if (this->is<GenericType_ptr>())
    {
        auto generic = this->as<GenericType_ptr>();
        if (generic->constraint_type)
        {
            return generic->constraint_type->unwrap_completely();
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
            [](std::monostate) -> std::string
            {
                return "_";
            },

            // Types
            [](AnyType_ptr) -> std::string
            {
                return "A";
            },
            [](NoneType_ptr) -> std::string
            {
                return "N";
            },
            [](IntType_ptr) -> std::string
            {
                return "i";
            },
            [](FloatType_ptr) -> std::string
            {
                return "f";
            },
            [](StringType_ptr) -> std::string
            {
                return "s";
            },
            [](BooleanType_ptr) -> std::string
            {
                return "b";
            },
            [](ListType_ptr) -> std::string
            {
                return "l";
            },
            [](SetType_ptr) -> std::string
            {
                return "st";
            },
            [](TupleType_ptr) -> std::string
            {
                return "t";
            },
            [](MapType_ptr) -> std::string
            {
                return "m";
            },
            [](VariantType_ptr) -> std::string
            {
                return "v";
            },
            [](IntersectionType_ptr) -> std::string
            {
                return "i";
            },
            [](Signature_ptr) -> std::string
            {
                return "sig";
            },

            // Scalar Objects
            [](NoneObject_ptr) -> std::string
            {
                return "On";
            },
            [](IntObject_ptr obj) -> std::string
            {
                return "Oi" + std::to_string(obj->value);
            },
            [](FloatObject_ptr obj) -> std::string
            {
                return "Of" + std::to_string(obj->value);
            },
            [](StringObject_ptr obj) -> std::string
            {
                return "Os" + obj->value;
            },
            [](BooleanObject_ptr obj) -> std::string
            {
                return obj->value ? "Ob1" : "Ob0";
            },

            // Literal Type
            [](LiteralType_ptr lit) -> std::string
            {
                return std::visit(
                    overloaded{
                        [](IntObject_ptr) -> std::string
                        {
                            return "li";
                        },
                        [](FloatObject_ptr) -> std::string
                        {
                            return "lf";
                        },
                        [](StringObject_ptr) -> std::string
                        {
                            return "ls";
                        },
                        [](BooleanObject_ptr) -> std::string
                        {
                            return "lb";
                        },
                        [](const auto&) -> std::string
                        {
                            return "lu";
                        }
                    },
                    lit->value->value
                );
            },

            // OOP Types
            [](ClassType_ptr cls) -> std::string
            {
                return "C" + cls->name;
            },
            [](TraitType_ptr trt) -> std::string
            {
                return "T" + trt->name;
            },
            [](EnumType_ptr enum_type) -> std::string
            {
                return "E" + enum_type->name;
            },
            [](TypeAlias_ptr alias) -> std::string
            {
                return "a" + alias->name;
            },
            [](GenericType_ptr gen) -> std::string
            {
                return "G" + gen->name;
            },
            [](EnumMemberType_ptr) -> std::string
            {
                return "Em";
            },

            // Fallback
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
            [](LiteralType_ptr l, LiteralType_ptr r) -> bool
            {
                return std::visit(
                    overloaded{
                        [](IntObject_ptr a, IntObject_ptr b) -> bool
                        {
                            return a->value == b->value;
                        },
                        [](FloatObject_ptr a, FloatObject_ptr b) -> bool
                        {
                            return a->value == b->value;
                        },
                        [](BooleanObject_ptr a, BooleanObject_ptr b) -> bool
                        {
                            return a->value == b->value;
                        },
                        [](StringObject_ptr a, StringObject_ptr b) -> bool
                        {
                            return a->value == b->value;
                        },
                        [](const auto&, const auto&) -> bool
                        {
                            return false;
                        }
                    },
                    l->value->value,
                    r->value->value
                );
            },

            [](VariantType_ptr l, VariantType_ptr r)
            {
                return are_equal_types_unordered(l->types, r->types);
            },
            [](IntersectionType_ptr l, IntersectionType_ptr r)
            {
                return are_equal_types_unordered(l->types, r->types);
            },

            [](Signature_ptr l, Signature_ptr r)
            {
                return are_equal_types(
                           l->parameter_types,
                           r->parameter_types
                       ) &&
                       are_equal_types(l->return_type, r->return_type);
            },

            [](ClassType_ptr l, ClassType_ptr r)
            {
                return l->type_id == r->type_id;
            },
            [](TraitType_ptr l, TraitType_ptr r)
            {
                return l->type_id == r->type_id;
            },

            [](EnumType_ptr l, EnumType_ptr r) -> bool
            {
                auto get_root = [](const std::string& name)
                {
                    size_t pos = name.find('.');
                    return pos == std::string::npos ? name
                                                    : name.substr(0, pos);
                };
                return get_root(l->name) == get_root(r->name);
            },

            [](TypeAlias_ptr l, TypeAlias_ptr r)
            {
                return are_equal_types(l->underlying_type, r->underlying_type);
            },

            [](GenericType_ptr l, GenericType_ptr r)
            {
                return l->name == r->name;
            },

            // Catch-all identical Primitive Types
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
