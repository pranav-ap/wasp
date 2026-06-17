#include "ASTCloner.h"
#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Statement.h"
#include "TypeNode.h"

#include <variant>

namespace Wasp
{

// ============================================================================
// Overloaded visitor helper
// ============================================================================

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

// ============================================================================
// Entry Points
// ============================================================================

Statement_ptr ASTCloner::clone(const Statement_ptr& stmt)
{
    if (!stmt)
    {
        return nullptr;
    }

    return std::visit(
        overloaded{
            [](const std::monostate&) -> Statement_ptr
            {
                Doctor::semantics().fatal(
                    "Attempted to clone a Statement with monostate"
                );
            },
            [&](const Import& s)
            {
                return clone(s);
            },
            [&](const ExpressionStatement& s)
            {
                return clone(s);
            },
            [&](const TypeAliasDefinition& s)
            {
                return clone(s);
            },
            [&](const EnumDefinition& s)
            {
                return clone(s);
            },
            [&](const FunctionDefinition& s)
            {
                return clone(s);
            },
            [&](const OperatorDefinition& s)
            {
                return clone(s);
            },
            [&](const ClassDefinition& s)
            {
                return clone(s);
            },
            [&](const TraitDefinition& s)
            {
                return clone(s);
            },
            [&](const PrimitiveDefinition& s)
            {
                return clone(s);
            },
            [&](const Branch& s)
            {
                return clone(s);
            },
            [&](const SimpleLoop& s)
            {
                return clone(s);
            },
            [&](const ForInLoop& s)
            {
                return clone(s);
            },
            [&](const LoopControl& s)
            {
                return clone(s);
            },
            [&](const Return& s)
            {
                return clone(s);
            },
            [&](const Pass& s)
            {
                return clone(s);
            },
            [&](const Required& s)
            {
                return clone(s);
            },
            [&](const Native& s)
            {
                return clone(s);
            }
        },
        stmt->data
    );
}

Expression_ptr ASTCloner::clone(const Expression_ptr& expr)
{
    if (!expr)
    {
        return nullptr;
    }

    return std::visit(
        overloaded{
            [](const std::monostate&) -> Expression_ptr
            {
                Doctor::semantics().fatal(
                    "Attempted to clone an Expression with monostate"
                );
            },
            [&](const IntegerLiteral& e)
            {
                return clone(e);
            },
            [&](const FloatLiteral& e)
            {
                return clone(e);
            },
            [&](const StringLiteral& e)
            {
                return clone(e);
            },
            [&](const BooleanLiteral& e)
            {
                return clone(e);
            },
            [&](const NoneLiteral& e)
            {
                return clone(e);
            },
            [&](const InterpolatedString& e)
            {
                return clone(e);
            },
            [&](const Range& e)
            {
                return clone(e);
            },
            [&](const Identifier& e)
            {
                return clone(e);
            },
            [&](const MemberAccess& e)
            {
                return clone(e);
            },
            [&](const Call& e)
            {
                return clone(e);
            },
            [&](const Pipe& e)
            {
                return clone(e);
            },
            [&](const Constructor& e)
            {
                return clone(e);
            },
            [&](const Prefix& e)
            {
                return clone(e);
            },
            [&](const Infix& e)
            {
                return clone(e);
            },
            [&](const ListLiteral& e)
            {
                return clone(e);
            },
            [&](const TupleLiteral& e)
            {
                return clone(e);
            },
            [&](const MapLiteral& e)
            {
                return clone(e);
            },
            [&](const SetLiteral& e)
            {
                return clone(e);
            },
            [&](const Binding& e)
            {
                return clone(e);
            },
            [&](const Assignment& e)
            {
                return clone(e);
            },
            [&](const TernaryExpression& e)
            {
                return clone(e);
            }
        },
        expr->data
    );
}

TypeNode_ptr ASTCloner::clone(const TypeNode_ptr& type)
{
    if (!type)
    {
        return nullptr;
    }

    // Check cache
    auto it = type_cache_.find(type);
    if (it != type_cache_.end())
    {
        return it->second;
    }

    TypeNode_ptr result = std::visit(
        overloaded{
            [](const std::monostate&) -> TypeNode_ptr
            {
                Doctor::semantics().fatal(
                    "Attempted to clone a TypeNode with monostate"
                );
            },
            [&](const NoneTypeNode& t)
            {
                return clone(t);
            },
            [&](const LiteralTypeNode& t)
            {
                return clone(t);
            },
            [&](const TypeIdentifierNode& t)
            {
                return clone(t);
            },
            [&](const ListTypeNode& t)
            {
                return clone(t);
            },
            [&](const TupleTypeNode& t)
            {
                return clone(t);
            },
            [&](const SetTypeNode& t)
            {
                return clone(t);
            },
            [&](const MapTypeNode& t)
            {
                return clone(t);
            },
            [&](const VariantTypeNode& t)
            {
                return clone(t);
            },
            [&](const IntersectionTypeNode& t)
            {
                return clone(t);
            },
            [&](const FunctionTypeNode& t)
            {
                return clone(t);
            },
            [&](const AngularTypeNode& t)
            {
                return clone(t);
            }
        },
        type->data
    );

    if (result)
    {
        type_cache_[type] = result;
    }

    return result;
}

Block ASTCloner::clone(const Block& block)
{
    Block result;
    result.statements.reserve(block.statements.size());
    for (const auto& stmt : block.statements)
    {
        result.statements.push_back(clone(stmt));
    }
    return result;
}

FieldVector ASTCloner::clone(const FieldVector& fields)
{
    FieldVector result;
    result.reserve(fields.size());

    for (const auto& field : fields)
    {
        Field cloned;
        cloned.name = field.name;
        cloned.type = clone(field.type);
        cloned.is_variadic = field.is_variadic;
        result.push_back(cloned);
    }

    return result;
}

FunctionDefinitionVector ASTCloner::clone(
    const FunctionDefinitionVector& funcs
)
{
    FunctionDefinitionVector result;
    result.reserve(funcs.size());

    for (const auto& func : funcs)
    {
        auto cloned = clone(func);
        if (cloned)
        {
            result.push_back(cloned->as<FunctionDefinition>());
        }
    }

    return result;
}

ExpressionVector ASTCloner::clone(const ExpressionVector& expressions)
{
    ExpressionVector result;
    result.reserve(expressions.size());

    for (const auto& expr : expressions)
    {
        result.push_back(clone(expr));
    }

    return result;
}

TypeNodeVector ASTCloner::clone(const TypeNodeVector& types)
{
    TypeNodeVector result;
    result.reserve(types.size());

    for (const auto& type : types)
    {
        result.push_back(clone(type));
    }

    return result;
}

// ============================================================================
// Statement Cloning
// ============================================================================

Statement_ptr ASTCloner::clone(const Import& stmt)
{
    Import cloned;
    cloned.access_modifier = stmt.access_modifier;
    cloned.jumps = stmt.jumps;
    cloned.path = stmt.path;
    cloned.module_alias = stmt.module_alias;
    cloned.expose_all = stmt.expose_all;
    cloned.exposed_names = stmt.exposed_names;
    cloned.excluded_names = stmt.excluded_names;
    return make_statement(cloned);
}

Statement_ptr ASTCloner::clone(const ExpressionStatement& stmt)
{
    ExpressionStatement cloned;
    cloned.expression = clone(stmt.expression);
    return make_statement(cloned);
}

Statement_ptr ASTCloner::clone(const TypeAliasDefinition& stmt)
{
    TypeAliasDefinition cloned;
    cloned.name = stmt.name;
    cloned.generics = clone(stmt.generics);
    cloned.ref_type = clone(stmt.ref_type);
    return make_statement(cloned);
}

Statement_ptr ASTCloner::clone(const EnumDefinition& stmt)
{
    EnumDefinition cloned;
    cloned.name = stmt.name;
    cloned.generics = clone(stmt.generics);
    cloned.members = stmt.members;

    for (const auto& nested : stmt.nested_enums)
    {
        auto cloned_nested = clone(nested);
        if (cloned_nested)
        {
            cloned.nested_enums.push_back(
                cloned_nested->as<EnumDefinition>()
            );
        }
    }

    return make_statement(cloned);
}

Statement_ptr ASTCloner::clone(const FunctionDefinition& stmt)
{
    FunctionDefinition cloned;
    cloned.name = stmt.name;
    cloned.generics = clone(stmt.generics);
    cloned.parameters = clone(stmt.parameters);
    cloned.return_type = clone(stmt.return_type);
    cloned.block = clone(stmt.block);
    cloned.is_pure = stmt.is_pure;
    cloned.is_shared = stmt.is_shared;
    return make_statement(cloned);
}

Statement_ptr ASTCloner::clone(const OperatorDefinition& stmt)
{
    OperatorDefinition cloned;
    cloned.name = stmt.name;
    cloned.generics = clone(stmt.generics);
    cloned.op_type = stmt.op_type;
    cloned.fixity = stmt.fixity;
    cloned.operands = clone(stmt.operands);
    cloned.return_type = clone(stmt.return_type);
    cloned.block = clone(stmt.block);
    return make_statement(cloned);
}

Statement_ptr ASTCloner::clone(const ClassDefinition& stmt)
{
    ClassDefinition cloned;
    cloned.name = stmt.name;
    cloned.generics = clone(stmt.generics);
    cloned.fields = clone(stmt.fields);
    cloned.methods = clone(stmt.methods);
    cloned.traits = clone(stmt.traits);
    return make_statement(cloned);
}

Statement_ptr ASTCloner::clone(const TraitDefinition& stmt)
{
    TraitDefinition cloned;
    cloned.name = stmt.name;
    cloned.generics = clone(stmt.generics);
    cloned.fields = clone(stmt.fields);
    cloned.methods = clone(stmt.methods);
    cloned.traits = clone(stmt.traits);
    return make_statement(cloned);
}

Statement_ptr ASTCloner::clone(const PrimitiveDefinition& stmt)
{
    PrimitiveDefinition cloned;
    cloned.name = stmt.name;
    cloned.generics = clone(stmt.generics);
    cloned.fields = clone(stmt.fields);
    cloned.methods = clone(stmt.methods);
    cloned.traits = clone(stmt.traits);
    return make_statement(cloned);
}

Statement_ptr ASTCloner::clone(const Branch& stmt)
{
    Branch cloned;
    cloned.block = clone(stmt.block);
    cloned.test = clone(stmt.test);
    cloned.alternative = clone(stmt.alternative);
    return make_statement(cloned);
}

Statement_ptr ASTCloner::clone(const SimpleLoop& stmt)
{
    SimpleLoop cloned;
    cloned.test = clone(stmt.test);
    cloned.style = stmt.style;
    cloned.block = clone(stmt.block);
    return make_statement(cloned);
}

Statement_ptr ASTCloner::clone(const ForInLoop& stmt)
{
    ForInLoop cloned;
    cloned.lhs_is_mutable = stmt.lhs_is_mutable;
    cloned.lhs = clone(stmt.lhs);
    cloned.iterable = clone(stmt.iterable);
    cloned.block = clone(stmt.block);
    return make_statement(cloned);
}

Statement_ptr ASTCloner::clone(const LoopControl& stmt)
{
    LoopControl cloned;
    cloned.type = stmt.type;
    return make_statement(cloned);
}

Statement_ptr ASTCloner::clone(const Return& stmt)
{
    Return cloned;
    if (stmt.expression)
    {
        cloned.expression = clone(*stmt.expression);
    }
    return make_statement(cloned);
}

Statement_ptr ASTCloner::clone(const Pass&)
{
    return make_statement(Pass{});
}

Statement_ptr ASTCloner::clone(const Required&)
{
    return make_statement(Required{});
}

Statement_ptr ASTCloner::clone(const Native&)
{
    return make_statement(Native{});
}

// ============================================================================
// Expression Cloning
// ============================================================================

Expression_ptr ASTCloner::clone(const IntegerLiteral& expr)
{
    return make_expression(IntegerLiteral{expr.value});
}

Expression_ptr ASTCloner::clone(const FloatLiteral& expr)
{
    return make_expression(FloatLiteral{expr.value});
}

Expression_ptr ASTCloner::clone(const StringLiteral& expr)
{
    return make_expression(StringLiteral{expr.value});
}

Expression_ptr ASTCloner::clone(const BooleanLiteral& expr)
{
    return make_expression(BooleanLiteral{expr.value});
}

Expression_ptr ASTCloner::clone(const NoneLiteral&)
{
    return make_expression(NoneLiteral{});
}

Expression_ptr ASTCloner::clone(const InterpolatedString& expr)
{
    InterpolatedString cloned;
    for (const auto& part : expr.parts)
    {
        cloned.parts.push_back(clone(part));
    }
    return make_expression(cloned);
}

Expression_ptr ASTCloner::clone(const Range& expr)
{
    Range cloned;
    cloned.start = clone(expr.start);
    cloned.end = clone(expr.end);
    cloned.is_inclusive = expr.is_inclusive;
    return make_expression(cloned);
}

Expression_ptr ASTCloner::clone(const Identifier& expr)
{
    Identifier cloned;
    cloned.name = expr.name;
    cloned.must_be_captured = expr.must_be_captured;
    return make_expression(cloned);
}

Expression_ptr ASTCloner::clone(const MemberAccess& expr)
{
    MemberAccess cloned;
    cloned.owner = clone(expr.owner);
    cloned.member = clone(expr.member);
    cloned.member_index = expr.member_index;
    return make_expression(cloned);
}

Expression_ptr ASTCloner::clone(const Call& expr)
{
    Call cloned;
    cloned.callee = clone(expr.callee);
    cloned.angular_nodes = clone(expr.angular_nodes);
    cloned.arguments = clone(expr.arguments);
    cloned.owner_kind = expr.owner_kind;
    cloned.kind = expr.kind;
    cloned.overload_index = expr.overload_index;
    cloned.owner_type_id = expr.owner_type_id;
    return make_expression(cloned);
}

Expression_ptr ASTCloner::clone(const Pipe& expr)
{
    Pipe cloned;
    cloned.left = clone(expr.left);
    cloned.right = clone(expr.right);
    return make_expression(cloned);
}

Expression_ptr ASTCloner::clone(const Constructor& expr)
{
    Constructor cloned;
    cloned.constructible = clone(expr.constructible);
    cloned.angular_nodes = clone(expr.angular_nodes);
    cloned.arguments = clone(expr.arguments);
    return make_expression(cloned);
}

Expression_ptr ASTCloner::clone(const Prefix& expr)
{
    Prefix cloned;
    cloned.op = expr.op;
    cloned.operand = clone(expr.operand);
    return make_expression(cloned);
}

Expression_ptr ASTCloner::clone(const Infix& expr)
{
    Infix cloned;
    cloned.left = clone(expr.left);
    cloned.op = expr.op;
    cloned.right = clone(expr.right);
    return make_expression(cloned);
}

Expression_ptr ASTCloner::clone(const ListLiteral& expr)
{
    ListLiteral cloned;
    cloned.expressions = clone(expr.expressions);
    return make_expression(cloned);
}

Expression_ptr ASTCloner::clone(const TupleLiteral& expr)
{
    TupleLiteral cloned;
    cloned.expressions = clone(expr.expressions);
    return make_expression(cloned);
}

Expression_ptr ASTCloner::clone(const MapLiteral& expr)
{
    MapLiteral cloned;
    for (const auto& [key, value] : expr.pairs)
    {
        cloned.pairs[clone(key)] = clone(value);
    }
    return make_expression(cloned);
}

Expression_ptr ASTCloner::clone(const SetLiteral& expr)
{
    SetLiteral cloned;
    cloned.expressions = clone(expr.expressions);
    return make_expression(cloned);
}

Expression_ptr ASTCloner::clone(const Binding& expr)
{
    Binding cloned;
    cloned.lhs = clone(expr.lhs);
    cloned.rhs = clone(expr.rhs);
    cloned.declared_type = clone(expr.declared_type);
    cloned.is_mutable = expr.is_mutable;
    return make_expression(cloned);
}

Expression_ptr ASTCloner::clone(const Assignment& expr)
{
    Assignment cloned;
    cloned.lhs = clone(expr.lhs);
    cloned.rhs = clone(expr.rhs);
    return make_expression(cloned);
}

Expression_ptr ASTCloner::clone(const TernaryExpression& expr)
{
    TernaryExpression cloned;
    cloned.then_expr = clone(expr.then_expr);
    cloned.test = clone(expr.test);
    cloned.else_expr = clone(expr.else_expr);
    return make_expression(cloned);
}

// ============================================================================
// Type Node Cloning
// ============================================================================

TypeNode_ptr ASTCloner::clone(const NoneTypeNode&)
{
    return make_type_node(NoneTypeNode{});
}

TypeNode_ptr ASTCloner::clone(const LiteralTypeNode& type)
{
    LiteralTypeNode cloned;
    cloned.literal = clone(type.literal);
    return make_type_node(cloned);
}

TypeNode_ptr ASTCloner::clone(const TypeIdentifierNode& type)
{
    return make_type_node(TypeIdentifierNode{type.name});
}

TypeNode_ptr ASTCloner::clone(const ListTypeNode& type)
{
    ListTypeNode cloned;
    cloned.element_type = clone(type.element_type);
    return make_type_node(cloned);
}

TypeNode_ptr ASTCloner::clone(const TupleTypeNode& type)
{
    TupleTypeNode cloned;
    for (const auto& t : type.element_types)
    {
        cloned.element_types.push_back(clone(t));
    }
    return make_type_node(cloned);
}

TypeNode_ptr ASTCloner::clone(const SetTypeNode& type)
{
    SetTypeNode cloned;
    cloned.element_type = clone(type.element_type);
    return make_type_node(cloned);
}

TypeNode_ptr ASTCloner::clone(const MapTypeNode& type)
{
    MapTypeNode cloned;
    cloned.key_type = clone(type.key_type);
    cloned.value_type = clone(type.value_type);
    return make_type_node(cloned);
}

TypeNode_ptr ASTCloner::clone(const VariantTypeNode& type)
{
    VariantTypeNode cloned;
    for (const auto& t : type.options)
    {
        cloned.options.push_back(clone(t));
    }
    return make_type_node(cloned);
}

TypeNode_ptr ASTCloner::clone(const IntersectionTypeNode& type)
{
    IntersectionTypeNode cloned;
    for (const auto& t : type.types)
    {
        cloned.types.push_back(clone(t));
    }
    return make_type_node(cloned);
}

TypeNode_ptr ASTCloner::clone(const FunctionTypeNode& type)
{
    FunctionTypeNode cloned;
    for (const auto& t : type.input_types)
    {
        cloned.input_types.push_back(clone(t));
    }
    cloned.return_type = clone(type.return_type);
    return make_type_node(cloned);
}

TypeNode_ptr ASTCloner::clone(const AngularTypeNode& type)
{
    AngularTypeNode cloned;
    cloned.name = type.name;
    cloned.type_arguments = clone(type.type_arguments);

    for (const auto& t : type.type_arguments)
    {
        cloned.type_arguments.push_back(clone(t));
    }

    return make_type_node(cloned);
}

} // namespace Wasp
