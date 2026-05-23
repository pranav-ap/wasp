#include "SmartLiar.h"
#include "AST.h"
#include "Doctor.h"
#include "Statement.h"
#include "Workspace.h"
#include <iterator>
#include <memory>
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

void SmartLiar::run(const std::vector<Module_ptr>& build_order)
{
    for (const auto& mod : build_order)
    {
        mod->stmts = run(mod->stmts);
    }
}

StatementVector SmartLiar::run(const StatementVector& statements)
{
    StatementVector desugared;

    for (const auto& stmt : statements)
    {
        Doctor::get().fatal_if_nullptr(stmt, WaspStage::Semantics);

        auto result = visit(stmt);

        if (std::holds_alternative<Statement_ptr>(result))
        {
            desugared.push_back(std::get<Statement_ptr>(result));
        }
        else
        {
            auto& vec = std::get<StatementVector>(result);
            desugared.insert(
                desugared.end(),
                std::make_move_iterator(vec.begin()),
                std::make_move_iterator(vec.end())
            );
        }
    }

    return desugared;
}

SmartLiarResult SmartLiar::visit(Statement_ptr stmt)
{
    return std::visit(
        overloaded{
            [&](OperatorDefinition& def)
            {
                return visit(def, stmt);
            },
            [&](ClassDefinition& def)
            {
                return visit(def, stmt);
            },
            [&](TraitDefinition& def)
            {
                return visit(def, stmt);
            },
            [&](auto&) -> SmartLiarResult
            {
                return stmt;
            }
        },
        stmt->data
    );
}

SmartLiarResult SmartLiar::visit(ClassDefinition& def, Statement_ptr original_stmt)
{
    def.members = run(def.members);
    return original_stmt;
}

SmartLiarResult SmartLiar::visit(TraitDefinition& def, Statement_ptr original_stmt)
{
    def.members = run(def.members);
    return original_stmt;
}

SmartLiarResult SmartLiar::visit(OperatorDefinition& def, Statement_ptr original_stmt)
{
    auto body = run(def.body);

    FunctionDefinition func_def(
        std::move(def.name),
        std::move(def.parameters),
        std::move(def.return_type),
        body,
        true,
        std::move(def.template_params)
    );

    func_def.symbol = def.symbol;
    func_def.group_symbol = def.group_symbol;
    func_def.parameter_symbols = std::move(def.parameter_symbols);
    func_def.context_symbol = def.context_symbol;

    return make_statement(func_def);
}

} // namespace Wasp
