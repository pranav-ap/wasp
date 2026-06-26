#include "Solidifier.h"
#include "AST.h"
#include "ASTCloner.h"
#include "Doctor.h"
#include "Statement.h"
#include "Type.h"
#include "TypeNode.h"
#include "TypeSystem.h"

#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

namespace
{

TypeVector extract_types(const std::map<std::string, Type_ptr>& subs)
{
    TypeVector result;

    for (auto& [_, type] : subs)
    {
        result.push_back(type);
    }

    return result;
}

std::string get_solid_name(const std::string& base_name, const TypeVector& type_arguments)
{
    if (type_arguments.empty())
    {
        return base_name;
    }

    return base_name + "_" + TypeSystem::mangle(type_arguments);
}

} // namespace

// ============================================================================
// Public API
// ============================================================================

Statement_ptr Solidifier::visit(
    FunctionDefinition& func,
    const std::map<std::string, Type_ptr>& substitution_map
)
{
    auto typenode_map = make_typenode_substitutions(substitution_map);
    Statement_ptr stmt = visit(func, typenode_map);

    if (!substitution_map.empty())
    {
        std::string name = get_solid_name(func.name, extract_types(substitution_map));
    }

    return stmt;
}

Statement_ptr Solidifier::visit(Statement_ptr& stmt, const std::map<std::string, Type_ptr>& substitution_map)
{
    auto typenode_map = make_typenode_substitutions(substitution_map);
    return visit(stmt, typenode_map);
}

// ============================================================================
// Private
// ============================================================================

Statement_ptr Solidifier::visit(Statement_ptr& stmt, const std::map<std::string, TypeNode_ptr>& typenode_map)
{
    Doctor::semantics().fatal_if_nullptr(stmt, "Attempted to visit a null Statement");

    return std::visit(
        overloaded{
            [&](auto&& node) -> Statement_ptr
            {
                if constexpr (requires { visit(node, typenode_map); })
                {
                    return visit(node, typenode_map);
                }

                return stmt;
            }
        },
        stmt->data
    );
}

StatementVector Solidifier::visit(
    StatementVector& statements,
    const std::map<std::string, TypeNode_ptr>& typenode_map
)
{
    StatementVector result;
    result.reserve(statements.size());

    for (auto& stmt : statements)
    {
        result.push_back(visit(stmt, typenode_map));
    }

    return result;
}

Statement_ptr Solidifier::visit(
    FunctionDefinition& func,
    const std::map<std::string, TypeNode_ptr>& typenode_map
)
{
    func.generics = {};
    func.symbol = nullptr;

    FieldVector solid_fields;

    for (Field& param : func.parameters)
    {
        solid_fields.push_back(visit(param, typenode_map));
    }

    func.parameters = solid_fields;

    if (func.return_type)
    {
        func.return_type = visit(func.return_type, typenode_map);
    }

    func.block = solidify(func.block, typenode_map);

    return make_statement(func);
}

Block Solidifier::solidify(Block& block, const std::map<std::string, TypeNode_ptr>& typenode_map)
{
    Block b;
    b.statements = visit(block.statements, typenode_map);
    return b;
}

