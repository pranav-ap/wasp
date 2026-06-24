#include "AST.h"
#include "SemanticsAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"

#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{
void SemanticsAnalyzer::visit(Block& block)
{
    hoist(block);
    collect(block);

    for (Statement_ptr& statement : block.statements)
    {
        visit(statement);
    }
}

void SemanticsAnalyzer::visit(Statement_ptr statement)
{
    std::visit(
        overloaded{[&](auto& node)
                   {
                       if constexpr (requires { visit(node); })
                       {
                           visit(node);
                       }
                   }},
        statement->data
    );
}

void SemanticsAnalyzer::visit(Branch& stmt)
{
    enter_scope(ScopeType::BRANCH);
    visit(stmt.test);
    visit(stmt.block);

    if (stmt.alternative != nullptr)
    {
        visit(stmt.alternative);
    }

    leave_scope();
}

void SemanticsAnalyzer::visit(SimpleLoop& stmt)
{
    enter_scope(ScopeType::LOOP);
    visit(stmt.test);
    visit(stmt.block);
    leave_scope();
}

void SemanticsAnalyzer::visit(ForInLoop& stmt)
{
    enter_scope(ScopeType::LOOP);
    visit(stmt.lhs);
    visit(stmt.block);
    leave_scope();
}

void SemanticsAnalyzer::visit(Return& stmt)
{
    if (stmt.expression.has_value())
    {
        visit(stmt.expression.value());
    }
}

void SemanticsAnalyzer::visit(ExpressionStatement& stmt)
{
    visit(stmt.expression);
}

} // namespace Wasp
