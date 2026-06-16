#include "Expression.h"
#include "SymbolScope.h"
#include "Type.h"
#include "TypeSystem.h"

#include <algorithm>
#include <cstddef>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

bool TypeSystem::assignable(
    SymbolScope_ptr scope,
    const TypeVector& type_vector_1,
    const TypeVector& type_vector_2
) const
{
    if (type_vector_1.size() != type_vector_2.size())
    {
        return false;
    }

    return std::equal(
        type_vector_1.begin(),
        type_vector_1.end(),
        type_vector_2.begin(),
        [&](const auto& a, const auto& b)
        {
            return assignable(scope, a, b);
        }
    );
}

bool TypeSystem::assignable(
    SymbolScope_ptr scope,
    const Type_ptr lhs_type,
    const Type_ptr rhs_type
) const
{
    if (!lhs_type || !rhs_type)
    {
        return false;
    }

    Type_ptr lhs = lhs_type->unwrap_alias();
    Type_ptr rhs = rhs_type->unwrap_alias();

    if (equal(scope, lhs, rhs))
    {
        return true;
    }

    if (lhs->is<GenericType_ptr>())
    {
        return assignable(
            scope,
            lhs->as<GenericType_ptr>()->constraint_type,
            rhs
        );
    }

    if (rhs->is<GenericType_ptr>())
    {
        return assignable(
            scope,
            lhs,
            rhs->as<GenericType_ptr>()->constraint_type
        );
    }

    if (lhs->is<IntersectionType_ptr>())
    {
        auto l = lhs->as<IntersectionType_ptr>();

        return std::all_of(
            l->types.begin(),
            l->types.end(),
            [&](Type_ptr t)
            {
                return assignable(scope, t, rhs);
            }
        );
    }

    if (rhs->is<IntersectionType_ptr>())
    {
        auto r = rhs->as<IntersectionType_ptr>();

        return std::all_of(
            r->types.begin(),
            r->types.end(),
            [&](Type_ptr t)
            {
                return assignable(scope, lhs, t);
            }
        );
    }

    if (lhs->is<VariantType_ptr>())
    {
        auto l = lhs->as<VariantType_ptr>();

        return std::any_of(
            l->types.begin(),
            l->types.end(),
            [&](Type_ptr t)
            {
                return assignable(scope, t, rhs);
            }
        );
    }

    if (rhs->is<VariantType_ptr>())
    {
        auto r = rhs->as<VariantType_ptr>();

        return std::all_of(
            r->types.begin(),
            r->types.end(),
            [&](Type_ptr t)
            {
                return assignable(scope, lhs, t);
            }
        );
    }

    return std::visit(
        overloaded{
            [](AnyType_ptr, const auto&) -> bool
            {
                return true;
            },

            // Literals

            [&](LiteralType_ptr l, LiteralType_ptr r) -> bool
            {
                if (!l || !r)
                {
                    return false;
                }

                // Compare the literal expressions
                auto l_expr = l->value;
                auto r_expr = r->value;

                if (!l_expr || !r_expr)
                {
                    return false;
                }

                if (l_expr->is<IntegerLiteral>() &&
                    r_expr->is<IntegerLiteral>())
                {
                    return l_expr->as<IntegerLiteral>().value ==
                           r_expr->as<IntegerLiteral>().value;
                }

                if (l_expr->is<FloatLiteral>() &&
                    r_expr->is<FloatLiteral>())
                {
                    return l_expr->as<FloatLiteral>().value ==
                           r_expr->as<FloatLiteral>().value;
                }

                if (l_expr->is<StringLiteral>() &&
                    r_expr->is<StringLiteral>())
                {
                    return l_expr->as<StringLiteral>().value ==
                           r_expr->as<StringLiteral>().value;
                }

                if (l_expr->is<BooleanLiteral>() &&
                    r_expr->is<BooleanLiteral>())
                {
                    return l_expr->as<BooleanLiteral>().value ==
                           r_expr->as<BooleanLiteral>().value;
                }

                if (l_expr->is<NoneLiteral>() && r_expr->is<NoneLiteral>())
                {
                    return true;
                }

                return false;
            },

            // Primitives

            [&](PrimitiveType_ptr l, IntType_ptr) -> bool
            {
                return l->name == "int";
            },
            [&](IntType_ptr, PrimitiveType_ptr r) -> bool
            {
                return r->name == "int";
            },

            [&](PrimitiveType_ptr l, FloatType_ptr) -> bool
            {
                return l->name == "float";
            },
            [&](FloatType_ptr, PrimitiveType_ptr r) -> bool
            {
                return r->name == "float";
            },

            [&](PrimitiveType_ptr l, StringType_ptr) -> bool
            {
                return l->name == "str";
            },
            [&](StringType_ptr, PrimitiveType_ptr r) -> bool
            {
                return r->name == "str";
            },

            [&](PrimitiveType_ptr l, BooleanType_ptr) -> bool
            {
                return l->name == "bool";
            },
            [&](BooleanType_ptr, PrimitiveType_ptr r) -> bool
            {
                return r->name == "bool";
            },

            [&](MapType_ptr l, MapType_ptr r) -> bool
            {
                return assignable(scope, l->key_type, r->key_type) &&
                       assignable(scope, r->value_type, l->value_type);
            },

            [&](SetType_ptr l, SetType_ptr r) -> bool
            {
                return assignable(scope, l->element_type, r->element_type);
            },

            [&](ListType_ptr l, ListType_ptr r) -> bool
            {
                return assignable(scope, l->element_type, r->element_type);
            },

            [&](TupleType_ptr l, TupleType_ptr r) -> bool
            {
                if (l->element_types.size() != r->element_types.size())
                {
                    return false;
                }

                for (size_t i = 0; i < l->element_types.size(); i++)
                {
                    bool is_assignable = assignable(
                        scope,
                        l->element_types[i],
                        r->element_types[i]
                    );

                    if (!is_assignable)
                    {
                        return false;
                    }
                }
                return true;
            },

            [&](TraitType_ptr lhs_trait, ClassType_ptr r) -> bool
            {
                int target_id = lhs_trait->type_id;

                for (const auto& trait_obj : r->traits)
                {
                    if (auto trait_def = trait_obj->as<TraitType_ptr>())
                    {
                        if (trait_def->type_id == target_id)
                        {
                            return true;
                        }
                    }
                }
                return false;
            },

            // Primitives

            [&](PrimitiveType_ptr l, ListType_ptr r) -> bool
            {
                if (l->name != "list")
                {
                    return false;
                }

                if (l->template_type->empty())
                {
                    return false;
                }

                Type_ptr generic = l->template_type->get_generic_type(0);

                return assignable(scope, generic, r->element_type);
            },

            [&](ListType_ptr l, PrimitiveType_ptr r) -> bool
            {
                if (r->name != "list")
                {
                    return false;
                }

                if (r->template_type->empty())
                {
                    return false;
                }

                Type_ptr generic = r->template_type->get_generic_type(0);

                return assignable(scope, l->element_type, generic);
            },

            [&](TupleType_ptr l, PrimitiveType_ptr r) -> bool
            {
                if (r->name != "tuple")
                {
                    return false;
                }

                if (r->template_type->empty())
                {
                    return false;
                }

                Type_ptr generic = r->template_type->get_generic_type(0);

                if (generic->is<GenericType_ptr>())
                {
                    generic = generic->as<GenericType_ptr>()
                                  ->constraint_type;
                }

                if (!generic->is<TupleType_ptr>())
                {
                    return false;
                }

                auto rhs_tuple = generic->as<TupleType_ptr>();

                if (l->element_types.size() !=
                    rhs_tuple->element_types.size())
                {
                    return false;
                }

                for (size_t i = 0; i < l->element_types.size(); i++)
                {
                    if (!assignable(
                            scope,
                            l->element_types[i],
                            rhs_tuple->element_types[i]
                        ))
                    {
                        return false;
                    }
                }
                return true;
            },

            [&](PrimitiveType_ptr l, TupleType_ptr rhs_tuple) -> bool
            {
                if (l->name != "tuple")
                {
                    return false;
                }

                if (l->template_type->ordered_parameter_names.empty())
                {
                    return false;
                }

                const auto& param_name = l->template_type
                                             ->ordered_parameter_names[0];
                auto it = l->template_type->template_parameters.find(
                    param_name
                );

                if (it == l->template_type->template_parameters.end())
                {
                    return false;
                }

                auto lhs_element_types = it->second;

                // Resolve generic to its constraint
                if (lhs_element_types->is<GenericType_ptr>())
                {
                    lhs_element_types = lhs_element_types
                                            ->as<GenericType_ptr>()
                                            ->constraint_type;
                }

                if (!lhs_element_types->is<TupleType_ptr>())
                {
                    return false;
                }

                auto lhs_tuple = lhs_element_types->as<TupleType_ptr>();

                if (lhs_tuple->element_types.size() !=
                    rhs_tuple->element_types.size())
                {
                    return false;
                }

                for (size_t i = 0; i < lhs_tuple->element_types.size();
                     i++)
                {
                    if (!assignable(
                            scope,
                            lhs_tuple->element_types[i],
                            rhs_tuple->element_types[i]
                        ))
                    {
                        return false;
                    }
                }
                return true;
            },

            // Set type to set class
            [&](PrimitiveType_ptr l, SetType_ptr rhs_set) -> bool
            {
                if (l->name != "set")
                {
                    return false;
                }
                if (l->template_type->ordered_parameter_names.empty())
                {
                    return false;
                }
                const auto& param_name = l->template_type
                                             ->ordered_parameter_names[0];
                auto it = l->template_type->template_parameters.find(
                    param_name
                );
                if (it == l->template_type->template_parameters.end())
                {
                    return false;
                }
                auto lhs_element_type = it->second;
                return assignable(
                    scope,
                    lhs_element_type,
                    rhs_set->element_type
                );
            },

            [&](SetType_ptr lhs_set, PrimitiveType_ptr r) -> bool
            {
                if (r->name != "set")
                {
                    return false;
                }
                if (r->template_type->ordered_parameter_names.empty())
                {
                    return false;
                }
                const auto& param_name = r->template_type
                                             ->ordered_parameter_names[0];
                auto it = r->template_type->template_parameters.find(
                    param_name
                );
                if (it == r->template_type->template_parameters.end())
                {
                    return false;
                }
                auto rhs_element_type = it->second;
                return assignable(
                    scope,
                    lhs_set->element_type,
                    rhs_element_type
                );
            },

            [&](PrimitiveType_ptr l, MapType_ptr rhs_map) -> bool
            {
                if (l->name != "map")
                {
                    return false;
                }

                if (l->template_type->ordered_parameter_names.size() < 2)
                {
                    return false;
                }

                // Get key type
                const auto&
                    key_param_name = l->template_type
                                         ->ordered_parameter_names[0];
                auto key_it = l->template_type->template_parameters.find(
                    key_param_name
                );
                if (key_it == l->template_type->template_parameters.end())
                {
                    return false;
                }

                // Get value type
                const auto&
                    value_param_name = l->template_type
                                           ->ordered_parameter_names[1];
                auto value_it = l->template_type->template_parameters.find(
                    value_param_name
                );
                if (value_it ==
                    l->template_type->template_parameters.end())
                {
                    return false;
                }

                return assignable(
                           scope,
                           key_it->second,
                           rhs_map->key_type
                       ) &&
                       assignable(
                           scope,
                           value_it->second,
                           rhs_map->value_type
                       );
            },

            [&](MapType_ptr l, PrimitiveType_ptr r) -> bool
            {
                if (r->name != "map")
                {
                    return false;
                }

                if (r->template_type->ordered_parameter_names.size() < 2)
                {
                    return false;
                }

                // Get key type
                const auto&
                    key_param_name = r->template_type
                                         ->ordered_parameter_names[0];
                auto key_it = r->template_type->template_parameters.find(
                    key_param_name
                );
                if (key_it == r->template_type->template_parameters.end())
                {
                    return false;
                }

                // Get value type
                const auto&
                    value_param_name = r->template_type
                                           ->ordered_parameter_names[1];
                auto value_it = r->template_type->template_parameters.find(
                    value_param_name
                );
                if (value_it ==
                    r->template_type->template_parameters.end())
                {
                    return false;
                }

                return assignable(scope, l->key_type, key_it->second) &&
                       assignable(scope, l->value_type, value_it->second);
            },

            [](const auto&, const auto&) -> bool
            {
                return false;
            }
        },
        lhs->data,
        rhs->data
    );
}

} // namespace Wasp
