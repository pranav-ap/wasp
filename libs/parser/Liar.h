#pragma once

#include "AST.h"
#include "Statement.h"
#include <string>
#include <variant>

namespace Wasp
{

using LiarResult = std::variant<Statement_ptr, StatementVector>;

class Liar
{
public:
    Liar() = default;

    StatementVector run(const StatementVector& statements);

private:
    LiarResult visit(Statement_ptr stmt);
    LiarResult visit(ClassDefinition& def);

    Statement_ptr transform_method_to_function(
        const std::string& class_name,
        MethodDefinition& method
    );
};

} // namespace Wasp
