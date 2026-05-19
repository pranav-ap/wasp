#include "Compiler.h"
#include "AST.h"
#include "Doctor.h"
#include "Objects.h"
#include "OpCode.h"
#include "Statement.h"
#include "Workspace.h"

#include <algorithm>
#include <memory>
#include <string>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void Compiler::emit_exports()
{
    auto mod = workspace->get_module(module_path);

    if (mod == nullptr)
    {
        emit(OpCode::EXIT_MODULE, 0);
        return;
    }

    Doctor::get().fatal_if_nullptr(mod, WaspStage::Compiler);

    int export_count = 0;

    for (const auto& exported_symbol : mod->exports)
    {
        int stack_index = resolve_local(exported_symbol->id);

        if (stack_index == -1)
        {
            continue;
        }

        emit(OpCode::GET_LOCAL, stack_index, exported_symbol->name);
        export_count++;
    }

    emit(OpCode::EXIT_MODULE, export_count);
}

void Compiler::visit(Import& import_stmt)
{
    Doctor::get().fatal_if_nullptr(
        import_stmt.symbol,
        WaspStage::Compiler,
        "Import statement must have a resolved symbol"
    );

    auto unresolved_module_symbol = import_stmt.symbol;
    Doctor::get().fatal_if_nullptr(unresolved_module_symbol, WaspStage::Compiler);

    auto resolved_module_symbol = unresolved_module_symbol->resolve();
    Doctor::get().fatal_if_nullptr(resolved_module_symbol, WaspStage::Compiler);

    auto module_payload = resolved_module_symbol->get_payload_as<ModuleData>();
    int module_index = workspace->get_module_index(module_payload.mod->absolute_filepath);

    emit(OpCode::IMPORT_MODULE, module_index, "module " + unresolved_module_symbol->name);
    int module_slot = get_or_add_local_index(unresolved_module_symbol);

    // Expose explicit symbols
    for (const auto& pair : import_stmt.exposed_symbols)
    {
        Doctor::get().fatal_if_nullptr(pair.symbol, WaspStage::Compiler);
        auto target_symbol = pair.symbol->resolve();

        emit(OpCode::GET_LOCAL, module_slot, "load module " + resolved_module_symbol->name);

        int member_index = module_payload.mod->get_member_index(target_symbol->name);
        emit(OpCode::GET_MEMBER, member_index, "expose " + target_symbol->name);

        get_or_add_local_index(pair.symbol);
    }

    // Expose wildcard symbols
    if (import_stmt.expose_all)
    {
        for (const auto& exported_symbol : module_payload.mod->exports)
        {
            if (std::find(
                    import_stmt.excluded_symbols.begin(),
                    import_stmt.excluded_symbols.end(),
                    exported_symbol->name
                ) == import_stmt.excluded_symbols.end())
            {
                emit(OpCode::GET_LOCAL, module_slot, "load module " + resolved_module_symbol->name);

                int member_index = module_payload.mod->get_member_index(exported_symbol->name);
                emit(OpCode::GET_MEMBER, member_index, "expose * " + exported_symbol->name);

                get_or_add_local_index(exported_symbol);
            }
        }
    }
}

} // namespace Wasp
