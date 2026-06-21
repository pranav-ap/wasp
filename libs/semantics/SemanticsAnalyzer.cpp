#include "SemanticsAnalyzer.h"
#include "Collector.h"
#include "Doctor.h"
#include "Final.h"
#include "Hoister.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <memory>
#include <vector>

namespace Wasp
{

void SemanticsAnalyzer::run(const std::vector<Module_ptr>& build_order)
{
    Doctor::get().start();

    enter_scope(ScopeType::WORKSPACE);

    Hoister hoister;
    Collector collector;
    Final fin;

    for (const auto& mod : build_order)
    {
        hoister.run(mod);
        collector.run(mod);

        auto ast_forest = collector.get_forest();
        auto scope_forest = collector.get_scope_forest();

        fin.run(mod);

        mod->save_ast("semantics");
    }

    leave_scope();

    Doctor::get().stop();
}

void SemanticsAnalyzer::enter_scope(ScopeType scope_type)
{
    auto new_scope = std::make_shared<SymbolScope>(
        scope_type,
        current_scope
    );

    current_scope = new_scope;
}

void SemanticsAnalyzer::leave_scope()
{
    if (current_scope != nullptr)
    {
        current_scope = current_scope->enclosing_scope;
    }
}

} // namespace Wasp
