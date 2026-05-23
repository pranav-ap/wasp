#include "Compiler.h"
#include "AST.h"
#include "Doctor.h"
#include "Objects.h"
#include "OpCode.h"
#include "Statement.h"
#include "Workspace.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

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

    struct ExposedMember
    {
        int index;
        Symbol_ptr symbol;
    };

    std::vector<ExposedMember> members_to_expose;

    // Explicit symbols
    for (const auto& pair : import_stmt.exposed_symbols)
    {
        auto target_symbol = pair.symbol->resolve();
        int member_index = module_payload.mod->get_member_index(target_symbol->name);
        members_to_expose.push_back({member_index, pair.symbol});
    }

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
                int member_index = module_payload.mod->get_member_index(exported_symbol->name);
                members_to_expose.push_back({member_index, exported_symbol});
            }
        }
    }

    if (!members_to_expose.empty())
    {
        emit(
            OpCode::UNPACK_MODULE_MEMBERS,
            module_slot,
            static_cast<int>(members_to_expose.size())
        );

        for (const auto& mem : members_to_expose)
        {
            emit_raw_byte(static_cast<std::byte>(mem.index));
            get_or_add_local_index(mem.symbol);
        }
    }
}

} // namespace Wasp
