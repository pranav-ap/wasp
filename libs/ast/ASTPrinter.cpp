#include "ASTPrinter.h"
#include "AST.h"
#include "Expression.h"
#include "Statement.h"
#include "Token.h"
#include "TypeNode.h"
#include "nlohmann/json_fwd.hpp"

#include <nlohmann/json.hpp>

#include <string>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

// ============================================================================
// Helper
// ============================================================================

nlohmann::json ASTPrinter::make_node(
    const std::string& type,
    const nlohmann::json& data
)
{
    nlohmann::json node;
    node["type"] = type;
    if (!data.empty())
    {
        for (auto& [key, value] : data.items())
        {
            node[key] = value;
        }
    }
    return node;
}

// ============================================================================
// Print Entry Points
// ============================================================================

nlohmann::json ASTPrinter::print(const Statement_ptr& stmt)
{
    if (!stmt)
    {
        return nullptr;
    }

    return std::visit(
        overloaded{
            [this](const std::monostate&) -> nlohmann::json
            {
                return nullptr;
            },
            [this](const Import& s)
            {
                return print(s);
            },
            [this](const Block& s)
            {
                return print(s);
            },
            [this](const ExpressionStatement& s)
            {
                return print(s);
            },
            [this](const TypeAliasDefinition& s)
            {
                return print(s);
            },
            [this](const EnumDefinition& s)
            {
                return print(s);
            },
            [this](const FunctionDefinition& s)
            {
                return print(s);
            },
            [this](const MethodDefinition& s)
            {
                return print(s);
            },
            [this](const OperatorDefinition& s)
            {
                return print(s);
            },
            [this](const RecordDefinition& s)
            {
                return print(s);
            },
            [this](const ClassDefinition& s)
            {
                return print(s);
            },
            [this](const TraitDefinition& s)
            {
                return print(s);
            },
            [this](const PrimitiveDefinition& s)
            {
                return print(s);
            },
            [this](const Branch& s)
            {
                return print(s);
            },
            [this](const SimpleLoop& s)
            {
                return print(s);
            },
            [this](const ForInLoop& s)
            {
                return print(s);
            },
            [this](const LoopControl& s)
            {
                return print(s);
            },
            [this](const Return& s)
            {
                return print(s);
            },
            [this](const Pass& s)
            {
                return print(s);
            },
            [this](const Required& s)
            {
                return print(s);
            },
            [this](const Native& s)
            {
                return print(s);
            },
        },
        stmt->data
    );
}

nlohmann::json ASTPrinter::print(const Expression_ptr& expr)
{
    if (!expr)
    {
        return nullptr;
    }

    return std::visit(
        overloaded{
            [this](const std::monostate&) -> nlohmann::json
            {
                return nullptr;
            },
            [this](const IntegerLiteral& e)
            {
                return print(e);
            },
            [this](const FloatLiteral& e)
            {
                return print(e);
            },
            [this](const StringLiteral& e)
            {
                return print(e);
            },
            [this](const BooleanLiteral& e)
            {
                return print(e);
            },
            [this](const NoneLiteral& e)
            {
                return print(e);
            },
            [this](const InterpolatedString& e)
            {
                return print(e);
            },
            [this](const Range& e)
            {
                return print(e);
            },
            [this](const Identifier& e)
            {
                return print(e);
            },
            [this](const MemberAccess& e)
            {
                return print(e);
            },
            [this](const Call& e)
            {
                return print(e);
            },
            [this](const Pipe& e)
            {
                return print(e);
            },
            [this](const Constructor& e)
            {
                return print(e);
            },
            [this](const Prefix& e)
            {
                return print(e);
            },
            [this](const Infix& e)
            {
                return print(e);
            },
            [this](const ListLiteral& e)
            {
                return print(e);
            },
            [this](const TupleLiteral& e)
            {
                return print(e);
            },
            [this](const MapLiteral& e)
            {
                return print(e);
            },
            [this](const SetLiteral& e)
            {
                return print(e);
            },
            [this](const Binding& e)
            {
                return print(e);
            },
            [this](const Assignment& e)
            {
                return print(e);
            },
            [this](const TernaryExpression& e)
            {
                return print(e);
            },
        },
        expr->data
    );
}

nlohmann::json ASTPrinter::print(const TypeNode_ptr& type)
{
    if (!type)
    {
        return nullptr;
    }

    return std::visit(
        overloaded{
            [this](const std::monostate&) -> nlohmann::json
            {
                return nullptr;
            },
            [this](const NoneTypeNode& t)
            {
                return print(t);
            },
            [this](const LiteralTypeNode& t)
            {
                return print(t);
            },
            [this](const TypeIdentifierNode& t)
            {
                return print(t);
            },
            [this](const ListTypeNode& t)
            {
                return print(t);
            },
            [this](const TupleTypeNode& t)
            {
                return print(t);
            },
            [this](const SetTypeNode& t)
            {
                return print(t);
            },
            [this](const MapTypeNode& t)
            {
                return print(t);
            },
            [this](const VariantTypeNode& t)
            {
                return print(t);
            },
            [this](const IntersectionTypeNode& t)
            {
                return print(t);
            },
            [this](const FunctionTypeNode& t)
            {
                return print(t);
            },
            [this](const AngularTypeNode& t)
            {
                return print(t);
            },
        },
        type->data
    );
}

