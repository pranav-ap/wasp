#include "Solidifier.h"
#include "AST.h"
#include "ASTCloner.h"
#include "Doctor.h"
#include "Statement.h"
#include "TypeNode.h"

#include <cstddef>
#include <map>
#include <string>
#include <variant>

namespace Wasp
{

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

// ============================================================================
// Main Entry Points - Type Argument Version
// ============================================================================

Statement_ptr Solidifier::solidify(
    const FunctionDefinition& func,
    const TypeNodeVector& type_arguments
)
{
    // Build substitution map
    auto subst_map = build_substitution_map(func.generics, type_arguments);

    // Delegate to the internal solidify
    return solidify(func, subst_map);
}

Statement_ptr Solidifier::solidify(
    const MethodDefinition& method,
    const FieldVector& generics,
    const TypeNodeVector& type_arguments
)
{
    auto subst_map = build_substitution_map(generics, type_arguments);
    return solidify(method, subst_map);
}

Statement_ptr Solidifier::solidify(
    const ClassDefinition& cls,
    const TypeNodeVector& type_arguments
)
{
    auto subst_map = build_substitution_map(cls.generics, type_arguments);
    return solidify(cls, subst_map);
}

Statement_ptr Solidifier::solidify(
    const TraitDefinition& trait,
    const TypeNodeVector& type_arguments
)
{
    auto subst_map = build_substitution_map(trait.generics, type_arguments);
    return solidify(trait, subst_map);
}

// ============================================================================
// Main Entry Points - Substitution Map Version
// ============================================================================

Statement_ptr Solidifier::solidify(
    const Statement_ptr& stmt,
    const std::map<std::string, TypeNode_ptr>& substitution_map
)
{
    // For now, just clone the statement - we'll handle each type later
    return ASTCloner::get().clone(stmt);
}

StatementVector Solidifier::solidify(
    const StatementVector& statements,
    const std::map<std::string, TypeNode_ptr>& substitution_map
)
{
    StatementVector result;
    result.reserve(statements.size());
    for (const auto& stmt : statements)
    {
        result.push_back(solidify(stmt, substitution_map));
    }
    return result;
}

Block Solidifier::solidify(
    const Block& block,
    const std::map<std::string, TypeNode_ptr>& substitution_map
)
{
    Block result;
    result.statements = solidify(block.statements, substitution_map);
    return result;
}

Expression_ptr Solidifier::solidify(
    const Expression_ptr& expr,
    const std::map<std::string, TypeNode_ptr>& substitution_map
)
{
    // For now, just clone expressions
    return ASTCloner::get().clone(expr);
}

ExpressionVector Solidifier::solidify(
    const ExpressionVector& expressions,
    const std::map<std::string, TypeNode_ptr>& substitution_map
)
{
    ExpressionVector result;
    result.reserve(expressions.size());
    for (const auto& expr : expressions)
    {
        result.push_back(solidify(expr, substitution_map));
    }
    return result;
}

TypeNode_ptr Solidifier::solidify(
    const TypeNode_ptr& type_node,
    const std::map<std::string, TypeNode_ptr>& substitution_map
)
{
    if (!type_node)
    {
        return nullptr;
    }

    // Visit the type node and substitute generics
    return std::visit(
        overloaded{
            [&](const TypeIdentifierNode& ident) -> TypeNode_ptr
            {
                // Check if this identifier is a generic parameter
                auto it = substitution_map.find(ident.name);
                if (it != substitution_map.end())
                {
                    // Replace with concrete type
                    return ASTCloner::get().clone(it->second);
                }
                // Not a generic - keep as is
                return ASTCloner::get().clone(type_node);
            },
            [&](const AngularTypeNode& angular) -> TypeNode_ptr
            {
                // Solidify the type arguments
                AngularTypeNode result;
                result.name = angular.name;
                result.symbol = angular.symbol;
                for (const auto& arg : angular.type_arguments)
                {
                    result.type_arguments.push_back(solidify(arg, substitution_map));
                }
                return make_type_node(result);
            },
            [&](const ListTypeNode& list) -> TypeNode_ptr
            {
                ListTypeNode result;
                result.element_type = solidify(list.element_type, substitution_map);
                return make_type_node(result);
            },
            [&](const TupleTypeNode& tuple) -> TypeNode_ptr
            {
                TupleTypeNode result;
                for (const auto& elem : tuple.element_types)
                {
                    result.element_types.push_back(solidify(elem, substitution_map));
                }
                return make_type_node(result);
            },
            [&](const SetTypeNode& set) -> TypeNode_ptr
            {
                SetTypeNode result;
                result.element_type = solidify(set.element_type, substitution_map);
                return make_type_node(result);
            },
            [&](const MapTypeNode& map) -> TypeNode_ptr
            {
                MapTypeNode result;
                result.key_type = solidify(map.key_type, substitution_map);
                result.value_type = solidify(map.value_type, substitution_map);
                return make_type_node(result);
            },
            [&](const VariantTypeNode& variant) -> TypeNode_ptr
            {
                VariantTypeNode result;
                for (const auto& opt : variant.options)
                {
                    result.options.push_back(solidify(opt, substitution_map));
                }
                return make_type_node(result);
            },
            [&](const IntersectionTypeNode& inter) -> TypeNode_ptr
            {
                IntersectionTypeNode result;
                for (const auto& type : inter.types)
                {
                    result.types.push_back(solidify(type, substitution_map));
                }
                return make_type_node(result);
            },
            [&](const FunctionTypeNode& func) -> TypeNode_ptr
            {
                FunctionTypeNode result;
                for (const auto& input : func.input_types)
                {
                    result.input_types.push_back(solidify(input, substitution_map));
                }
                result.return_type = solidify(func.return_type, substitution_map);
                return make_type_node(result);
            },
            [&](const auto&) -> TypeNode_ptr
            {
                // NoneTypeNode, LiteralTypeNode - no generics to substitute
                return ASTCloner::get().clone(type_node);
            }
        },
        type_node->data
    );
}

TypeNodeVector Solidifier::solidify(
    const TypeNodeVector& types,
    const std::map<std::string, TypeNode_ptr>& substitution_map
)
{
    TypeNodeVector result;
    result.reserve(types.size());
    for (const auto& type : types)
    {
        result.push_back(solidify(type, substitution_map));
    }
    return result;
}

Field Solidifier::solidify(
    const Field& field,
    const std::map<std::string, TypeNode_ptr>& substitution_map
)
{
    Field result;
    result.name = field.name;
    result.type = solidify(field.type, substitution_map);
    result.is_variadic = field.is_variadic;
    result.symbol = field.symbol; // TODO : Symbols handled later
    return result;
}

FieldVector Solidifier::solidify(
    const FieldVector& fields,
    const std::map<std::string, TypeNode_ptr>& substitution_map
)
{
    FieldVector result;
    result.reserve(fields.size());
    for (const auto& field : fields)
    {
        result.push_back(solidify(field, substitution_map));
    }
    return result;
}

// ============================================================================
// Definition Solidification
// ============================================================================

Statement_ptr Solidifier::solidify(
    const FunctionDefinition& func,
    const std::map<std::string, TypeNode_ptr>& substitution_map
)
{
    // TODO: Implement full function solidification
    return ASTCloner::get().clone(func);
}

Statement_ptr Solidifier::solidify(
    const MethodDefinition& method,
    const std::map<std::string, TypeNode_ptr>& substitution_map
)
{
    // TODO: Implement full method solidification
    return ASTCloner::get().clone(method);
}

Statement_ptr Solidifier::solidify(
    const ClassDefinition& cls,
    const std::map<std::string, TypeNode_ptr>& substitution_map
)
{
    // TODO: Implement full class solidification
    return ASTCloner::get().clone(cls);
}

Statement_ptr Solidifier::solidify(
    const TraitDefinition& trait,
    const std::map<std::string, TypeNode_ptr>& substitution_map
)
{
    // TODO: Implement full trait solidification
    return ASTCloner::get().clone(trait);
}

// ============================================================================
// Substitution Map Building
// ============================================================================

std::map<std::string, TypeNode_ptr> Solidifier::build_substitution_map(
    const FieldVector& generics,
    const TypeNodeVector& type_arguments
)
{
    std::map<std::string, TypeNode_ptr> subst_map;

    Doctor::semantics().assert(
        generics.size() == type_arguments.size(),
        "Number of type arguments must match number of generic parameters"
    );

    for (size_t i = 0; i < generics.size(); ++i)
    {
        subst_map[generics[i].name] = type_arguments[i];
    }

    return subst_map;
}

// ============================================================================
// Utils
// ============================================================================

bool Solidifier::is_generic(const FieldVector& generics) const
{
    return !generics.empty();
}

std::string Solidifier::mangle(const TypeNodeVector& args)
{
    std::string result;
    for (size_t i = 0; i < args.size(); ++i)
    {
        if (i > 0)
        {
            result += "_";
        }
        result += mangle(args[i]);
    }
    return result;
}

std::string Solidifier::mangle(const TypeNode_ptr& type)
{
    Doctor::semantics().fatal_if_nullptr(
        type,
        "Attempted to mangle a null TypeNode"
    );

    return std::visit(
        overloaded{
            [&](const TypeIdentifierNode& ident) -> std::string
            {
                return ident.name;
            },
            [&](const AngularTypeNode& angular) -> std::string
            {
                std::string result = angular.name;
                for (const auto& arg : angular.type_arguments)
                {
                    result += "_" + mangle(arg);
                }
                return result;
            },
            [&](const ListTypeNode& list) -> std::string
            {
                return "list_" + mangle(list.element_type);
            },
            [&](const TupleTypeNode& tuple) -> std::string
            {
                std::string result = "tuple";
                for (const auto& elem : tuple.element_types)
                {
                    result += "_" + mangle(elem);
                }
                return result;
            },
            [&](const SetTypeNode& set) -> std::string
            {
                return "set_" + mangle(set.element_type);
            },
            [&](const MapTypeNode& map) -> std::string
            {
                return "map_" + mangle(map.key_type) + "_" + mangle(map.value_type);
            },
            [&](const auto&) -> std::string
            {
                Doctor::semantics().fatal(
                    "Attempted to mangle an unsupported TypeNode variant"
                );
            }
        },
        type->data
    );
}

std::string Solidifier::get_solid_name(
    const std::string& base_name,
    const TypeNodeVector& type_arguments
)
{
    if (type_arguments.empty())
    {
        return base_name;
    }

    return base_name + "_" + mangle(type_arguments);
}

} // namespace Wasp
