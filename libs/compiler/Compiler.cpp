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
#include <type_traits>
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
    : workspace(std::move(workspace)), current_block_id(InvalidBlockId),
      parent(nullptr), compiler_depth(0), current_lexical_scope_depth(0)
{
    current_block_id = graph.create_block();
    graph.set_entry_block(current_block_id);
}

Compiler::Compiler(Compiler* parent)
    : parent(parent), workspace(parent->workspace),
      compiler_depth(parent->compiler_depth + 1),
      current_lexical_scope_depth(parent->current_lexical_scope_depth + 1),
      module_path(parent->module_path)
{
    current_block_id = graph.create_block();
    graph.set_entry_block(current_block_id);
}

// ==========================================================================
// Entry Point
// ==========================================================================

FunctionBlueprintObject_ptr Compiler::run(
    const StatementVector& block,
    std::string filepath,
    bool is_main
)
{
    this->module_path = std::move(filepath);

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

    emit_exports();

    if (is_main)
    {
        emit(OpCode::EXIT_WORKSPACE);
    }

    return std::make_shared<FunctionBlueprintObject>(flatten(), module_path);
}

// ========================================================================
// Statement Visitors
// ========================================================================

void Compiler::visit(std::vector<Statement_ptr>& statements)
{
    // PASS 1: Pre-allocate stack slots to support forward references
    for (auto& stmt_ptr : statements)
    {
        Symbol_ptr sym = nullptr;
        bool is_callable = false;

        std::visit(
            overloaded{
                [&](FunctionDefinition& def)
                {
                    sym = def.group_symbol;
                    is_callable = true;
                },
                [&](OperatorDefinition& def)
                {
                    sym = def.group_symbol;
                    is_callable = true;
                },
                [&](ClassDefinition& def)
                {
                    sym = def.symbol;
                },
                [&](TraitDefinition& def)
                {
                    sym = def.symbol;
                },
                [&](EnumDefinition& def)
                {
                    sym = def.symbol;
                },
                [&](TypeAliasDefinition& def)
                {
                    sym = def.symbol;
                },
                [](auto&) { /* Do nothing for all other statement types */ }
            },
            stmt_ptr->data
        );

        // If it's a declaration we haven't allocated yet, claim a slot!
        if (sym && resolve_local(sym->id) == -1)
        {
            int slot = get_or_add_local_index(sym);

            if (is_callable)
            {
                emit(OpCode::PUSH_EMPTY_OVERLOAD_GROUP);
            }
            else
            {
                emit(OpCode::LOAD_NONE);
            }
            emit(
                OpCode::SET_LOCAL,
                slot,
                "pre-allocate hoist for " + sym->name
            );
        }
    }

    // PASS 2: Compile all statements in their natural, top-to-bottom order!
    for (auto& stmt_ptr : statements)
    {
        visit(stmt_ptr);
    }
}

void Compiler::visit(const Statement_ptr statement)
{
    Doctor::get().fatal_if_nullptr(statement, WaspStage::Compiler);

    std::visit(
        [this](auto& node)
        {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, std::monostate>)
            {
                Doctor::get().fatal(
                    WaspStage::Compiler,
                    "Unhandled Statement (monostate) in Compiler!"
                );
            }
            else if constexpr (requires { this->visit(node); })
            {
                this->visit(node);
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

// ============================================================================
// Expression Visitors
// ============================================================================

void Compiler::visit(std::vector<Expression_ptr>& expressions)
{
    for (const auto& expr : expressions)
    {
        visit(expr);
    }
}

void Compiler::visit(const Expression_ptr expr)
{
    Doctor::get().fatal_if_nullptr(expr, WaspStage::Compiler);

    std::visit(
        [this](auto& node)
        {
            if constexpr (requires { this->visit(node); })
            {
                this->visit(node);
            }
            else
            {
                Doctor::get().fatal(
                    WaspStage::Compiler,
                    "Unimplemented expression compilation"
                );
            }
        },
        expr->data
    );
}

} // namespace Wasp
