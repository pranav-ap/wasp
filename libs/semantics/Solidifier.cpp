#include "Solidifier.h"
#include "AST.h"
#include "ASTCloner.h"
#include "Doctor.h"
#include "Statement.h"
#include "Type.h"
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

static TypeVector extract_types(const std::map<std::string, Type_ptr>& subs)
{
    TypeVector result;
    for (const auto& [_, type] : subs)
    {
        result.push_back(type);
    }
    return result;
}

TypeNode_ptr Solidifier::type_to_typenode(Type_ptr type)
{
    if (!type)
    {
        return nullptr;
    }

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
                for (const auto& elem : tuple->element_types)
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
                for (const auto& opt : variant->types)
                {
                    result.options.push_back(type_to_typenode(opt));
                }
                return make_type_node(result);
            },
            [&](IntersectionType_ptr inter) -> TypeNode_ptr
            {
                IntersectionTypeNode result;
                for (const auto& t : inter->types)
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
                for (const auto& arg : angular->type_arguments)
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

TypeNodeVector Solidifier::types_to_typenodes(const TypeVector& types)
{
    TypeNodeVector result;
    result.reserve(types.size());
    for (const auto& t : types)
    {
        result.push_back(type_to_typenode(t));
    }
    return result;
}

std::map<std::string, TypeNode_ptr> Solidifier::make_typenode_substitutions(
    const std::map<std::string, Type_ptr>& substitutions
)
{
    std::map<std::string, TypeNode_ptr> result;
    for (const auto& [name, type] : substitutions)
    {
        result[name] = type_to_typenode(type);
    }
    return result;
}

Type_ptr Solidifier::substitute_type(
    Type_ptr type,
    const std::map<std::string, Type_ptr>& substitutions
) const
{
    if (!type)
    {
        return nullptr;
    }

    if (type->is<GenericType_ptr>())
    {
        auto generic = type->as<GenericType_ptr>();
        auto it = substitutions.find(generic->name);
        if (it != substitutions.end())
        {
            return it->second;
        }
        return type;
    }

    // Handle composite types
    if (type->is<ListType_ptr>())
    {
        auto list = type->as<ListType_ptr>();
        auto new_element = substitute_type(list->element_type, substitutions);
        return make_shared_type<ListType>(new_element);
    }

    if (type->is<SetType_ptr>())
    {
        auto set = type->as<SetType_ptr>();
        auto new_element = substitute_type(set->element_type, substitutions);
        return make_shared_type<SetType>(new_element);
    }

    if (type->is<MapType_ptr>())
    {
        auto map = type->as<MapType_ptr>();
        auto new_key = substitute_type(map->key_type, substitutions);
        auto new_value = substitute_type(map->value_type, substitutions);
        return make_shared_type<MapType>(new_key, new_value);
    }

    if (type->is<TupleType_ptr>())
    {
        auto tuple = type->as<TupleType_ptr>();
        TypeVector new_elements;
        for (const auto& elem : tuple->element_types)
        {
            new_elements.push_back(substitute_type(elem, substitutions));
        }
        return make_shared_type<TupleType>(new_elements);
    }

    return type;
}

// ============================================================================
// Name mangling
// ============================================================================

std::string Solidifier::mangle(const Type_ptr& type)
{
    return std::visit(
        overloaded{
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
            [](ClassType_ptr cls) -> std::string
            {
                return cls->name;
            },
            [](TraitType_ptr trait) -> std::string
            {
                return trait->name;
            },
            [](PrimitiveType_ptr prim) -> std::string
            {
                return prim->name;
            },
            [&](ListType_ptr list) -> std::string
            {
                return "list_" + mangle(list->element_type);
            },
            [&](TupleType_ptr tuple) -> std::string
            {
                std::string result = "tuple";
                for (const auto& elem : tuple->element_types)
                {
                    result += "_" + mangle(elem);
                }
                return result;
            },
            [&](SetType_ptr set) -> std::string
            {
                return "set_" + mangle(set->element_type);
            },
            [&](MapType_ptr map) -> std::string
            {
                return "map_" + mangle(map->key_type) + "_" + mangle(map->value_type);
            },
            [](auto&) -> std::string
            {
                return "unknown";
            }
        },
        type->data
    );
}

std::string Solidifier::mangle(const TypeVector& types)
{
    std::string result;
    for (size_t i = 0; i < types.size(); ++i)
    {
        if (i > 0)
        {
            result += "_";
        }
        result += mangle(types[i]);
    }
    return result;
}

std::string Solidifier::get_solidified_name(const std::string& base_name, const TypeVector& type_arguments)
{
    if (type_arguments.empty())
    {
        return base_name;
    }
    return base_name + "_" + mangle(type_arguments);
}

bool Solidifier::is_generic(const FieldVector& generics) const
{
    return !generics.empty();
}

// ============================================================================
// Public API – forward to private solidify_node
// ============================================================================

Statement_ptr Solidifier::solidify(
    const FunctionDefinition& func,
    const std::map<std::string, Type_ptr>& substitution_map
)
{
    auto typenode_map = make_typenode_substitutions(substitution_map);
    auto stmt = solidify_node(func, typenode_map);
    if (!substitution_map.empty())
    {
        std::string name = get_solidified_name(func.name, extract_types(substitution_map));
    }
    return stmt;
}

Statement_ptr Solidifier::solidify(
    const MethodDefinition& method,
    const std::map<std::string, Type_ptr>& substitution_map
)
{
    auto typenode_map = make_typenode_substitutions(substitution_map);
    auto stmt = solidify_node(method, typenode_map);
    if (!substitution_map.empty())
    {
        std::string name = get_solidified_name(method.name, extract_types(substitution_map));
    }
    return stmt;
}

Statement_ptr Solidifier::solidify(
    const ClassDefinition& cls,
    const std::map<std::string, Type_ptr>& substitution_map
)
{
    auto typenode_map = make_typenode_substitutions(substitution_map);
    auto stmt = solidify_node(cls, typenode_map);
    if (!substitution_map.empty())
    {
        std::string name = get_solidified_name(cls.name, extract_types(substitution_map));
    }
    return stmt;
}

Statement_ptr Solidifier::solidify(
    const TraitDefinition& trait,
    const std::map<std::string, Type_ptr>& substitution_map
)
{
    auto typenode_map = make_typenode_substitutions(substitution_map);
    auto stmt = solidify_node(trait, typenode_map);
    if (!substitution_map.empty())
    {
        std::string name = get_solidified_name(trait.name, extract_types(substitution_map));
    }
    return stmt;
}

Statement_ptr Solidifier::solidify(
    const RecordDefinition& record,
    const std::map<std::string, Type_ptr>& substitution_map
)
{
    auto typenode_map = make_typenode_substitutions(substitution_map);
    auto stmt = solidify_node(record, typenode_map);
    if (!substitution_map.empty())
    {
        std::string name = get_solidified_name(record.name, extract_types(substitution_map));
    }
    return stmt;
}

Statement_ptr Solidifier::solidify(
    const OperatorDefinition& op,
    const std::map<std::string, Type_ptr>& substitution_map
)
{
    auto typenode_map = make_typenode_substitutions(substitution_map);
    auto stmt = solidify_node(op, typenode_map);
    if (!substitution_map.empty())
    {
        std::string name = get_solidified_name(op.name, extract_types(substitution_map));
    }
    return stmt;
}

Statement_ptr Solidifier::solidify(
    const Statement_ptr& stmt,
    const std::map<std::string, Type_ptr>& substitution_map
)
{
    auto typenode_map = make_typenode_substitutions(substitution_map);
    return solidify_node(stmt, typenode_map);
}

StatementVector Solidifier::solidify(
    const StatementVector& statements,
    const std::map<std::string, Type_ptr>& substitution_map
)
{
    auto typenode_map = make_typenode_substitutions(substitution_map);
    return solidify_node(statements, typenode_map);
}

Block Solidifier::solidify_block(const Block& block, const std::map<std::string, Type_ptr>& substitution_map)
{
    auto typenode_map = make_typenode_substitutions(substitution_map);
    return solidify_node(block, typenode_map);
}

Expression_ptr Solidifier::solidify(
    const Expression_ptr& expr,
    const std::map<std::string, Type_ptr>& substitution_map
)
{
    auto typenode_map = make_typenode_substitutions(substitution_map);
    return solidify_node(expr, typenode_map);
}

ExpressionVector Solidifier::solidify(
    const ExpressionVector& expressions,
    const std::map<std::string, Type_ptr>& substitution_map
)
{
    auto typenode_map = make_typenode_substitutions(substitution_map);
    return solidify_node(expressions, typenode_map);
}

TypeNode_ptr Solidifier::solidify(
    const TypeNode_ptr& type_node,
    const std::map<std::string, Type_ptr>& substitution_map
)
{
    auto typenode_map = make_typenode_substitutions(substitution_map);
    return solidify_node(type_node, typenode_map);
}

TypeNodeVector Solidifier::solidify(
    const TypeNodeVector& types,
    const std::map<std::string, Type_ptr>& substitution_map
)
{
    auto typenode_map = make_typenode_substitutions(substitution_map);
    return solidify_node(types, typenode_map);
}

Field Solidifier::solidify(const Field& field, const std::map<std::string, Type_ptr>& substitution_map)
{
    auto typenode_map = make_typenode_substitutions(substitution_map);
    return solidify_node(field, typenode_map);
}

FieldVector Solidifier::solidify(
    const FieldVector& fields,
    const std::map<std::string, Type_ptr>& substitution_map
)
{
    auto typenode_map = make_typenode_substitutions(substitution_map);
    return solidify_node(fields, typenode_map);
}

// ============================================================================
// Private helpers – do the actual work with TypeNode maps
// ============================================================================

Statement_ptr Solidifier::solidify_node(
    const FunctionDefinition& func,
    const std::map<std::string, TypeNode_ptr>& typenode_map
)
{
    FunctionDefinition result;
    result.name = func.name;
    result.is_pure = func.is_pure;
    result.generics = {}; // generics removed after instantiation
    result.symbol = func.symbol;

    for (const auto& param : func.parameters)
    {
        result.parameters.push_back(solidify_node(param, typenode_map));
    }

    if (func.return_type)
    {
        result.return_type = solidify_node(func.return_type, typenode_map);
    }
    else
    {
        result.return_type = nullptr;
    }

    result.block = solidify_node(func.block, typenode_map);

    return make_statement(result);
}

Statement_ptr Solidifier::solidify_node(
    const MethodDefinition& method,
    const std::map<std::string, TypeNode_ptr>& typenode_map
)
{
    MethodDefinition result;
    result.name = method.name;
    result.is_pure = method.is_pure;
    result.is_shared = method.is_shared;
    result.symbol = method.symbol;

    for (const auto& param : method.parameters)
    {
        result.parameters.push_back(solidify_node(param, typenode_map));
    }
    result.return_type = solidify_node(method.return_type, typenode_map);
    result.block = solidify_node(method.block, typenode_map);

    return make_statement(result);
}

Statement_ptr Solidifier::solidify_node(
    const ClassDefinition& cls,
    const std::map<std::string, TypeNode_ptr>& typenode_map
)
{
    ClassDefinition result;
    result.name = cls.name;
    result.generics = {};
    result.symbol = cls.symbol;

    for (const auto& field : cls.fields)
    {
        result.fields.push_back(solidify_node(field, typenode_map));
    }

    for (const auto& method : cls.methods)
    {
        auto solidified = solidify_node(method, typenode_map);
        if (solidified && solidified->is<MethodDefinition>())
        {
            result.methods.push_back(solidified->as<MethodDefinition>());
        }
    }

    for (const auto& trait : cls.traits)
    {
        result.traits.push_back(solidify_node(trait, typenode_map));
    }

    return make_statement(result);
}

Statement_ptr Solidifier::solidify_node(
    const TraitDefinition& trait,
    const std::map<std::string, TypeNode_ptr>& typenode_map
)
{
    TraitDefinition result;
    result.name = trait.name;
    result.generics = {};
    result.symbol = trait.symbol;

    for (const auto& field : trait.fields)
    {
        result.fields.push_back(solidify_node(field, typenode_map));
    }

    for (const auto& method : trait.methods)
    {
        auto solidified = solidify_node(method, typenode_map);
        if (solidified && solidified->is<MethodDefinition>())
        {
            result.methods.push_back(solidified->as<MethodDefinition>());
        }
    }

    for (const auto& super : trait.traits)
    {
        result.traits.push_back(solidify_node(super, typenode_map));
    }

    return make_statement(result);
}

Statement_ptr Solidifier::solidify_node(
    const RecordDefinition& record,
    const std::map<std::string, TypeNode_ptr>& typenode_map
)
{
    RecordDefinition result;
    result.name = record.name;
    result.symbol = record.symbol;

    for (const auto& field : record.fields)
    {
        result.fields.push_back(solidify_node(field, typenode_map));
    }

    return make_statement(result);
}

Statement_ptr Solidifier::solidify_node(
    const OperatorDefinition& op,
    const std::map<std::string, TypeNode_ptr>& typenode_map
)
{
    OperatorDefinition result;
    result.name = op.name;
    result.generics = {};
    result.op_type = op.op_type;
    result.fixity = op.fixity;
    result.symbol = op.symbol;

    for (const auto& operand : op.operands)
    {
        result.operands.push_back(solidify_node(operand, typenode_map));
    }
    result.return_type = solidify_node(op.return_type, typenode_map);
    result.block = solidify_node(op.block, typenode_map);

    return make_statement(result);
}

Statement_ptr Solidifier::solidify_node(
    const Statement_ptr& stmt,
    const std::map<std::string, TypeNode_ptr>& typenode_map
)
{
    Doctor::semantics().fatal_if_nullptr(stmt, "Attempted to solidify a null Statement");

    return std::visit(
        overloaded{
            [&](const FunctionDefinition& s) -> Statement_ptr
            {
                return solidify_node(s, typenode_map);
            },
            [&](const MethodDefinition& s) -> Statement_ptr
            {
                return solidify_node(s, typenode_map);
            },
            [&](const ClassDefinition& s) -> Statement_ptr
            {
                return solidify_node(s, typenode_map);
            },
            [&](const TraitDefinition& s) -> Statement_ptr
            {
                return solidify_node(s, typenode_map);
            },
            [&](const RecordDefinition& s) -> Statement_ptr
            {
                return solidify_node(s, typenode_map);
            },
            [&](const OperatorDefinition& s) -> Statement_ptr
            {
                return solidify_node(s, typenode_map);
            },
            [&](const Block& s) -> Statement_ptr
            {
                Block b = solidify_node(s, typenode_map);
                return make_statement(b);
            },
            [&](const ExpressionStatement& s) -> Statement_ptr
            {
                ExpressionStatement es;
                es.expression = solidify_node(s.expression, typenode_map);
                return make_statement(es);
            },
            [&](const TypeAliasDefinition& s) -> Statement_ptr
            {
                TypeAliasDefinition tas;
                tas.name = s.name;
                tas.generics = {};
                tas.ref_type = solidify_node(s.ref_type, typenode_map);
                tas.symbol = s.symbol;
                return make_statement(tas);
            },
            [&](const EnumDefinition& s) -> Statement_ptr
            {
                // Enums currently have no generics; just clone
                return ASTCloner::get().clone(s);
            },
            [&](const PrimitiveDefinition& s) -> Statement_ptr
            {
                return ASTCloner::get().clone(s);
            },
            [&](const Branch& s) -> Statement_ptr
            {
                Branch b;
                b.test = solidify_node(s.test, typenode_map);
                b.block = solidify_node(s.block, typenode_map);
                b.alternative = solidify_node(s.alternative, typenode_map);
                return make_statement(b);
            },
            [&](const SimpleLoop& s) -> Statement_ptr
            {
                SimpleLoop sl;
                sl.style = s.style;
                sl.test = solidify_node(s.test, typenode_map);
                sl.block = solidify_node(s.block, typenode_map);
                return make_statement(sl);
            },
            [&](const ForInLoop& s) -> Statement_ptr
            {
                ForInLoop fil;
                fil.lhs_is_mutable = s.lhs_is_mutable;
                fil.lhs = solidify_node(s.lhs, typenode_map);
                fil.iterable = solidify_node(s.iterable, typenode_map);
                fil.block = solidify_node(s.block, typenode_map);
                return make_statement(fil);
            },
            [&](const LoopControl& s) -> Statement_ptr
            {
                return ASTCloner::get().clone(s);
            },
            [&](const Return& s) -> Statement_ptr
            {
                Return r;
                if (s.expression)
                {
                    r.expression = solidify_node(*s.expression, typenode_map);
                }
                return make_statement(r);
            },
            [&](const Pass&) -> Statement_ptr
            {
                return make_statement(Pass{});
            },
            [&](const Required&) -> Statement_ptr
            {
                return make_statement(Required{});
            },
            [&](const Native&) -> Statement_ptr
            {
                return make_statement(Native{});
            },
            [&](const Import& s) -> Statement_ptr
            {
                return ASTCloner::get().clone(s);
            },
            [&](const std::monostate&) -> Statement_ptr
            {
                Doctor::semantics().fatal("Attempted to solidify monostate");
            }
        },
        stmt->data
    );
}

StatementVector Solidifier::solidify_node(
    const StatementVector& statements,
    const std::map<std::string, TypeNode_ptr>& typenode_map
)
{
    StatementVector result;
    result.reserve(statements.size());
    for (const auto& stmt : statements)
    {
        result.push_back(solidify_node(stmt, typenode_map));
    }
    return result;
}

Block Solidifier::solidify_node(const Block& block, const std::map<std::string, TypeNode_ptr>& typenode_map)
{
    Block b;
    b.statements = solidify_node(block.statements, typenode_map);
    return b;
}

Expression_ptr Solidifier::solidify_node(
    const Expression_ptr& expr,
    const std::map<std::string, TypeNode_ptr>& typenode_map
)
{
    // Currently, expressions are cloned without substitution.
    // If expressions can contain generic type references, they must be handled here.
    return ASTCloner::get().clone(expr);
}

ExpressionVector Solidifier::solidify_node(
    const ExpressionVector& expressions,
    const std::map<std::string, TypeNode_ptr>& typenode_map
)
{
    ExpressionVector result;
    result.reserve(expressions.size());
    for (const auto& expr : expressions)
    {
        result.push_back(solidify_node(expr, typenode_map));
    }
    return result;
}

TypeNode_ptr Solidifier::solidify_node(
    const TypeNode_ptr& type_node,
    const std::map<std::string, TypeNode_ptr>& typenode_map
)
{
    Doctor::semantics().fatal_if_nullptr(type_node, "Attempted to solidify a null TypeNode");

    return std::visit(
        overloaded{
            [&](const TypeIdentifierNode& ident) -> TypeNode_ptr
            {
                auto it = typenode_map.find(ident.name);
                if (it != typenode_map.end())
                {
                    return ASTCloner::get().clone(it->second);
                }
                // Not a generic – keep as is
                return ASTCloner::get().clone(type_node);
            },
            [&](const AngularTypeNode& angular) -> TypeNode_ptr
            {
                // Solidify type arguments and create a concrete type name
                TypeNodeVector solidified_args;
                for (const auto& arg : angular.type_arguments)
                {
                    solidified_args.push_back(solidify_node(arg, typenode_map));
                }
                // For now, produce a mangled identifier
                std::string mangled_name = angular.name;
                for (const auto& arg : solidified_args)
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
            [&](const ListTypeNode& list) -> TypeNode_ptr
            {
                ListTypeNode result;
                result.element_type = solidify_node(list.element_type, typenode_map);
                return make_type_node(result);
            },
            [&](const TupleTypeNode& tuple) -> TypeNode_ptr
            {
                TupleTypeNode result;
                for (const auto& elem : tuple.element_types)
                {
                    result.element_types.push_back(solidify_node(elem, typenode_map));
                }
                return make_type_node(result);
            },
            [&](const SetTypeNode& set) -> TypeNode_ptr
            {
                SetTypeNode result;
                result.element_type = solidify_node(set.element_type, typenode_map);
                return make_type_node(result);
            },
            [&](const MapTypeNode& map) -> TypeNode_ptr
            {
                MapTypeNode result;
                result.key_type = solidify_node(map.key_type, typenode_map);
                result.value_type = solidify_node(map.value_type, typenode_map);
                return make_type_node(result);
            },
            [&](const VariantTypeNode& variant) -> TypeNode_ptr
            {
                VariantTypeNode result;
                for (const auto& opt : variant.options)
                {
                    result.options.push_back(solidify_node(opt, typenode_map));
                }
                return make_type_node(result);
            },
            [&](const IntersectionTypeNode& inter) -> TypeNode_ptr
            {
                IntersectionTypeNode result;
                for (const auto& t : inter.types)
                {
                    result.types.push_back(solidify_node(t, typenode_map));
                }
                return make_type_node(result);
            },
            [&](const FunctionTypeNode& func) -> TypeNode_ptr
            {
                FunctionTypeNode result;
                for (const auto& param : func.parameter_types)
                {
                    result.parameter_types.push_back(solidify_node(param, typenode_map));
                }
                result.return_type = solidify_node(func.return_type, typenode_map);
                return make_type_node(result);
            },
            [&](const auto&) -> TypeNode_ptr
            {
                // NoneTypeNode, LiteralTypeNode – no generics
                return ASTCloner::get().clone(type_node);
            }
        },
        type_node->data
    );
}

TypeNodeVector Solidifier::solidify_node(
    const TypeNodeVector& types,
    const std::map<std::string, TypeNode_ptr>& typenode_map
)
{
    TypeNodeVector result;
    result.reserve(types.size());
    for (const auto& type : types)
    {
        result.push_back(solidify_node(type, typenode_map));
    }
    return result;
}

Field Solidifier::solidify_node(const Field& field, const std::map<std::string, TypeNode_ptr>& typenode_map)
{
    Field result;
    result.name = field.name;
    result.type = solidify_node(field.type, typenode_map);
    result.is_variadic = field.is_variadic;
    result.symbol = field.symbol;
    return result;
}

FieldVector Solidifier::solidify_node(
    const FieldVector& fields,
    const std::map<std::string, TypeNode_ptr>& typenode_map
)
{
    FieldVector result;
    result.reserve(fields.size());
    for (const auto& field : fields)
    {
        result.push_back(solidify_node(field, typenode_map));
    }
    return result;
}

} // namespace Wasp
