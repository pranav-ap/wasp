#include "SymbolScope.h"
#include "Workspace.h"
#include <vector>

namespace Wasp
{
class SemanticsAnalyzer
{
public:
    explicit SemanticsAnalyzer(Workspace_ptr workspace) : workspace(workspace)
    {
    }

    void run(std::vector<Module_ptr>& build_order);

private:
    SymbolScope_ptr current_scope;
    Workspace_ptr workspace;

    void init_module(Module_ptr current_module);

    void enter_scope(ScopeType scope_type);
    void leave_scope();
};
} // namespace Wasp
