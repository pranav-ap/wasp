#include "SemanticsAnalyzer.h"
#include "Collector.h"
#include "Doctor.h"
#include "Final.h"
#include "Hoister.h"
#include "Refiner.h"
#include "SymbolScope.h"
#include "Transformer.h"
#include "Workspace.h"
#include "fmt/base.h"

#include <memory>
#include <vector>

namespace Wasp
{

void SemanticsAnalyzer::run(
    const std::vector<Module_ptr>& build_order
)
{
    enter_scope(ScopeType::WORKSPACE);

    Hoister hoisting;
    Collector collect;
    Transformer trans;
    Refiner refiner;
    Final fin;

    for (const auto& mod : build_order)
    {
        Doctor::get().start();

        hoisting.run(mod);
        collect.run(mod);

        trans.run(mod);

        auto ast_forest = trans.get_forest();
        auto scope_forest = trans.get_scope_forest();

        refiner.run(mod);

        fin.run(mod);

        double time_taken = Doctor::get().stop();

        fmt::print(
            stdout,
            "Module analyzed in {:.2f} seconds.\n",
            time_taken
        );
    }

    leave_scope();
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
