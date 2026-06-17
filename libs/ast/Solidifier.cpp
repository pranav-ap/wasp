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
// Entry Points
// ============================================================================

Statement_ptr Solidifier::solidify(
    const FunctionDefinition& func,
    const TypeNodeVector& type_arguments
)
{
    auto subst_map = build_substitution_map(func.generics, type_arguments);
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
// Internals
// ============================================================================

Statement_ptr Solidifier::solidify(
    const Statement_ptr& stmt,
    const std::map<std::string, TypeNode_ptr>& substitution_map
)
{
    Doctor::semantics().fatal_if_nullptr(
        stmt,
        "Attempted to solidify a null Statement"
    );

    return std::visit(
        overloaded{
            [&](const std::monostate&) -> Statement_ptr
            {
                Doctor::semantics().fatal(
                    "Attempted to solidify a Statement in monostate"
                );
            },
            [&](const Import& s) -> Statement_ptr
            {
                return ASTCloner::get().clone(s);
            },
            [&](const ExpressionStatement& s) -> Statement_ptr
            {
                ExpressionStatement result;
                result.expression = solidify(s.expression, substitution_map);
                return make_statement(result);
            },
            [&](const TypeAliasDefinition& s) -> Statement_ptr
            {
                TypeAliasDefinition result;
                result.name = s.name;
                result.generics = {}; // No generics after instantiation
                result.ref_type = solidify(s.ref_type, substitution_map);
                result.symbol = s.symbol;
                result.overload_symbol = s.overload_symbol;
                return make_statement(result);
            },
            [&](const EnumDefinition& s) -> Statement_ptr
            {
                // TODO : Implement full enum solidification
                return ASTCloner::get().clone(s);
            },
            [&](const FunctionDefinition& s) -> Statement_ptr
            {
                return solidify(s, substitution_map);
            },
            [&](const MethodDefinition& s) -> Statement_ptr
            {
                return solidify(s, substitution_map);
            },
            [&](const OperatorDefinition& s) -> Statement_ptr
            {
                OperatorDefinition result;
                result.name = s.name;
                result.generics = {}; // No generics after instantiation
                result.op_type = s.op_type;
                result.fixity = s.fixity;
                // Solidify operands (parameters)
                for (const auto& operand : s.operands)
                {
                    result.operands.push_back(solidify(operand, substitution_map));
                }
                // Solidify return type
                result.return_type = solidify(s.return_type, substitution_map);
                // Solidify body
                result.block = solidify(s.block, substitution_map);
                result.symbol = s.symbol; // TODO must handle symbols
                result.overload_symbol = s.overload_symbol;
                return make_statement(result);
            },
            [&](const ClassDefinition& s) -> Statement_ptr
            {
                return solidify(s, substitution_map);
            },
            [&](const TraitDefinition& s) -> Statement_ptr
            {
                return solidify(s, substitution_map);
            },
            [&](const PrimitiveDefinition& s) -> Statement_ptr
            {
                return ASTCloner::get().clone(s);
            },
            [&](const Branch& s) -> Statement_ptr
            {
                Branch result;
                result.test = solidify(s.test, substitution_map);
                result.block = solidify(s.block, substitution_map);
                result.alternative = solidify(s.alternative, substitution_map);
                return make_statement(result);
            },
            [&](const SimpleLoop& s) -> Statement_ptr
            {
                SimpleLoop result;
                result.style = s.style;
                result.test = solidify(s.test, substitution_map);
                result.block = solidify(s.block, substitution_map);
                return make_statement(result);
            },
            [&](const ForInLoop& s) -> Statement_ptr
            {
                ForInLoop result;
                result.lhs_is_mutable = s.lhs_is_mutable;
                result.lhs = solidify(s.lhs, substitution_map);
                result.iterable = solidify(s.iterable, substitution_map);
                result.block = solidify(s.block, substitution_map);
                return make_statement(result);
            },
            [&](const LoopControl& s) -> Statement_ptr
            {
                return ASTCloner::get().clone(s);
            },
            [&](const Return& s) -> Statement_ptr
            {
                Return result;
                if (s.expression)
                {
                    result.expression = solidify(*s.expression, substitution_map);
                }
                return make_statement(result);
            },
            [&](const Pass& s) -> Statement_ptr
            {
                return make_statement(Pass{});
            },
            [&](const Required& s) -> Statement_ptr
            {
                return make_statement(Required{});
            },
            [&](const Native& s) -> Statement_ptr
            {
                return make_statement(Native{});
            }
        },
        stmt->data
    );
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
    Doctor::semantics().fatal_if_nullptr(
        type_node,
        "Attempted to solidify a null TypeNode"
    );

    return std::visit(
        overloaded{
            [&](const TypeIdentifierNode& ident) -> TypeNode_ptr
            {
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
                TypeNodeVector solidified_args;

                for (const auto& arg : angular.type_arguments)
                {
                    auto solidified_arg = solidify(arg, substitution_map);
                    solidified_args.push_back(solidified_arg);
                }

                std::string mangled_name = get_solid_name(
                    angular.name,
                    solidified_args
                );

                TypeIdentifierNode result;
                result.name = mangled_name;
                // TODO: Set symbol to point to the instantiated type later
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
    FunctionDefinition result;

    result.name = func.name;
    result.is_pure = func.is_pure;
    result.symbol = func.symbol; // TODO: Update symbol later
    result.overload_symbol = func.overload_symbol;

    // Remove generics after instantiation
    result.generics = {};

    // Solidify parameters
    for (const auto& param : func.parameters)
    {
        result.parameters.push_back(solidify(param, substitution_map));
    }

    // Solidify return type
    result.return_type = solidify(func.return_type, substitution_map);

    // Solidify the function body
    result.block = solidify(func.block, substitution_map);

    return make_statement(result);
}

Statement_ptr Solidifier::solidify(
    const MethodDefinition& method,
    const std::map<std::string, TypeNode_ptr>& substitution_map
)
{
    MethodDefinition result;

    result.name = method.name;
    result.is_pure = method.is_pure;
    result.is_shared = method.is_shared;
    result.symbol = method.symbol; // TODO: Update symbol later
    result.overload_symbol = method.overload_symbol;

    // Solidify parameters
    for (const auto& param : method.parameters)
    {
        result.parameters.push_back(solidify(param, substitution_map));
    }

    // Solidify return type
    result.return_type = solidify(method.return_type, substitution_map);

    // Solidify the method body
    result.block = solidify(method.block, substitution_map);

    return make_statement(result);
}

Statement_ptr Solidifier::solidify(
    const ClassDefinition& cls,
    const std::map<std::string, TypeNode_ptr>& substitution_map
)
{
    ClassDefinition result;

    result.name = cls.name;
    result.symbol = cls.symbol; // TODO: Update symbol to point to instantiated
    result.overload_symbol = cls.overload_symbol;

    result.generics = {};

    for (const auto& field : cls.fields)
    {
        result.fields.push_back(solidify(field, substitution_map));
    }

    for (const auto& method : cls.methods)
    {
        auto solidified_method = solidify(method, substitution_map);

        Doctor::semantics().fatal_if_nullptr(
            solidified_method,
            "Expected solidified method to be a MethodDefinition"
        );

        Doctor::semantics().assert(
            solidified_method->is<MethodDefinition>(),
            "Expected solidified method to be a MethodDefinition"
        );

        result.methods.push_back(solidified_method->as<MethodDefinition>());
    }

    for (const auto& trait : cls.traits)
    {
        result.traits.push_back(solidify(trait, substitution_map));
    }

    return make_statement(result);
}

Statement_ptr Solidifier::solidify(
    const TraitDefinition& trait,
    const std::map<std::string, TypeNode_ptr>& substitution_map
)
{
    TraitDefinition result;

    result.name = trait.name;
    result.symbol = trait.symbol; // TODO: Update symbol later
    result.overload_symbol = trait.overload_symbol;

    result.generics = {};

    for (const auto& field : trait.fields)
    {
        result.fields.push_back(solidify(field, substitution_map));
    }

    for (const auto& method : trait.methods)
    {
        auto solidified_method = solidify(method, substitution_map);
        if (solidified_method)
        {
            result.methods.push_back(solidified_method->as<MethodDefinition>());
        }
    }

    for (const auto& super : trait.traits)
    {
        result.traits.push_back(solidify(super, substitution_map));
    }

    return make_statement(result);
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
