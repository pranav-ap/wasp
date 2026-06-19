#pragma once

#include "AST.h"
#include "Expression.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"
#include <vector>

namespace Wasp
{

class Salter
{
public:
    explicit Salter()
    {
    }

    void run(const std::vector<Module_ptr>& build_order);

private:
    Module_ptr current_module;
    SymbolScope_ptr current_scope;

private:
    // Statements

    Statement_ptr visit(Block& block);
    Block salt(Block& block);

    Statement_ptr visit(Statement_ptr statement);

    Statement_ptr visit(ExpressionStatement& statement);

    // Expressions

    Expression_ptr visit(Expression_ptr expression);

    Expression_ptr visit(InterpolatedString& expr);

    // Utils

    void enter_scope(ScopeType scope_type);
    void leave_scope();
};

} // namespace Wasp
