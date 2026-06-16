#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Type.h"
#include "TypeChecker.h"

#include <memory>
#include <string>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

Type_ptr TypeChecker::visit(Identifier& expr)
{
    auto symbol = current_scope->lookup(expr.name);

    Doctor::semantics().fatal_if_nullptr(
        symbol,
        "Undefined variable: " + expr.name
    );

    expr.symbol = symbol;

    return symbol->get_type();
}

Type_ptr TypeChecker::visit(MemberAccess& expr)
{
    Doctor::semantics().fatal("Not supported yet");
}

} // namespace Wasp
