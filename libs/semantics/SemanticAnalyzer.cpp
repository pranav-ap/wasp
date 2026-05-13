#include "SemanticAnalyzer.h"
#include "AST.h"
#include "Objects.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <ctime>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void SemanticAnalyzer::run(const std::vector<Module_ptr>& build_order)
{
    enter_scope(ScopeType::WORKSPACE);

    for (const auto mod : build_order)
    {
        current_module = mod;

        enter_scope(ScopeType::MODULE);
        hoist_statements(mod->stmts);

        for (const auto& stmt : mod->stmts)
        {
            visit(stmt);
        }

        StringVector ordered_export_names = setup_ordered_export_names(mod);
        setup_exports(mod, ordered_export_names);
        leave_scope();
        extract_module_type(mod);
    }

    leave_scope();
}

} // namespace Wasp
