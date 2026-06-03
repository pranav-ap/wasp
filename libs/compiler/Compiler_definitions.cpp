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
    // Type aliases are compile-time only, no runtime code needed
    // But we still need to allocate a local slot if referenced
    int physical_index = get_or_add_local_index(def.symbol);

    if (physical_index != -1)
    {
        // Load none as placeholder for compile-time only value
        emit(OpCode::LOAD_NONE);
        emit(
            OpCode::SET_LOCAL,
            physical_index,
            "compile-time alias: " + def.name
        );
    }
}

void Compiler::visit(EnumDefinition& def)
{
    // Enum definitions are compile-time only
    // Values are inlined as integers, no runtime enum objects
    int physical_index = get_or_add_local_index(def.symbol);

    if (physical_index != -1)
    {
        // Load none as placeholder for compile-time only value
        emit(OpCode::LOAD_NONE);
        emit(
            OpCode::SET_LOCAL,
            physical_index,
            "compile-time enum: " + def.name
        );
    }
}

} // namespace Wasp
