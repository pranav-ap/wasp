#include "Compiler.h"
#include "OpCode.h"
#include "Statement.h"


template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{
void Compiler::visit(TypeAliasDefinition& def)
{
    int physical_index = get_or_add_local_index(def.symbol);

    if (physical_index != -1)
    {
        emit(OpCode::LOAD_NONE);
        emit(OpCode::SET_LOCAL, physical_index, "compile-time alias: " + def.name);
    }
}

void Compiler::visit(EnumDefinition& def)
{
    int physical_index = get_or_add_local_index(def.symbol);

    if (physical_index != -1)
    {
        emit(OpCode::LOAD_NONE);
        emit(OpCode::SET_LOCAL, physical_index, "compile-time enum: " + def.name);
    }
}

} // namespace Wasp
