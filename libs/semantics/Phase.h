#pragma once

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
    Workspace_ptr workspace;
    Module_ptr current_module;
    SymbolScope_ptr current_scope;
    TypeSystem_ptr type_system;

public:
    explicit Phase(
        Workspace_ptr workspace,
        Module_ptr current_module,
        SymbolScope_ptr current_scope
    )
        : workspace(workspace), current_module(current_module),
          current_scope(current_scope), type_system(std::make_shared<TypeSystem>())
    {
    }

    virtual void run();

    void import_symbols(Import& statement);
    virtual void visit(Block& block) = 0;

    // Utils

    void enter_scope(ScopeType scope_type);
    void leave_scope();
};

} // namespace Wasp
