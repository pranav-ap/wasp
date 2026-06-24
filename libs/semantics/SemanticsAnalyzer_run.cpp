#include "Doctor.h"
#include "SemanticsAnalyzer.h"
#include "Symbol.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <vector>

namespace Wasp
{

void SemanticsAnalyzer::run(std::vector<Module_ptr>& build_order)
{
    Doctor::semantics().start();

    enter_scope(ScopeType::WORKSPACE);

    for (Module_ptr& mod : build_order)
    {
        current_module = mod;

        enter_scope(ScopeType::MODULE);

        visit(mod->block);
        current_module->save_ast("semantics");
        init_module(current_module);

        leave_scope();
    }

    leave_scope();

    Doctor::semantics().stop();
}

} // namespace Wasp
