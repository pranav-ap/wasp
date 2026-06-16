#include "TypeChecker.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <memory>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void TypeChecker::run(Module_ptr mod)
{
    current_module = mod;

    enter_scope(ScopeType::MODULE);
    visit(current_module->block);
    leave_scope();
}

void TypeChecker::enter_scope(ScopeType scope_type)
{
    auto new_scope = std::make_shared<SymbolScope>(
        scope_type,
        current_scope
    );

    current_scope = new_scope;
}

void TypeChecker::leave_scope()
{
    if (current_scope != nullptr)
    {
        current_scope = current_scope->enclosing_scope;
    }
}

} // namespace Wasp
