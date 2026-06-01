#pragma once

#include "AST.h"
#include "Statement.h"

#include <memory>

namespace Wasp
{

class Salt
{
public:
    Statement_ptr visit(const Statement_ptr statement);

private:
    StatementVector visit(const StatementVector statements);

    Statement_ptr visit(ClassDefinition& statement);
    Statement_ptr visit(TraitDefinition& statement);
    Statement_ptr visit(OperatorDefinition& statement);
};

using Salt_ptr = std::shared_ptr<Salt>;

} // namespace Wasp
