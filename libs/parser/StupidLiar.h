#pragma once

#include "AST.h"
#include "Statement.h"
#include <string>
#include <variant>

namespace Wasp
{

using StupidLiarResult = std::variant<Statement_ptr, StatementVector>;

class StupidLiar
{
public:
    StupidLiar() = default;

    StatementVector run(const StatementVector& statements);

private:
    StupidLiarResult visit(Statement_ptr stmt);
    StupidLiarResult visit(ClassDefinition& def);

    Statement_ptr transform_method_to_function(
        const std::string& class_name,
        MethodDefinition& method
    );
};

} // namespace Wasp
