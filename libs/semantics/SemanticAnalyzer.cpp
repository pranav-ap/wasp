#include "SemanticAnalyzer.h"
#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Token.h"
#include "Workspace.h"

#include <ctime>
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

void SemanticAnalyzer::run(const std::vector<Module_ptr>& build_order)
{
    enter_scope(ScopeType::WORKSPACE);

    for (const auto mod : build_order)
    {
        current_module = mod;

        enter_scope(ScopeType::MODULE);

        StatementVector desugared_stmts;

        for (const auto& stmt : mod->stmts)
        {
            auto salted_stmt = salt->visit(stmt);
            desugared_stmts.push_back(salted_stmt);
        }

        hoist_statements(desugared_stmts);

        StatementVector updated_stmts;

        for (const auto& stmt : desugared_stmts)
        {
            visit(stmt);

            for (const auto& tmpl : pending_templates)
            {
                updated_stmts.push_back(tmpl);
            }

            pending_templates.clear();
            updated_stmts.push_back(stmt);
        }

        mod->stmts = std::move(updated_stmts);

        StringVector ordered_export_names = setup_ordered_export_names(mod);
        setup_exports(mod, ordered_export_names);
        leave_scope();
    }

    leave_scope();
}

void SemanticAnalyzer::visit(StatementVector& statements)
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
                    std::is_same_v<T, Import> ||
                    std::is_same_v<T, FieldDefinition> ||
                    std::is_same_v<T, OperatorDefinition> ||
                    std::is_same_v<T, Splitter>
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

Object_ptr SemanticAnalyzer::visit(Expression_ptr expr)
{
    Doctor::get().fatal_if_nullptr(expr, WaspStage::Semantics);

    if (expr->is<InterpolatedString>())
    {
        return desugar_interpolated_string(expr);
    }

    auto resolved_type = std::visit(
        [&](auto& node) -> Object_ptr
        {
            if constexpr (requires { visit(node); })
            {
                return visit(node);
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

    if (expr->is<MemberAccess>() && !expr->is_desugared)
    {
        desugar_member_access(expr);
    }

    return resolved_type;
}

Object_ptr SemanticAnalyzer::desugar_interpolated_string(
    const Expression_ptr& expr
)
{
    auto& interp = expr->as<InterpolatedString>();

    if (interp.parts.empty())
    {
        expr->data = StringLiteral{""};
        expr->is_desugared = true;
        return visit(expr);
    }

    if (interp.parts.size() == 1)
    {
        interp.parts.push_back(make_expression(StringLiteral{""}));
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
    expr->is_desugared = true;

    // Re-visit so Infix nodes are desugared into method calls
    return visit(expr);
}

void SemanticAnalyzer::desugar_member_access(Expression_ptr expr)
{
    auto& ma = expr->as<MemberAccess>();

    if (ma.is_enum_value)
    {
        EnumMember em(ma.enum_type_id, ma.enum_member_value);
        expr->data = em;
    }
}

} // namespace Wasp