// ============================================================================
// Vectors
// ============================================================================

nlohmann::json ASTPrinter::print(const Block& block)
{
    nlohmann::json result;
    for (const auto& stmt : block.statements)
    {
        result.push_back(print(stmt));
    }
    return result;
}

nlohmann::json ASTPrinter::print(const FieldVector& fields)
{
    nlohmann::json result;
    for (const auto& field : fields)
    {
        nlohmann::json node;
        node["name"] = field.name;
        node["type"] = print(field.type);
        node["is_variadic"] = field.is_variadic;

        result.push_back(node);
    }
    return result;
}

nlohmann::json ASTPrinter::print(const FunctionDefinitionVector& funcs)
{
    nlohmann::json result;
    for (const auto& func : funcs)
    {
        result.push_back(print(func));
    }
    return result;
}

nlohmann::json ASTPrinter::print(const MethodDefinitionVector& methods)
{
    nlohmann::json result;
    for (const auto& method : methods)
    {
        result.push_back(print(method));
    }
    return result;
}

nlohmann::json ASTPrinter::print(const ExpressionVector& expressions)
{
    nlohmann::json result;
    for (const auto& expr : expressions)
    {
        result.push_back(print(expr));
    }
    return result;
}

nlohmann::json ASTPrinter::print(const TypeNodeVector& types)
{
    nlohmann::json result;
    for (const auto& type : types)
    {
        result.push_back(print(type));
    }
    return result;
}

// ============================================================================
// Statement Printing
// ============================================================================

nlohmann::json ASTPrinter::print(const Import& stmt)
{
    nlohmann::json node = make_node("Import");
    node["path"] = stmt.path;
    if (stmt.module_alias.has_value())
    {
        node["alias"] = stmt.module_alias.value();
    }
    node["expose_all"] = stmt.expose_all;
    if (!stmt.exposed_names.empty())
    {
        nlohmann::json names;
        for (const auto& pair : stmt.exposed_names)
        {
            nlohmann::json p;
            p["name"] = pair.name;
            if (pair.alias.has_value())
            {
                p["alias"] = pair.alias.value();
            }
            names.push_back(p);
        }
        node["exposed_names"] = names;
    }
    return node;
}

nlohmann::json ASTPrinter::print(const ExpressionStatement& stmt)
{
    return make_node(
        "ExpressionStatement",
        {{"expression", print(stmt.expression)}}
    );
}

nlohmann::json ASTPrinter::print(const TypeAliasDefinition& stmt)
{
    return make_node(
        "TypeAliasDefinition",
        {{"name", stmt.name},
         {"generics", print(stmt.generics)},
         {"ref_type", print(stmt.ref_type)}}
    );
}

nlohmann::json ASTPrinter::print(const EnumDefinition& stmt)
{
    nlohmann::json node = make_node("EnumDefinition");
    node["name"] = stmt.name;
    node["members"] = stmt.members;
    if (!stmt.nested_enums.empty())
    {
        nlohmann::json nested;
        for (const auto& e : stmt.nested_enums)
        {
            nested.push_back(print(e));
        }
        node["nested_enums"] = nested;
    }
    return node;
}

nlohmann::json ASTPrinter::print(const FunctionDefinition& stmt)
{
    return make_node(
        "FunctionDefinition",
        {{"name", stmt.name},
         {"generics", print(stmt.generics)},
         {"parameters", print(stmt.parameters)},
         {"return_type", print(stmt.return_type)},
         {"block", print(stmt.block)},
         {"is_pure", stmt.is_pure}}
    );
}

nlohmann::json ASTPrinter::print(const MethodDefinition& stmt)
{
    return make_node(
        "MethodDefinition",
        {{"name", stmt.name},
         {"parameters", print(stmt.parameters)},
         {"return_type", print(stmt.return_type)},
         {"block", print(stmt.block)},
         {"is_pure", stmt.is_pure},
         {"is_shared", stmt.is_shared}}
    );
}

nlohmann::json ASTPrinter::print(const OperatorDefinition& stmt)
{
    return make_node(
        "OperatorDefinition",
        {{"name", stmt.name},
         {"op_type", to_string(stmt.op_type)},
         {"fixity", to_string(stmt.fixity)},
         {"operands", print(stmt.operands)},
         {"return_type", print(stmt.return_type)},
         {"block", print(stmt.block)}}
    );
}

