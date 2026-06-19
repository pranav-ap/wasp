#pragma once

#include "AST.h"
#include "Expression.h"
#include "Phase.h"
#include "Statement.h"
#include "Type.h"

#include <vector>

namespace Wasp
{

class Salter : public Phase
{
public:
    explicit Salter() : Phase()
    {
    }

private:
    Block salted_block;

private:
    // Statements

    void visit(Block& block);
    void visit(Statement_ptr statement);

    void visit(Import& statement);

    void visit(FunctionDefinition& statement);
    void visit(MethodDefinition& statement);
    void visit(OperatorDefinition& statement);
    void visit(ClassDefinition& statement);
    void visit(TraitDefinition& statement);
    void visit(PrimitiveDefinition& statement);
    void visit(EnumDefinition& statement);
    void visit(TypeAliasDefinition& statement);

    void visit(Branch& statement);
    void visit(SimpleLoop& statement);
    void visit(ForInLoop& statement);

    void visit(Return& statement);

    void visit(ExpressionStatement& statement);

    // Expressions

    Expression_ptr visit(Expression_ptr expression);

    Expression_ptr visit(InterpolatedString& expr);
};

} // namespace Wasp