TypeNode_ptr Solidifier::visit(
    TypeNode_ptr& type_node,
    const std::map<std::string, TypeNode_ptr>& typenode_map
)
{
    Doctor::semantics().fatal_if_nullptr(type_node, "Attempted to visit a null TypeNode");

    return std::visit(
        overloaded{
            [&](TypeIdentifierNode& ident) -> TypeNode_ptr
            {
                // replace if we have a substitution
                auto it = typenode_map.find(ident.name);
                if (it != typenode_map.end())
                {
                    return ASTCloner::get().clone(it->second);
                }

                // send back original

                return type_node;
            },
            [&](AngularTypeNode& angular) -> TypeNode_ptr
            {
                // Solidify type arguments and create a concrete type name
                TypeNodeVector solidified_args;
                for (auto& arg : angular.type_arguments)
                {
                    solidified_args.push_back(visit(arg, typenode_map));
                }
                // For now, produce a mangled identifier
                std::string mangled_name = angular.name;
                for (auto& arg : solidified_args)
                {
                    if (arg->is<TypeIdentifierNode>())
                    {
                        mangled_name += "_" + arg->as<TypeIdentifierNode>().name;
                    }
                }
                TypeIdentifierNode result;
                result.name = mangled_name;
                return make_type_node(result);
            },
            [&](ListTypeNode& list) -> TypeNode_ptr
            {
                list.element_type = visit(list.element_type, typenode_map);
                return make_type_node(list);
            },
            [&](TupleTypeNode& tuple) -> TypeNode_ptr
            {
                for (auto& elem : tuple.element_types)
                {
                    tuple.element_types.push_back(visit(elem, typenode_map));
                }
                return make_type_node(tuple);
            },
            [&](SetTypeNode& set) -> TypeNode_ptr
            {
                set.element_type = visit(set.element_type, typenode_map);
                return make_type_node(set);
            },
            [&](MapTypeNode& map) -> TypeNode_ptr
            {
                map.key_type = visit(map.key_type, typenode_map);
                map.value_type = visit(map.value_type, typenode_map);
                return make_type_node(map);
            },
            [&](VariantTypeNode& variant) -> TypeNode_ptr
            {
                for (auto& opt : variant.options)
                {
                    variant.options.push_back(visit(opt, typenode_map));
                }

                return make_type_node(variant);
            },
            [&](IntersectionTypeNode& inter) -> TypeNode_ptr
            {
                for (auto& t : inter.types)
                {
                    inter.types.push_back(visit(t, typenode_map));
                }

                return make_type_node(inter);
            },
            [&](FunctionTypeNode& func) -> TypeNode_ptr
            {
                for (auto& param : func.parameter_types)
                {
                    func.parameter_types.push_back(visit(param, typenode_map));
                }

                func.return_type = visit(func.return_type, typenode_map);
                return make_type_node(func);
            },
            [&](auto&) -> TypeNode_ptr
            {
                return type_node;
            }
        },
        type_node->data
    );
}

TypeNodeVector Solidifier::visit(
    TypeNodeVector& types,
    const std::map<std::string, TypeNode_ptr>& typenode_map
)
{
    TypeNodeVector result;
    result.reserve(types.size());

    for (TypeNode_ptr& type : types)
    {
        result.push_back(visit(type, typenode_map));
    }

    return result;
}

Field Solidifier::visit(Field& field, const std::map<std::string, TypeNode_ptr>& typenode_map)
{
    field.type = visit(field.type, typenode_map);
    field.symbol = nullptr;
    return field;
}

FieldVector Solidifier::visit(FieldVector& fields, const std::map<std::string, TypeNode_ptr>& typenode_map)
{
    FieldVector result;
    result.reserve(fields.size());

    for (auto& field : fields)
    {
        result.push_back(visit(field, typenode_map));
    }

    return result;
}

std::map<std::string, TypeNode_ptr> Solidifier::make_typenode_substitutions(
    const std::map<std::string, Type_ptr>& substitutions
)
{
    std::map<std::string, TypeNode_ptr> result;

    for (auto& [name, type] : substitutions)
    {
        result[name] = type_to_typenode(type);
    }

    return result;
}

TypeNodeVector Solidifier::types_to_typenodes(TypeVector& types)
{
    TypeNodeVector result;
    result.reserve(types.size());

    for (Type_ptr& t : types)
    {
        result.push_back(type_to_typenode(t));
    }

    return result;
}