nlohmann::json ASTPrinter::print(const RecordDefinition& stmt)
{
    return make_node(
        "RecordDefinition",
        {{"name", stmt.name}, {"fields", print(stmt.fields)}}
    );
}

nlohmann::json ASTPrinter::print(const ClassDefinition& stmt)
{
    return make_node(
        "ClassDefinition",
        {{"name", stmt.name},
         {"generics", print(stmt.generics)},
         {"fields", print(stmt.fields)},
         {"methods", print(stmt.methods)},
         {"traits", print(stmt.traits)}}
    );
}

nlohmann::json ASTPrinter::print(const TraitDefinition& stmt)
{
    return make_node(
        "TraitDefinition",
        {{"name", stmt.name},
         {"generics", print(stmt.generics)},
         {"methods", print(stmt.methods)},
         {"traits", print(stmt.traits)}}
    );
}

nlohmann::json ASTPrinter::print(const PrimitiveDefinition& stmt)
{
    return make_node(
        "PrimitiveDefinition",
        {{"name", stmt.name}, {"methods", print(stmt.methods)}}
    );
}

nlohmann::json ASTPrinter::print(const Branch& stmt)
{
    nlohmann::json node = make_node("Branch");
    node["block"] = print(stmt.block);
    if (stmt.test)
    {
        node["test"] = print(stmt.test);
    }
    if (stmt.alternative)
    {
        node["alternative"] = print(stmt.alternative);
    }
    return node;
}

nlohmann::json ASTPrinter::print(const SimpleLoop& stmt)
{
    return make_node(
        "SimpleLoop",
        {{"style", to_string(stmt.style)},
         {"test", print(stmt.test)},
         {"block", print(stmt.block)}}
    );
}

nlohmann::json ASTPrinter::print(const ForInLoop& stmt)
{
    return make_node(
        "ForInLoop",
        {{"lhs_is_mutable", stmt.lhs_is_mutable},
         {"lhs", print(stmt.lhs)},
         {"iterable", print(stmt.iterable)},
         {"block", print(stmt.block)}}
    );
}

nlohmann::json ASTPrinter::print(const LoopControl& stmt)
{
    return make_node("LoopControl", {{"type", to_string(stmt.type)}});
}

nlohmann::json ASTPrinter::print(const Return& stmt)
{
    nlohmann::json node = make_node("Return");
    if (stmt.expression.has_value())
    {
        node["expression"] = print(stmt.expression.value());
    }
    return node;
}

nlohmann::json ASTPrinter::print(const Pass& stmt)
{
    return make_node("Pass");
}

nlohmann::json ASTPrinter::print(const Required& stmt)
{
    return make_node("Required");
}

nlohmann::json ASTPrinter::print(const Native& stmt)
{
    return make_node("Native");
}

// ============================================================================
// Expression Printing
// ============================================================================

nlohmann::json ASTPrinter::print(const IntegerLiteral& expr)
{
    return make_node("IntegerLiteral", {{"value", expr.value}});
}

nlohmann::json ASTPrinter::print(const FloatLiteral& expr)
{
    return make_node("FloatLiteral", {{"value", expr.value}});
}

nlohmann::json ASTPrinter::print(const StringLiteral& expr)
{
    return make_node("StringLiteral", {{"value", expr.value}});
}

nlohmann::json ASTPrinter::print(const BooleanLiteral& expr)
{
    return make_node("BooleanLiteral", {{"value", expr.value}});
}

nlohmann::json ASTPrinter::print(const NoneLiteral& expr)
{
    return make_node("NoneLiteral");
}

nlohmann::json ASTPrinter::print(const InterpolatedString& expr)
{
    return make_node("InterpolatedString", {{"parts", print(expr.parts)}});
}

nlohmann::json ASTPrinter::print(const Range& expr)
{
    return make_node(
        "Range",
        {{"start", print(expr.start)},
         {"end", print(expr.end)},
         {"is_inclusive", expr.is_inclusive}}
    );
}

nlohmann::json ASTPrinter::print(const Identifier& expr)
{
    nlohmann::json node = make_node("Identifier", {{"name", expr.name}});

    node["must_be_captured"] = expr.must_be_captured;
    return node;
}

nlohmann::json ASTPrinter::print(const MemberAccess& expr)
{
    return make_node(
        "MemberAccess",
        {{"owner", print(expr.owner)},
         {"member", print(expr.member)},
         {"member_index", expr.member_index}}
    );
}

nlohmann::json ASTPrinter::print(const Call& expr)
{
    nlohmann::json node = make_node("Call");
    node["callee"] = print(expr.callee);
    node["angular_nodes"] = print(expr.angular_nodes);
    node["arguments"] = print(expr.arguments);
    node["owner_name"] = expr.owner_name;
    node["overload_index"] = expr.overload_index;
    node["owner_type_id"] = expr.owner_type_id;
    return node;
}

