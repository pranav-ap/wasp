#include "Doctor.h"
#include "Expression.h"
#include "Type.h"
#include "TypeChecker.h"

#include <string>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

Type_ptr TypeChecker::visit(Prefix& expr)
{
    Doctor::semantics().fatal("Not supported yet");
}

Type_ptr TypeChecker::visit(Infix& expr)
{
    Doctor::semantics().fatal("Not supported yet");
}

} // namespace Wasp
