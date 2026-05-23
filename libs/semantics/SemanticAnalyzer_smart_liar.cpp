#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "Token.h"
#include "Workspace.h"

#include <cctype>
#include <ctime>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void SemanticAnalyzer::visit(std::vector<Statement_ptr>& statements)
{
    hoist_statements(statements);

    for (const auto& stmt : statements)
    {
        visit(stmt);
    }
}

void SemanticAnalyzer::visit(const Statement_ptr statement)
{
    Doctor::get().fatal_if_nullptr(statement, WaspStage::Semantics);

    std::visit(
        overloaded{
            [&](std::monostate&)
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Unhandled Statement in Semantic Analyzer!"
                );
            },
            [&](auto& stat)
            {
                using T = std::decay_t<decltype(stat)>;

                if constexpr (
                    std::is_same_v<T, Import> || std::is_same_v<T, MethodDefinition> ||
                    std::is_same_v<T, FieldDefinition>
                )
                {
                    return;
                }
                else
                {
                    this->visit(stat);
                }
            }
        },
        statement->data
    );
}

void SemanticAnalyzer::visit(ExpressionStatement& statement)
{
    visit(statement.expression);
}

ObjectVector SemanticAnalyzer::visit(ExpressionVector expressions)
{
    ObjectVector computed_types;
    computed_types.reserve(expressions.size());
    for (const auto& expr : expressions)
    {
        computed_types.push_back(visit(expr));
    }
    return computed_types;
}

Object_ptr SemanticAnalyzer::visit(const Expression_ptr expr)
{
    Doctor::get().fatal_if_nullptr(expr, WaspStage::Semantics);

    Object_ptr resolved_type = std::visit(
        [&](auto& node) -> Object_ptr
        {
            if constexpr (requires { this->visit(node); })
            {
                return this->visit(node);
            }
            else
            {
                Doctor::get().fatal(
                    WaspStage::Semantics,
                    "Unhandled Expression"
                );
            }
        },
        expr->data
    );

    if (expr->is_desugared)
    {
        return resolved_type;
    }

    if (expr->is<IntegerLiteral>())
    {
        desugar_literal(expr, "int");

        auto symbol = current_scope->lookup("int");
        Doctor::get().fatal_if_nullptr(
            symbol,
            WaspStage::Semantics,
            "Runtime class symbol 'int' not found."
        );

        return symbol->get_type();
    }
    else if (expr->is<FloatLiteral>())
    {
        desugar_literal(expr, "float");

        auto symbol = current_scope->lookup("float");
        Doctor::get().fatal_if_nullptr(
            symbol,
            WaspStage::Semantics,
            "Runtime class symbol 'float' not found."
        );

        return symbol->get_type();
    }
    else if (expr->is<StringLiteral>())
    {
        desugar_literal(expr, "str");

        auto symbol = current_scope->lookup("str");
        Doctor::get().fatal_if_nullptr(
            symbol,
            WaspStage::Semantics,
            "Runtime class symbol 'str' not found."
        );

        return symbol->get_type();
    }
    else if (expr->is<BooleanLiteral>())
    {
        desugar_literal(expr, "bool");

        auto symbol = current_scope->lookup("bool");
        Doctor::get().fatal_if_nullptr(
            symbol,
            WaspStage::Semantics,
            "Runtime class symbol 'bool' not found."
        );

        return symbol->get_type();
    }
    else if (expr->is<InterpolatedString>())
    {
        return desugar_interpolated_string(expr);
    }

    // Desugar Overloaded Operators into standard function calls

    if (expr->is<Infix>())
    {
        auto& infix = expr->as<Infix>();
        if (infix.symbol)
        {
            Symbol_ptr sym = infix.symbol;
            int idx = infix.overload_index;
            ExpressionVector args = {infix.left, infix.right};
            desugar_overloaded_operator(expr, sym, idx, args);
        }
    }
    else if (expr->is<Prefix>())
    {
        auto& prefix = expr->as<Prefix>();
        if (prefix.symbol)
        {
            Symbol_ptr sym = prefix.symbol;
            int idx = prefix.overload_index;
            ExpressionVector args = {prefix.operand};
            desugar_overloaded_operator(expr, sym, idx, args);
        }
    }
    else if (expr->is<Postfix>())
    {
        auto& postfix = expr->as<Postfix>();
        if (postfix.symbol)
        {
            Symbol_ptr sym = postfix.symbol;
            int idx = postfix.overload_index;
            ExpressionVector args = {postfix.operand};
            desugar_overloaded_operator(expr, sym, idx, args);
        }
    }

    if (expr->is<Call>())
    {
        desugar_call(expr);
    }
    else if (expr->is<MemberAccess>())
    {
        desugar_member_access(expr);
    }

    return resolved_type;
}

