#include "AST.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Type.h"
#include "TypeSystem.h"
#include "Workspace.h"

#include <map>
#include <memory>

namespace Wasp
{

class Refiner
{
public:
    explicit Refiner()
        : current_scope(nullptr), type_system(std::make_shared<TypeSystem>())
    {
    }

    void run(Module_ptr mod);


private:
    Module_ptr current_module;
    SymbolScope_ptr current_scope;
    TypeSystem_ptr type_system;

    // Statements

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

    // Utils

    void enter_scope(ScopeType scope_type);
    void leave_scope();
};

} // namespace Wasp
