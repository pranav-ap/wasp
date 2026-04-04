#include "Compiler.h"
#include "Objects.h"
#include "Doctor.h"
#include "OpCode.h"
#include "Statement.h"

#include <string>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void Compiler::visit(ClassDefinition &stmt)
{

}

} // namespace Wasp
