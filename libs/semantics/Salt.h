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
    Statement_ptr visit(const Statement_ptr statement);
    Expression_ptr visit(const Expression_ptr expr);

private:
    StatementVector visit(const StatementVector statements);
    Statement_ptr visit(ExpressionStatement& statement);

    Statement_ptr visit(ClassDefinition& statement);
    Statement_ptr visit(TraitDefinition& statement);
    Statement_ptr visit(OperatorDefinition& statement);

    ExpressionVector visit(ExpressionVector expressions);

    Expression_ptr visit(InterpolatedString& expr);
};

using Salt_ptr = std::shared_ptr<Salt>;

} // namespace Wasp