void SemanticAnalyzer::desugar_member_access(Expression_ptr expr)
{
    Doctor::get().fatal_if_nullptr(expr, WaspStage::Semantics);

    Doctor::get().assert(
        expr->is<MemberAccess>(),
        WaspStage::Semantics,
        "Expected a MemberAccess expression for desugar_member_access."
    );

    auto& ma = expr->as<MemberAccess>();

    if (!ma.is_enum_value)
    {
        return;
    }

    Symbol_ptr alias_symbol = current_scope->lookup("int");
    Doctor::get().fatal_if_nullptr(
        alias_symbol,
        WaspStage::Semantics,
        "Type alias 'int' not found for enum member desugaring."
    );

    Object_ptr actual_type = unwrap_type_alias(alias_symbol->get_type());
    auto class_type = actual_type->as<ClassType_ptr>();

    Symbol_ptr class_symbol = current_scope->lookup(class_type->name);

    Doctor::get().fatal_if_nullptr(
        class_symbol,
        WaspStage::Semantics,
        "Runtime class symbol '" + class_type->name + "' not found."
    );

    Identifier id(class_symbol->name);
    id.symbol = class_symbol;
    id.must_be_captured = class_symbol->should_be_captured(current_scope->get_closure_depth());

    auto id_node = make_expression(id, true);
    bind_identifier(id_node->as<Identifier>(), class_symbol);

    auto literal_child = make_expression(IntegerLiteral{ma.enum_member_value}, true);

    expr->data = Constructor(id_node, {literal_child});
    expr->is_desugared = true;
}

void SemanticAnalyzer::desugar_literal(
    const Expression_ptr& expr,
    const std::string& type_alias_name
)
{
    // type int
    Symbol_ptr alias_symbol = current_scope->lookup(type_alias_name);
    Doctor::get().fatal_if_nullptr(
        alias_symbol,
        WaspStage::Semantics,
        "Type alias '" + type_alias_name + "' not found for literal desugaring."
    );

    Object_ptr actual_type = unwrap_type_alias(alias_symbol->get_type());
    auto class_type = actual_type->as<ClassType_ptr>();

    // class Int
    Symbol_ptr class_symbol = current_scope->lookup(class_type->name);

    Doctor::get().fatal_if_nullptr(
        class_symbol,
        WaspStage::Semantics,
        "Runtime class symbol '" + class_type->name + "' not found."
    );

    Identifier id(class_symbol->name);
    id.symbol = class_symbol;
    id.must_be_captured = class_symbol->should_be_captured(current_scope->get_closure_depth());

    auto id_node = make_expression(id, true);
    bind_identifier(id_node->as<Identifier>(), class_symbol);

    auto literal_child = make_expression(std::move(expr->data), true);

    expr->data = Constructor(id_node, {literal_child});
    expr->is_desugared = true;
}

void SemanticAnalyzer::desugar_call(Expression_ptr expr)
{
    Doctor::get().fatal_if_nullptr(expr, WaspStage::Semantics);

    Doctor::get().assert(
        expr->is<Call>(),
        WaspStage::Semantics,
        "Expected a Call expression for desugar_call."
    );

    Call call = expr->as<Call>();

    if (call.is_method_call)
    {
        auto& ma = call.callable->as<MemberAccess>();

        MethodCall mc;
        mc.callable = call.callable;
        mc.arguments = std::move(call.arguments);
        mc.overload_index = call.overload_index;

        mc.instance = ma.left;
        mc.method_index = ma.member_index;
        mc.is_trait_dispatch = ma.is_trait_dispatch;

        expr->data = std::move(mc);
    }
    else
    {
        // Standard function call
        FunctionCall fc;
        fc.callable = call.callable;
        fc.arguments = std::move(call.arguments);
        fc.overload_index = call.overload_index;

        expr->data = std::move(fc);
    }

    expr->is_desugared = true;
}

Object_ptr SemanticAnalyzer::desugar_interpolated_string(const Expression_ptr& expr)
{
    auto& interp = expr->as<InterpolatedString>();

    if (interp.parts.empty())
    {
        expr->data = StringLiteral{""};
        return visit(expr);
    }

    Expression_ptr root = interp.parts[0];

    for (size_t i = 1; i < interp.parts.size(); ++i)
    {
        Token plus_token;
        plus_token.type = TokenType::PLUS;
        plus_token.lexeme = "+";

        root = make_expression(Infix{root, plus_token, interp.parts[i]});
    }

    expr->data = std::move(root->data);

    // Re-visit so Infix nodes are desugared into method calls
    return visit(expr);
}

void SemanticAnalyzer::desugar_overloaded_operator(
    const Expression_ptr& expr,
    Symbol_ptr operator_symbol,
    int overload_index,
    const ExpressionVector& arguments
)
{
    auto id_node = make_expression(Identifier(operator_symbol->name), true);

    bind_identifier(id_node->as<Identifier>(), operator_symbol);

    FunctionCall func_call;
    func_call.callable = id_node;
    func_call.arguments = arguments;
    func_call.overload_index = overload_index;

    expr->data = std::move(func_call);
    expr->is_desugared = true;
}

} // namespace Wasp
