#include "Compiler.h"
#include "Objects.h"
#include "OpCode.h"
#include "Statement.h"

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void Compiler::visit(ClassDefinition& class_definition)
{
    Object_ptr class_blueprint = class_definition.symbol->get_type();

    int const_id = workspace->pool->allocate_class_definition(class_blueprint);
    emit(OpCode::LOAD_CONST, const_id, "class " + class_definition.name);

    int physical_index = resolve_local(class_definition.symbol->id);

    if (physical_index == -1)
    {
        physical_index = static_cast<int>(locals.size());
        locals.push_back(class_definition.symbol);
    }

    emit(OpCode::SET_LOCAL, physical_index, "class " + class_definition.name);
}

} // namespace Wasp
