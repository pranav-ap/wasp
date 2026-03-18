#include "Compiler.h"
#include "Doctor.h"
#include "OpCode.h"
#include "Statement.h"

#include <map>
#include <memory>
#include <string>

template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp {
void Compiler::visit(SimpleImport& statement) {
    std::string unique_module_path = statement.resolved_path.string();
    int path_index = pool->allocate(unique_module_path);

    // Tell the VM to execute this module and push a ModuleObject onto the stack
    emit(OpCode::IMPORT, path_index);

    Doctor::get().fatal_if_nullptr(
        statement.symbol, WaspStage::Compiler, "Simple Import must have a resolved symbol."
    );

    id_to_name_map[statement.symbol->id] = statement.symbol->name;
    emit(OpCode::DEFINE_LOCAL, statement.symbol->id);
}

void Compiler::visit(FromImport& statement) {
    // No Op
}

} // namespace Wasp
