#include "Compiler.h"
#include "AST.h"
#include "CFGraph.h"
#include "Doctor.h"
#include "Objects.h"
#include "OpCode.h"
#include "Statement.h"
#include "Workspace.h"

#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

Compiler::Compiler(Workspace_ptr workspace)
    : workspace(std::move(workspace)), current_block_id(InvalidBlockId), parent(nullptr),
      compiler_depth(0), current_lexical_scope_depth(0)
{
    current_block_id = graph.create_block();
    graph.set_entry_block(current_block_id);
}

Compiler::Compiler(Compiler* parent)
    : parent(parent), workspace(parent->workspace), compiler_depth(parent->compiler_depth + 1),
      current_lexical_scope_depth(parent->current_lexical_scope_depth + 1),
      module_path(parent->module_path)
{
    current_block_id = graph.create_block();
    graph.set_entry_block(current_block_id);
}

FunctionBlueprintObject_ptr Compiler::run(
    const StatementVector& block,
    std::string filepath,
    bool is_main
)
{
    this->module_path = std::move(filepath);

    if (is_main)
        emit(OpCode::ENTER_WORKSPACE);
    emit(OpCode::ENTER_MODULE);

    for (const auto& s : block)
        visit(s);

    BlockId exit = graph.create_block();
    graph.add_edge(current_block_id, exit);
    emit(OpCode::JUMP, static_cast<int>(exit));
    set_current_block(exit);

    emit_exports();

    if (is_main)
        emit(OpCode::EXIT_WORKSPACE);

    return std::make_shared<FunctionBlueprintObject>(flatten(), module_path);
}

void Compiler::emit_exports()
{
    int export_count = 0;
    for (const auto& sym : stack)
    {
        if (sym->is_exportable())
        {
            emit(OpCode::GET_LOCAL, resolve_local(sym->id), sym->name);
            export_count++;
        }
    }
    emit(OpCode::EXIT_MODULE, export_count);
}

// ========================================================================
// Visitors
// ========================================================================

void Compiler::visit(std::vector<Statement_ptr>& statements)
{
    auto is_func = [](const Statement_ptr& s)
    {
        return s->is<FunctionDefinition>() || s->is<PureFunctionDefinition>();
    };

    for (auto& stmt : statements)
        if (is_func(stmt))
            visit(stmt);

    for (auto& stmt : statements)
        if (!is_func(stmt))
            visit(stmt);
}

void Compiler::visit(const Statement_ptr statement)
{
    Doctor::get().fatal_if_nullptr(statement, WaspStage::Compiler);

    std::visit(
        overloaded{
            [&](std::monostate&)
            {
                Doctor::get().fatal(
                    WaspStage::Compiler,
                    "Unhandled Statement (monostate) in Compiler!"
                );
            },
            [&](auto& stat)
            {
                this->visit(stat);
            }
        },
        statement->data
    );
}

void Compiler::visit(ExpressionStatement& statement)
{
    visit(statement.expression);
    emit(OpCode::POP);
}

} // namespace Wasp
