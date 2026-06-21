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

    Statement_ptr visit(Block&);
    Block salt(Block&);
    Block salt(TypeDefinition&);

    Statement_ptr visit(Statement_ptr);

    Statement_ptr visit(ExpressionStatement&);

    Statement_ptr visit(FunctionDefinition&);
    Statement_ptr visit(OperatorDefinition&);

    Statement_ptr visit(ClassDefinition& statement);
    Statement_ptr visit(TraitDefinition& statement);
    Statement_ptr visit(PrimitiveDefinition& statement);

    Statement_ptr visit(Branch& statement);
    Statement_ptr visit(SimpleLoop& statement);
    Statement_ptr visit(ForInLoop& statement);

    Statement_ptr visit(Return& statement);

    // Expressions

    Expression_ptr visit(Expression_ptr);

    Expression_ptr visit(InterpolatedString&);
    Expression_ptr visit(Call& call);

    // Utils

    void enter_scope(ScopeType);
    void leave_scope();

    bool is_template(const Statement_ptr) const;
};

} // namespace Wasp
