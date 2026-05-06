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
    Doctor::get().fatal_if_nullptr(
        import_stmt.symbol,
        WaspStage::Compiler,
        "Simple Import must have a resolved symbol"
    );

    auto target_symbol = import_stmt.symbol->resolve();
    auto module_payload = target_symbol->get_payload_as<ModuleData>();

    int module_index = workspace->get_module_index(
        module_payload.mod->absolute_filepath
    );

    emit(OpCode::IMPORT_MODULE, module_index, "module " + target_symbol->name);

    stack.push_back(import_stmt.symbol);
}

void Compiler::visit(FromImport& from_import)
{
    auto module_symbol = from_import.symbol;
    auto module_payload = module_symbol->get_payload_as<ModuleData>();

    int module_index = workspace->get_module_index(
        module_payload.mod->absolute_filepath
    );

    for (const auto& import_as_pair : from_import.import_as_pairs)
    {
        Doctor::get().fatal_if_nullptr(import_as_pair.symbol, WaspStage::Compiler);
        auto target_symbol = import_as_pair.symbol->resolve();

        emit(OpCode::IMPORT_MODULE, module_index, "module " + module_symbol->name);

        int member_index = module_payload.mod->get_member_index(target_symbol->name);
        emit(OpCode::GET_MEMBER, member_index);

        stack.push_back(import_as_pair.symbol);
    }
}

} // namespace Wasp
