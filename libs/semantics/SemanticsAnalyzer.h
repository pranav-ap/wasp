#include "SymbolScope.h"
#include "Workspace.h"
#include <vector>

namespace Wasp
{
class SemanticsAnalyzer
{
public:
    explicit SemanticsAnalyzer() = default;

    void run(std::vector<Module_ptr>& build_order);

private:
    SymbolScope_ptr current_scope;

    void enter_scope(ScopeType scope_type);
    void leave_scope();
};
} // namespace Wasp
