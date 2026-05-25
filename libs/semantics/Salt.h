#pragma once

#include "AST.h"
#include "Expression.h"
#include "Statement.h"
#include "Workspace.h"

#include <memory>

namespace Wasp
{

class Salt
{
public:
    Salt(Workspace_ptr workspace) : workspace(workspace) {};

    StatementVector visit(const StatementVector statements);
    Statement_ptr run(const Statement_ptr statement);

private:
    Workspace_ptr workspace;

    Statement_ptr visit(ExpressionStatement& statement);

    Expression_ptr visit(const Expression_ptr expr);
    ExpressionVector visit(ExpressionVector expressions);

    Expression_ptr visit(IntegerLiteral& expr);
    Expression_ptr visit(FloatLiteral& expr);
    Expression_ptr visit(StringLiteral& expr);
    Expression_ptr visit(BooleanLiteral& expr);

    Expression_ptr visit(Prefix& expr);
    Expression_ptr visit(Infix& expr);
    Expression_ptr visit(Postfix& expr);

    Expression_ptr visit(InterpolatedString& expr);
};

using Salt_ptr = std::shared_ptr<Salt>;

} // namespace Wasp
