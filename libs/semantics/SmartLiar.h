#pragma once

#include "AST.h"
#include "Statement.h"
#include "Workspace.h"
#include <variant>
#include <vector>

namespace Wasp
{

using SmartLiarResult = std::variant<Statement_ptr, StatementVector>;

class SmartLiar
{
public:
    SmartLiar() = default;

    void run(const std::vector<Module_ptr>& build_order);
    StatementVector run(const StatementVector& statements);

private:
    SmartLiarResult visit(Statement_ptr stmt);
    SmartLiarResult visit(OperatorDefinition& def, Statement_ptr original_stmt);
    SmartLiarResult visit(ClassDefinition& def, Statement_ptr original_stmt);
    SmartLiarResult visit(TraitDefinition& def, Statement_ptr original_stmt);
};

} // namespace Wasp