TypeNode_ptr Solidifier::type_to_typenode(Type_ptr type)
{
    Doctor::semantics().fatal_if_nullptr(type, "Attempted to substitute a null Type");

    return std::visit(
        overloaded{
            [](IntType_ptr) -> TypeNode_ptr
            {
                return make_type_node(TypeIdentifierNode{"int"});
            },
            [](FloatType_ptr) -> TypeNode_ptr
            {
                return make_type_node(TypeIdentifierNode{"float"});
            },
            [](StringType_ptr) -> TypeNode_ptr
            {
                return make_type_node(TypeIdentifierNode{"str"});
            },
            [](BooleanType_ptr) -> TypeNode_ptr
            {
                return make_type_node(TypeIdentifierNode{"bool"});
            },
            [](NoneType_ptr) -> TypeNode_ptr
            {
                return make_type_node(NoneTypeNode{});
            },
            [](GenericType_ptr generic) -> TypeNode_ptr
            {
                return make_type_node(TypeIdentifierNode{generic->name});
            },
            [](ClassType_ptr cls) -> TypeNode_ptr
            {
                return make_type_node(TypeIdentifierNode{cls->name});
            },
            [](TraitType_ptr trait) -> TypeNode_ptr
            {
                return make_type_node(TypeIdentifierNode{trait->name});
            },
            [](PrimitiveType_ptr prim) -> TypeNode_ptr
            {
                return make_type_node(TypeIdentifierNode{prim->name});
            },
            [&](ListType_ptr list) -> TypeNode_ptr
            {
                ListTypeNode result;
                result.element_type = type_to_typenode(list->element_type);
                return make_type_node(result);
            },
            [&](TupleType_ptr tuple) -> TypeNode_ptr
            {
                TupleTypeNode result;
                for (const Type_ptr& elem : tuple->element_types)
                {
                    result.element_types.push_back(type_to_typenode(elem));
                }
                return make_type_node(result);
            },
            [&](SetType_ptr set) -> TypeNode_ptr
            {
                SetTypeNode result;
                result.element_type = type_to_typenode(set->element_type);
                return make_type_node(result);
            },
            [&](MapType_ptr map) -> TypeNode_ptr
            {
                MapTypeNode result;
                result.key_type = type_to_typenode(map->key_type);
                result.value_type = type_to_typenode(map->value_type);
                return make_type_node(result);
            },
            [&](VariantType_ptr variant) -> TypeNode_ptr
            {
                VariantTypeNode result;

                for (auto& opt : variant->types)
                {
                    result.options.push_back(type_to_typenode(opt));
                }

                return make_type_node(result);
            },
            [&](IntersectionType_ptr inter) -> TypeNode_ptr
            {
                IntersectionTypeNode result;

                for (auto& t : inter->types)
                {
                    result.types.push_back(type_to_typenode(t));
                }

                return make_type_node(result);
            },
            [](EnumType_ptr enum_type) -> TypeNode_ptr
            {
                return make_type_node(TypeIdentifierNode{enum_type->name});
            },
            [&](AngularType_ptr angular) -> TypeNode_ptr
            {
                AngularTypeNode result;
                result.name = angular->name;

                for (const Type_ptr& arg : angular->type_arguments)
                {
                    result.type_arguments.push_back(type_to_typenode(arg));
                }

                return make_type_node(result);
            },
            [](auto&) -> TypeNode_ptr
            {
                Doctor::semantics().fatal("Unsupported type in type_to_typenode");
            }
        },
        type->data
    );
}

