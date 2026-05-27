#include "Doctor.h"
#include "Objects.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

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
            [](const GenericType& gen) -> std::string
            {
                return "G" + gen.name;
            },

            [](const auto&) -> std::string
            {
                return "U";
            }
        },
        value->value
    );
}

std::string get_canonical_trait_name(const ObjectVector& traits)
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

bool are_equal_types(ObjectVector left_vector, ObjectVector right_vector)
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

bool are_equal_types_unordered(ObjectVector left_vector, ObjectVector right_vector)
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

bool are_equal_types(Object_ptr left, Object_ptr right)
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
                        [](const BooleanObject& a, const BooleanObject& b) -> bool
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
                return are_equal_types(l->parameter_types, r->parameter_types) &&
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
                    return pos == std::string::npos ? name : name.substr(0, pos);
                };

                return get_root(l->name) == get_root(r->name);
            },

            [](const TypeAlias_ptr& l, const TypeAlias_ptr& r)
            {
                return are_equal_types(l->underlying_type, r->underlying_type);
            },

            [](const GenericType& l, const GenericType& r)
            {
                return l.name == r.name;
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
