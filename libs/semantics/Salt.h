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
    StatementVector visit(const StatementVector statements);
    Statement_ptr run(const Statement_ptr statement);

private:
    Statement_ptr visit(ExpressionStatement& statement);

    Expression_ptr visit(const Expression_ptr expr);
    ExpressionVector visit(ExpressionVector expressions);

    Expression_ptr visit(InterpolatedString& expr);

    Expression_ptr visit(Prefix& expr);
    Expression_ptr visit(Infix& expr);
    Expression_ptr visit(Postfix& expr);
};

using Salt_ptr = std::shared_ptr<Salt>;

} // namespace Wasp
