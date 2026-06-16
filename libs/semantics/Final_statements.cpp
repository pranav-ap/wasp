#include "AST.h"
#include "Final.h"
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

void Final::visit(Block& block)
{
    for (auto& statement : block.statements)
    {
        visit(statement);
    }
}

void Final::visit(Statement_ptr statement)
{
    std::visit(
        [&](auto& node)
        {
            if constexpr (requires { visit(node); })
            {
                visit(node);
            }
        },
        statement->data
    );
}

void Final::visit(Import&)
{
    // TODO: Implement
}

void Final::visit(Branch& stmt)
{
    enter_scope(ScopeType::BRANCH);
    visit(stmt.test);
    visit(stmt.block);
    leave_scope();
}

void Final::visit(SimpleLoop& stmt)
{
    enter_scope(ScopeType::LOOP);
    visit(stmt.test);
    visit(stmt.block);
    leave_scope();
}

void Final::visit(ForInLoop& stmt)
{
    enter_scope(ScopeType::LOOP);
    visit(stmt.lhs);
    visit(stmt.block);
    leave_scope();
}

void Final::visit(Return& stmt)
{
    if (stmt.expression.has_value())
    {
        visit(stmt.expression.value());
    }
}

void Final::visit(ExpressionStatement& stmt)
{
    visit(stmt.expression);
}

} // namespace Wasp
