#include "Compiler.h"
#include "Objects.h"
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

// -----------------------------------------------------------------------
// Function Definition
// ------------------------------------------------------------------------

void Compiler::visit(ClassDefinition& class_definition)
{
    std::vector<std::string> field_names;

    for (const auto& [member_name, type_annotation] : class_definition.members)
    {
        field_names.push_back(member_name);
    }

    // create cls

    ClassDefinitionObject cls{class_definition.name, {}, class_definition.traits};

    int const_id = workspace->pool->allocate_class_definition(make_object(cls));

    emit(OpCode::LOAD_CONST, const_id, "class " + class_definition.name);

    int physical_index = resolve_local(class_definition.symbol->id);

    if (physical_index == -1)
    {
        physical_index = static_cast<int>(locals.size());
        locals.push_back(class_definition.symbol);
    }

    emit(OpCode::SET_LOCAL, physical_index, "bind class " + class_definition.name);
}
} // namespace Wasp
