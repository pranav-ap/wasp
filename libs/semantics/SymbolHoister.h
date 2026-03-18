#pragma once

#include "SymbolScope.h"
#include "Workspace.h"

#include <memory>
#include <utility>
#include <vector>

namespace Wasp
{

class SymbolHoister
{
private:
    std::shared_ptr<Workspace> workspace;
    SymbolScope_ptr current_scope;

    void hoist(Module_ptr mod);

public:
    explicit SymbolHoister(std::shared_ptr<Workspace> workspace)
        : workspace(std::move(workspace)),
          current_scope(std::make_shared<SymbolScope>(ScopeType::MODULE))
    {
    }

    void run(const std::vector<Module_ptr>& build_order);
    void run(Module_ptr module);
};

} // namespace Wasp
