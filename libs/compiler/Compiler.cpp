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
    : workspace(workspace), current_block_id(InvalidBlockId), parent(nullptr), compiler_depth(0)
{
    current_block_id = graph.create_block();
    graph.set_entry_block(current_block_id);
}

Compiler::Compiler(Compiler* parent)
    : parent(parent), workspace(parent->workspace), compiler_depth(parent->compiler_depth + 1),
      current_lexical_scope_depth(parent->current_lexical_scope_depth + 1)
{
    current_block_id = graph.create_block();
    graph.set_entry_block(current_block_id);
}

FunctionBlueprintObject_ptr Compiler::run(
    const StatementVector& block,
    std::string name,
    bool is_main
)
{
    if (is_main)
    {
        emit(OpCode::ENTER_WORKSPACE);
    }

    emit(OpCode::ENTER_MODULE);

    for (const auto& s : block)
    {
        visit(s);
    }

    BlockId exit = graph.create_block();
    graph.add_edge(current_block_id, exit);
    emit(OpCode::JUMP, static_cast<int>(exit));
    set_current_block(exit);

    int export_count = 0;

    for (const auto& sym : locals)
    {
        if (sym->is_exported())
        {
            int physical_index = resolve_local(sym->id);
            emit(OpCode::GET_LOCAL, physical_index, sym->name);
            export_count++;
        }
    }

    // Tell the VM exactly how many items to pop and bundle into the ModuleObject
    emit(OpCode::EXIT_MODULE, export_count);

    if (is_main)
    {
        emit(OpCode::EXIT_WORKSPACE);
    }

    CodeObject final_code = flatten();

    auto function_object = std::make_shared<FunctionBlueprintObject>(std::move(final_code), name);

    return function_object;
}

// ========================================================================
// Visitors
// ========================================================================

void Compiler::visit(std::vector<Statement_ptr>& statements)
{
    // -------------------------------------------------------------------
    // PASS 1: Compiler Hoisting
    // Compile local functions FIRST so the VM creates them in memory
    // and reserves their local variable slots before standard code executes!
    // -------------------------------------------------------------------
    for (auto& stmt : statements)
    {
        if (stmt->is<FunctionDefinition>())
        {
            visit(stmt);
        }
    }

    // -------------------------------------------------------------------
    // PASS 2: Compile the rest of the logic
    // Now things like `ciao()` can successfully resolve because the
    // function compiler already pushed `ciao` into the `locals` vector!
    // -------------------------------------------------------------------
    for (auto& stmt : statements)
    {
        if (!stmt->is<FunctionDefinition>())
        {
            visit(stmt);
        }
    }
}

void Compiler::visit(const Statement_ptr statement)
{
    Doctor::get().fatal_if_nullptr(statement, WaspStage::Compiler);

    std::visit(
        overloaded{
            [&](ExpressionStatement& stat)
            {
                visit(stat);
            },
            [&](VariableDefinition& stat)
            {
                visit(stat);
            },
            [&](ClassDefinition& stat)
            {
                visit(stat);
            },
            [&](ImplDefinition& stat)
            {
                visit(stat);
            },
            [&](IfBranch& stat)
            {
                visit(stat);
            },
            [&](ElseBranch& stat)
            {
                visit(stat);
            },
            [&](SimpleLoop& stat)
            {
                visit(stat);
            },
            [&](ForInLoop& stat)
            {
                visit(stat);
            },
            [&](Pass& stat)
            {
                visit(stat);
            },
            [&](LoopControl& stat)
            {
                visit(stat);
            },
            [&](FunctionDefinition& stat)
            {
                visit(stat);
            },
            [&](Return& stat)
            {
                visit(stat);
            },
            [&](SimpleImport& stat)
            {
                visit(stat);
            },
            [&](FromImport& stat)
            {
                visit(stat);
            },
            [](auto)
            {
                Doctor::get().fatal(WaspStage::Compiler, "Unknown Statement");
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
