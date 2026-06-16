#include "AST.h"
#include "Expression.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"

namespace Wasp
{

class Hoister
{
public:
    explicit Hoister() : current_scope(nullptr)
    {
    }

    void run(Module_ptr mod);

private:
    Module_ptr current_module;
    SymbolScope_ptr current_scope;

    void enter_scope(ScopeType scope_type);
    void leave_scope();

    void visit(Block& block);
    void visit(Statement_ptr statement);

    void visit(Import& statement);

    void visit(FunctionDefinition& statement);
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
