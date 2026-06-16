#include "TypeChecker.h"
#include "AST.h"
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

void TypeChecker::visit(Block& block)
{
    for (auto& statement : block.statements)
    {
        visit(statement);
    }
}

void TypeChecker::visit(Statement_ptr statement)
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

void TypeChecker::visit(Import&)
{
    // TODO: Implement
}

void TypeChecker::visit(Branch& stmt)
{
    enter_scope(ScopeType::BRANCH);
    visit(stmt.test);
    visit(stmt.block);
    leave_scope();
}

void TypeChecker::visit(SimpleLoop& stmt)
{
    enter_scope(ScopeType::LOOP);
    visit(stmt.test);
    visit(stmt.block);
    leave_scope();
}

void TypeChecker::visit(ForInLoop& stmt)
{
    enter_scope(ScopeType::LOOP);
    visit(stmt.lhs);
    visit(stmt.block);
    leave_scope();
}

void TypeChecker::visit(Return& stmt)
{
    if (stmt.expression.has_value())
    {
        visit(stmt.expression.value());
    }
}

void TypeChecker::visit(ExpressionStatement& stmt)
{
    visit(stmt.expression);
}

} // namespace Wasp