nlohmann::json ASTPrinter::print(const Pipe& expr)
{
    return make_node(
        "Pipe",
        {{"left", print(expr.left)}, {"right", print(expr.right)}}
    );
}

nlohmann::json ASTPrinter::print(const Constructor& expr)
{
    return make_node(
        "Constructor",
        {{"constructible", print(expr.constructible)},
         {"angular_nodes", print(expr.angular_nodes)},
         {"arguments", print(expr.arguments)}}
    );
}

nlohmann::json ASTPrinter::print(const Prefix& expr)
{
    return make_node(
        "Prefix",
        {{"op", to_string(expr.op.type)}, {"operand", print(expr.operand)}}
    );
}

nlohmann::json ASTPrinter::print(const Infix& expr)
{
    return make_node(
        "Infix",
        {{"left", print(expr.left)},
         {"op", to_string(expr.op.type)},
         {"right", print(expr.right)}}
    );
}

nlohmann::json ASTPrinter::print(const ListLiteral& expr)
{
    return make_node("ListLiteral", {{"expressions", print(expr.expressions)}});
}

nlohmann::json ASTPrinter::print(const TupleLiteral& expr)
{
    return make_node("TupleLiteral", {{"expressions", print(expr.expressions)}});
}

nlohmann::json ASTPrinter::print(const MapLiteral& expr)
{
    nlohmann::json pairs;
    for (const auto& [key, value] : expr.pairs)
    {
        nlohmann::json pair;
        pair["key"] = print(key);
        pair["value"] = print(value);
        pairs.push_back(pair);
    }
    return make_node("MapLiteral", {{"pairs", pairs}});
}

nlohmann::json ASTPrinter::print(const SetLiteral& expr)
{
    return make_node("SetLiteral", {{"expressions", print(expr.expressions)}});
}

nlohmann::json ASTPrinter::print(const Binding& expr)
{
    return make_node(
        "Binding",
        {{"lhs", print(expr.lhs)},
         {"rhs", print(expr.rhs)},
         {"declared_type", print(expr.declared_type)},
         {"is_mutable", expr.is_mutable}}
    );
}

nlohmann::json ASTPrinter::print(const Assignment& expr)
{
    return make_node(
        "Assignment",
        {{"lhs", print(expr.lhs)}, {"rhs", print(expr.rhs)}}
    );
}

nlohmann::json ASTPrinter::print(const TernaryExpression& expr)
{
    return make_node(
        "TernaryExpression",
        {{"then_expr", print(expr.then_expr)},
         {"test", print(expr.test)},
         {"else_expr", print(expr.else_expr)}}
    );
}

// ============================================================================
// Type Printing
// ============================================================================

nlohmann::json ASTPrinter::print(const NoneTypeNode& type)
{
    return make_node("NoneType");
}

nlohmann::json ASTPrinter::print(const LiteralTypeNode& type)
{
    return make_node("LiteralType", {{"value", print(type.literal)}});
}

nlohmann::json ASTPrinter::print(const TypeIdentifierNode& type)
{
    return make_node("TypeIdentifier", {{"name", type.name}});
}

nlohmann::json ASTPrinter::print(const ListTypeNode& type)
{
    return make_node("ListType", {{"element_type", print(type.element_type)}});
}

nlohmann::json ASTPrinter::print(const TupleTypeNode& type)
{
    return make_node("TupleType", {{"element_types", print(type.element_types)}});
}

nlohmann::json ASTPrinter::print(const SetTypeNode& type)
{
    return make_node("SetType", {{"element_type", print(type.element_type)}});
}

nlohmann::json ASTPrinter::print(const MapTypeNode& type)
{
    return make_node(
        "MapType",
        {{"key_type", print(type.key_type)}, {"value_type", print(type.value_type)}}
    );
}

nlohmann::json ASTPrinter::print(const VariantTypeNode& type)
{
    return make_node("VariantType", {{"options", print(type.options)}});
}

nlohmann::json ASTPrinter::print(const IntersectionTypeNode& type)
{
    return make_node("IntersectionType", {{"types", print(type.types)}});
}

nlohmann::json ASTPrinter::print(const FunctionTypeNode& type)
{
    return make_node(
        "FunctionType",
        {{"input_types", print(type.parameter_types)},
         {"return_type", print(type.return_type)}}
    );
}

nlohmann::json ASTPrinter::print(const AngularTypeNode& type)
{
    nlohmann::json node = make_node(
        "AngularType",
        {{"name", type.name}, {"type_arguments", print(type.type_arguments)}}
    );

    return node;
}

} // namespace Wasp