Type_ptr Solidifier::substitute_type(Type_ptr type, std::map<std::string, Type_ptr>& substitutions) const
{
    Doctor::semantics().fatal_if_nullptr(type, "Attempted to substitute a null Type");

    return std::visit(
        overloaded{
            [&](GenericType_ptr t) -> Type_ptr
            {
                auto it = substitutions.find(t->name);
                if (it != substitutions.end())
                {
                    return it->second;
                }

                return type;
            },

            // ============================================================================
            // Composite Types
            // ============================================================================
            [&](ListType_ptr t) -> Type_ptr
            {
                Type_ptr new_element = substitute_type(t->element_type, substitutions);
                return make_shared_type<ListType>(new_element);
            },
            [&](SetType_ptr t) -> Type_ptr
            {
                Type_ptr new_element = substitute_type(t->element_type, substitutions);
                return make_shared_type<SetType>(new_element);
            },
            [&](TupleType_ptr t) -> Type_ptr
            {
                TypeVector new_elements;
                for (const Type_ptr& elem : t->element_types)
                {
                    new_elements.push_back(substitute_type(elem, substitutions));
                }
                return make_shared_type<TupleType>(new_elements);
            },
            [&](MapType_ptr t) -> Type_ptr
            {
                Type_ptr new_key = substitute_type(t->key_type, substitutions);
                Type_ptr new_value = substitute_type(t->value_type, substitutions);
                return make_shared_type<MapType>(new_key, new_value);
            },

            // ============================================================================
            // Algebraic Types
            // ============================================================================
            [&](VariantType_ptr t) -> Type_ptr
            {
                TypeVector new_types;
                for (const Type_ptr& opt : t->types)
                {
                    new_types.push_back(substitute_type(opt, substitutions));
                }
                return make_shared_type<VariantType>(new_types);
            },
            [&](IntersectionType_ptr t) -> Type_ptr
            {
                TypeVector new_types;
                for (const Type_ptr& opt : t->types)
                {
                    new_types.push_back(substitute_type(opt, substitutions));
                }
                return make_shared_type<IntersectionType>(new_types);
            },

            // ============================================================================
            // Callable Types
            // ============================================================================

            [&](FunctionType_ptr t) -> Type_ptr
            {
                TypeVector new_params;
                for (const Type_ptr& param : t->parameter_types)
                {
                    new_params.push_back(substitute_type(param, substitutions));
                }

                Type_ptr new_return = substitute_type(t->return_type, substitutions);

                return make_shared_type<FunctionType>(
                    t->name,
                    new_params,
                    new_return,
                    t->template_type,
                    t->is_pure,
                    t->is_native
                );
            },

            [&](MethodType_ptr t) -> Type_ptr
            {
                TypeVector new_params;
                for (const Type_ptr& param : t->parameter_types)
                {
                    new_params.push_back(substitute_type(param, substitutions));
                }

                Type_ptr new_return = substitute_type(t->return_type, substitutions);

                return make_shared_type<MethodType>(
                    t->name,
                    new_params,
                    new_return,
                    t->template_type,
                    t->is_shared,
                    t->is_pure,
                    t->is_native,
                    t->is_required
                );
            },

            [&](FunctionTypeVector vec) -> Type_ptr
            {
                FunctionTypeVector new_vec;
                for (auto& func : vec)
                {
                    auto substituted = substitute_type(make_type(func), substitutions);
                    new_vec.push_back(substituted->as<FunctionType_ptr>());
                }
                return make_type(new_vec);
            },

            [&](MethodTypeVector vec) -> Type_ptr
            {
                MethodTypeVector new_vec;
                for (auto& method : vec)
                {
                    auto substituted = substitute_type(make_type(method), substitutions);
                    new_vec.push_back(substituted->as<MethodType_ptr>());
                }
                return make_type(new_vec);
            },

            // ============================================================================
            // OOP Types
            // ============================================================================
            [&](ClassType_ptr t) -> Type_ptr
            {
                FieldMap_ptr new_fields = std::make_shared<FieldMap>();
                if (t->fields)
                {
                    for (auto& [name, field_type] : t->fields->types)
                    {
                        new_fields->types[name] = substitute_type(field_type, substitutions);
                        new_fields->ordered_keys.push_back(name);
                    }
                }

                MethodMap_ptr new_methods = std::make_shared<MethodMap>();
                if (t->methods)
                {
                    for (auto& [name, method_vec] : t->methods->method_overload_types)
                    {
                        MethodTypeVector new_vec;
                        for (auto& method : method_vec)
                        {
                            auto substituted = substitute_type(make_type(method), substitutions);
                            new_vec.push_back(substituted->as<MethodType_ptr>());
                        }
                        new_methods->method_overload_types[name] = new_vec;
                        new_methods->ordered_keys.push_back(name);
                    }
                }

                ClassType_ptr new_class = std::make_shared<ClassType>(t->name);
                new_class->fields = new_fields;
                new_class->methods = new_methods;
                new_class->itables = t->itables;

                // Substitute traits
                TypeVector new_traits;
                for (auto& trait : t->traits)
                {
                    new_traits.push_back(substitute_type(trait, substitutions));
                }
                new_class->traits = new_traits;

                new_class->template_type = t->template_type;

                return make_type(new_class);
            },
            [&](TraitType_ptr t) -> Type_ptr
            {
                FieldMap_ptr new_fields = std::make_shared<FieldMap>();
                if (t->fields)
                {
                    for (auto& [name, field_type] : t->fields->types)
                    {
                        new_fields->types[name] = substitute_type(field_type, substitutions);
                        new_fields->ordered_keys.push_back(name);
                    }
                }

                MethodMap_ptr new_methods = std::make_shared<MethodMap>();
                if (t->methods)
                {
                    for (auto& [name, method_vec] : t->methods->method_overload_types)
                    {
                        MethodTypeVector new_vec;
                        for (auto& method : method_vec)
                        {
                            auto substituted = substitute_type(make_type(method), substitutions);
                            new_vec.push_back(substituted->as<MethodType_ptr>());
                        }
                        new_methods->method_overload_types[name] = new_vec;
                        new_methods->ordered_keys.push_back(name);
                    }
                }

                TraitType_ptr new_trait = std::make_shared<TraitType>(t->name);
                new_trait->fields = new_fields;
                new_trait->methods = new_methods;
                new_trait->itables = t->itables;

                TypeVector new_traits;
                for (auto& trait : t->traits)
                {
                    new_traits.push_back(substitute_type(trait, substitutions));
                }
                new_trait->traits = new_traits;

                new_trait->template_type = t->template_type;

                return make_type(new_trait);
            },
            [&](PrimitiveType_ptr t) -> Type_ptr
            {
                FieldMap_ptr new_fields = std::make_shared<FieldMap>();
                if (t->fields)
                {
                    for (auto& [name, field_type] : t->fields->types)
                    {
                        new_fields->types[name] = substitute_type(field_type, substitutions);
                        new_fields->ordered_keys.push_back(name);
                    }
                }

                MethodMap_ptr new_methods = std::make_shared<MethodMap>();
                if (t->methods)
                {
                    for (auto& [name, method_vec] : t->methods->method_overload_types)
                    {
                        MethodTypeVector new_vec;
                        for (auto& method : method_vec)
                        {
                            auto substituted = substitute_type(make_type(method), substitutions);
                            new_vec.push_back(substituted->as<MethodType_ptr>());
                        }
                        new_methods->method_overload_types[name] = new_vec;
                        new_methods->ordered_keys.push_back(name);
                    }
                }

                PrimitiveType_ptr new_trait = std::make_shared<PrimitiveType>(t->name);
                new_trait->fields = new_fields;
                new_trait->methods = new_methods;
                new_trait->itables = t->itables;

                TypeVector new_traits;
                for (auto& trait : t->traits)
                {
                    new_traits.push_back(substitute_type(trait, substitutions));
                }
                new_trait->traits = new_traits;

                new_trait->template_type = t->template_type;

                return make_type(new_trait);
            },

            // ============================================================================
            // Simple types don't need subs
            // ============================================================================
            [&](auto&) -> Type_ptr
            {
                return type;
            }
        },
        type->data
    );
}

} // namespace Wasp
