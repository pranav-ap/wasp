#include "Compiler.h"
#include "Doctor.h"
#include "OpCode.h"
#include "Statement.h"
#include "Workspace.h"

#include <map>
#include <memory>
#include <string>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{
void Compiler::visit(SimpleImport& import_stmt)
{
    auto module_symbol = import_stmt.symbol;
    auto module_payload = module_symbol->get_payload_as<ModuleData>();

    int path_index = workspace->pool->allocate(module_payload.mod->file_path.string());

    // Tell the VM to execute this module and push a ModuleObject onto the stack
    emit(OpCode::IMPORT, path_index);

    Doctor::get().fatal_if_nullptr(
        import_stmt.symbol,
        WaspStage::Compiler,
        "Simple Import must have a resolved symbol.");

    id_to_name_map[module_symbol->id] = module_symbol->name;
    emit(OpCode::DEFINE_LOCAL, import_stmt.symbol->id);
}

void Compiler::visit(FromImport& import_stmt)
{
    auto module_symbol = import_stmt.symbol;
    auto module_payload = module_symbol->get_payload_as<ModuleData>();

    int path_index = workspace->pool->allocate(module_payload.mod->file_path.string());

    // Tell the VM to load the module onto the stack
    // Stack: [ Module ]
    emit(OpCode::IMPORT, path_index);

    // Iterate through every name the user requested (e.g., 'print_cats', 'print_dogs')
    for (const auto& imported_node : import_stmt.symbols)
    {
        Doctor::get().assert(
            !imported_node.resolved_symbols.empty(),
            WaspStage::Compiler,
            "FromImport node must have at least one resolved symbol.");

        // Iterate through ALL overloaded versions of that name found by the Semantic Analyzer
        for (const auto& overload_symbol : imported_node.resolved_symbols)
        {
            // Duplicate the module so GET_MEMBER doesn't consume our only copy
            // Stack: [ Module, Module ]
            emit(OpCode::DUP);

            int original_id;

            if (overload_symbol->payload_is<AliasData>())
            {
                original_id = overload_symbol->get_payload_as<AliasData>().target->id;
            }
            else
            {
                original_id = overload_symbol->id;
            }

            // Allocate the Integer ID to the constant pool instead of a string
            int member_id_index = workspace->pool->allocate(workspace->pool->allocate(original_id));

            // Fetch the function from the module!
            // Stack: [ Module, Function ]
            emit(OpCode::GET_MEMBER, member_id_index);

            // Save the function to the current scope using the NEW ALIAS ID
            id_to_name_map[overload_symbol->id] = overload_symbol->name;
            // Stack: [ Module ]
            emit(OpCode::DEFINE_LOCAL, overload_symbol->id);
        }
    }

    // Now that we have extracted all functions, throw away the module object!
    // Stack: []
    emit(OpCode::POP);
}

} // namespace Wasp
