#include "SemanticsAnalyzer.h"
#include "Doctor.h"
#include "Hoisting.h"
#include "SymbolScope.h"
#include "TypeChecker.h"
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

    Hoisting hoisting;
    TypeChecker type_checker;

    for (const auto& mod : build_order)
    {
        Doctor::get().start();

        hoisting.run(mod);
        type_checker.run(mod);

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
