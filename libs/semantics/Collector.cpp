#include "Collector.h"
#include "AST.h"
#include "Doctor.h"
#include "Statement.h"
#include "SymbolScope.h"

#include <tuple>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

std::tuple<Statement_ptr, SymbolScope_ptr> Collector::get_tree(Symbol_ptr symbol)
{
    auto it = forest.find(symbol);

    Doctor::semantics().check(
        it != forest.end(),
        "No AST found for symbol '" + symbol->name + "'"
    );

    return {it->second, scope_forest[symbol]};
}

void Collector::visit(Import&)
{
    // TODO: Implement
}

} // namespace Wasp
