#include "Compiler.h"
#include "Doctor.h"
#include "OpCode.h"
#include "Statement.h"
#include "Workspace.h"

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

    Doctor::get().fatal_if_nullptr(
        import_stmt.symbol,
        WaspStage::Compiler,
        "Simple Import must have a resolved symbol"
    );

    auto module_payload = module_symbol->get_payload_as<ModuleData>();

    int module_index = workspace->get_module_index(module_payload.mod->absolute_filepath);

    // Tell the VM to execute this module and push a ModuleObject onto the stack
    emit(OpCode::IMPORT_MODULE, module_index, "module " + module_symbol->name);

    stack.push_back(module_symbol);
}

void Compiler::visit(FromImport& import_stmt)
{
    auto module_symbol = import_stmt.symbol;
    auto module_payload = module_symbol->get_payload_as<ModuleData>();

    int module_index = workspace->get_module_index(module_payload.mod->absolute_filepath);

    // Tell the VM to load the module onto the stack
    // Stack: [ Module ]
    emit(OpCode::IMPORT_MODULE, module_index, "module " + module_symbol->name);

    // Iterate through every name the user requested (e.g., 'print_cats', 'print_dogs')
    for (const auto& imported_node : import_stmt.symbols)
    {
        Doctor::get().assert(
            !imported_node.resolved_symbols.empty(),
            WaspStage::Compiler,
            "FromImport node must have at least one resolved symbol.");

        // Iterate through ALL overloaded versions of that name found by the Semantic Analyzer
        for (const auto& symbol : imported_node.resolved_symbols)
        {
            // Duplicate the module so GET_MEMBER doesn't consume our only copy
            // Stack: [ Module, Module ]
            emit(OpCode::DUP);

            int member_id = symbol->id;

            if (symbol->payload_is<AliasData>())
            {
                member_id = symbol->get_payload_as<AliasData>().target->id;
            }

            // Stack: [ Module, Function ]
            emit(OpCode::GET_MEMBER, member_id);

            // Stack: [ Module ]
            stack.push_back(symbol);
        }
    }

    // Now that we have extracted all functions, throw away the module object!
    // Stack: []
    emit(OpCode::POP);
}

} // namespace Wasp
