#include "SemanticAnalyzer.h"
#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
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
        saltly_expr->data
    );
    if (saltly_expr->is<Call>() && !saltly_expr->is_desugared)
    {
        desugar_call(saltly_expr);
    }

    return resolved_type;
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

    // Check if this is a method call (callable is a MemberAccess)
    if (call.callable->is<MemberAccess>())
    {
        auto& ma = call.callable->as<MemberAccess>();

        if (ma.is_trait_dispatch)
        {
            // Trait method call
            TraitMethodCall tmc;
            tmc.callable = call.callable;
            tmc.arguments = std::move(call.arguments);
            tmc.overload_index = call.overload_index;
            tmc.instance = ma.left;
            tmc.method_index = ma.member_index;
            tmc.trait_type_id = call.trait_type_id;

            expr->data = std::move(tmc);
        }
        else
        {
            // Class method call
            ClassMethodCall cmc;
            cmc.callable = call.callable;
            cmc.arguments = std::move(call.arguments);
            cmc.overload_index = call.overload_index;
            cmc.instance = ma.left;
            cmc.method_index = ma.member_index;

            expr->data = std::move(cmc);
        }
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

} // namespace Wasp
