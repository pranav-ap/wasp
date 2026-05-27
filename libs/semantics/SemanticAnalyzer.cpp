#include "SemanticAnalyzer.h"
#include "AST.h"
#include "Doctor.h"
#include "Objects.h"
#include "Statement.h"
#include "SymbolScope.h"
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
        hoist_statements(mod->stmts);

        StatementVector updated_stmts;

        for (const auto& stmt : mod->stmts)
        {
            auto saltly_stmt = salt->visit(stmt);
            visit(saltly_stmt);

            for (const auto& tmpl : pending_templates)
            {
                updated_stmts.push_back(tmpl);
            }

            pending_templates.clear();

            updated_stmts.push_back(saltly_stmt);
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
        auto saltly_stmt = salt->visit(stmt);
        visit(saltly_stmt);
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
                    std::is_same_v<T, MethodDefinition> ||
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

Object_ptr SemanticAnalyzer::visit(const Expression_ptr expr)
{
    Doctor::get().fatal_if_nullptr(expr, WaspStage::Semantics);

    auto saltly_expr = salt->visit(expr);
    visit(saltly_expr);

    return std::visit(
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
        saltly_expr->data
    );
}

} // namespace Wasp
