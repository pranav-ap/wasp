#pragma once

#include "AST.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "TypeSystem.h"
#include "Workspace.h"

#include <memory>

namespace Wasp
{

class Phase
{
public:
    explicit Phase()
        : current_scope(nullptr), type_system(std::make_shared<TypeSystem>())
    {
    }

    void run(Module_ptr mod);

    Module_ptr current_module;
    SymbolScope_ptr current_scope;
    TypeSystem_ptr type_system;

    // Statements

    void visit(Block& block);
    void visit(Statement_ptr statement);

    // Utils

    void enter_scope(ScopeType scope_type);
    void leave_scope();
};

} // namespace Wasp
