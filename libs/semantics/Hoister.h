#pragma once

#include "AST.h"
#include "Expression.h"
#include "Phase.h"
#include "Statement.h"

namespace Wasp
{

class Hoister : public Phase
{
public:
    explicit Hoister() : Phase()
    {
    }

private:
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

    void visit(ExpressionStatement& statement);

    void visit(Expression_ptr expression);

    void visit(Binding& binding);
    void visit(TernaryExpression& expr);
};

} // namespace Wasp
