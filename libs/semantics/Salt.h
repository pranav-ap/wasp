#pragma once

#include "AST.h"
#include "Expression.h"
#include "Statement.h"

#include <memory>

namespace Wasp
{

class Salt
{
public:
    Statement_ptr run(const Statement_ptr statement);

private:
    StatementVector visit(const StatementVector statements);
    Statement_ptr visit(ExpressionStatement& statement);

    Statement_ptr visit(ClassDefinition& statement);
    Statement_ptr visit(TraitDefinition& statement);
    Statement_ptr visit(OperatorDefinition& statement);

    Expression_ptr visit(const Expression_ptr expr);
    ExpressionVector visit(ExpressionVector expressions);

    Expression_ptr visit(InterpolatedString& expr);

    Expression_ptr visit(Prefix& expr);
    Expression_ptr visit(Infix& expr);
    Expression_ptr visit(Postfix& expr);
};

using Salt_ptr = std::shared_ptr<Salt>;

} // namespace Wasp
