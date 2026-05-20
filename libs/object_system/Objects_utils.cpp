#include "Doctor.h"
#include "Objects.h"
#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <variant>

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

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

    auto left_type = unwrap_type_alias(left);
    auto right_type = unwrap_type_alias(right);

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
                return are_equal_types(l->parameter_types, r->parameter_types) &&
                       are_equal_types(l->return_type, r->return_type);
            },

            [](const ModuleType_ptr& l, const ModuleType_ptr& r)
            {
                return l->type_id == r->type_id;
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

            [](const TemplateParameterType_ptr& l, const TemplateParameterType_ptr& r)
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

// ============================================================================
// Utils
// ============================================================================

Object_ptr convert_type(Object_ptr type, Object_ptr operand)
{
    Doctor::get().fatal(WaspStage::VM, "convert_type is not implemented yet");
}

Object_ptr unwrap_type_alias(Object_ptr type)
{
    Doctor::get()
        .fatal_if_nullptr(type, WaspStage::Semantics, "Attempted to unwrap a null type pointer");

    while (type->is<TypeAlias_ptr>())
    {
        type = type->as<TypeAlias_ptr>()->underlying_type;
    }

    return type;
}

Object_ptr unwrap_completely(Object_ptr type)
{
    Doctor::get()
        .fatal_if_nullptr(type, WaspStage::Semantics, "Attempted to unwrap a null type pointer");

    while (true)
    {
        if (type->is<TypeAlias_ptr>())
        {
            type = type->as<TypeAlias_ptr>()->underlying_type;
            continue;
        }

        if (auto* generic_ptr = type->try_as<TemplateParameterType_ptr>())
        {
            type = (*generic_ptr)->constraint_type;
            continue;
        }

        break;
    }

    return type;
}

} // namespace Wasp
