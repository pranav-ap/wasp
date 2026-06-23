#include "AST.h"
#include "Doctor.h"
#include "SemanticsAnalyzer.h"
#include "Symbol.h"
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

void SemanticsAnalyzer::enter_scope(ScopeType scope_type)
{
    auto new_scope = std::make_shared<SymbolScope>(scope_type, current_scope);
    current_scope = new_scope;
}

void SemanticsAnalyzer::leave_scope()
{
    if (current_scope != nullptr)
    {
        current_scope = current_scope->enclosing_scope;
    }
}

Statement_ptr SemanticsAnalyzer::get_tree(Symbol_ptr symbol)
{
    auto it = forest.find(symbol);

    Doctor::semantics().check(
        it != forest.end(),
        "No AST found for symbol '" + symbol->name + "'"
    );

    return it->second;
}

} // namespace Wasp
