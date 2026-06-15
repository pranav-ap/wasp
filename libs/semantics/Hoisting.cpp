#include "Hoisting.h"
#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Statement.h"
#include "SymbolFactory.h"
#include "SymbolScope.h"
#include "Type.h"
#include "Workspace.h"

#include <memory>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void Hoisting::run(Module_ptr mod)
{
    current_module = mod;

    enter_scope(ScopeType::MODULE);
    visit(current_module->block);
    leave_scope();
}

void Hoisting::enter_scope(ScopeType scope_type)
{
    auto new_scope = std::make_shared<SymbolScope>(
        scope_type,
        current_scope
    );
    current_scope = new_scope;
}

void Hoisting::leave_scope()
{
    if (current_scope != nullptr)
    {
        current_scope = current_scope->enclosing_scope;
    }
}

void Hoisting::visit(Block& block)
{
    for (auto& statement : block.statements)
    {
        visit(statement);
    }
}

void Hoisting::visit(Statement_ptr statement)
{
    std::visit(
        [&](auto& node)
        {
            if constexpr (requires { visit(node); })
            {
                visit(node);
            }
        },
        statement->data
    );
}

void Hoisting::visit(Import&)
{
}

void Hoisting::visit(FunctionDefinition& def)
{
    auto symbol = SymbolFactory::create_function(
        def.name,
        nullptr,
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    def.symbol = current_scope->define(symbol);

    enter_scope(ScopeType::FUNCTION);

    for (auto& param : def.parameters)
    {
        auto var_symbol = SymbolFactory::create_variable(
            param.name,
            nullptr,
            false,
            current_scope->closure_depth,
            current_scope->lexical_depth
        );

        param.symbol = current_scope->define(var_symbol);
    }

    visit(def.block);

    leave_scope();
}

void Hoisting::visit(OperatorDefinition& def)
{
    auto symbol = SymbolFactory::create_function(
        def.name,
        nullptr,
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    def.symbol = current_scope->define(symbol);

    enter_scope(ScopeType::FUNCTION);

    for (auto& operand : def.operands)
    {
        auto var_symbol = SymbolFactory::create_variable(
            operand.name,
            nullptr,
            false,
            current_scope->closure_depth,
            current_scope->lexical_depth
        );

        operand.symbol = current_scope->define(var_symbol);
    }

    visit(def.block);

    leave_scope();
}

void Hoisting::visit(ClassDefinition& def)
{
    auto symbol = SymbolFactory::create_type(
        def.name,
        nullptr,
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    def.symbol = current_scope->define(symbol);

    for (auto& method : def.methods)
    {
        visit(method);
    }
}

void Hoisting::visit(TraitDefinition& def)
{
    auto symbol = SymbolFactory::create_type(
        def.name,
        nullptr,
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    def.symbol = current_scope->define(symbol);

    for (auto& method : def.methods)
    {
        visit(method);
    }
}

void Hoisting::visit(PrimitiveDefinition& def)
{
    auto symbol = SymbolFactory::create_type(
        def.name,
        nullptr,
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    def.symbol = current_scope->define(symbol);

    for (auto& method : def.methods)
    {
        visit(method);
    }
}

void Hoisting::visit(EnumDefinition& def)
{
    auto symbol = SymbolFactory::create_type(
        def.name,
        nullptr,
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    def.symbol = current_scope->define(symbol);
}

void Hoisting::visit(TypeAliasDefinition& def)
{
    auto symbol = SymbolFactory::create_type_alias(
        def.name,
        nullptr,
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    def.symbol = current_scope->define(symbol);
}

void Hoisting::visit(Branch& stmt)
{
    enter_scope(ScopeType::BRANCH);
    visit(stmt.test);
    visit(stmt.block);
    leave_scope();
}

void Hoisting::visit(SimpleLoop& stmt)
{
    enter_scope(ScopeType::LOOP);
    visit(stmt.test);
    visit(stmt.block);
    leave_scope();
}

void Hoisting::visit(ForInLoop& stmt)
{
    enter_scope(ScopeType::LOOP);
    visit(stmt.lhs);
    visit(stmt.block);
    leave_scope();
}

void Hoisting::visit(ExpressionStatement& statement)
{
    visit(statement.expression);
}

// ============================================================================
// Expressions
// ============================================================================

void Hoisting::visit(Expression_ptr expression)
{
    std::visit(
        [&](auto& node)
        {
            if constexpr (requires { visit(node); })
            {
                visit(node);
            }
        },
        expression->data
    );
}

void Hoisting::visit(Binding& binding)
{
    Doctor::get().assert(
        binding.lhs->is<Identifier>(),
        WaspStage::Parser,
        "Left-hand side of a binding must be an identifier"
    );

    auto& id = binding.lhs->as<Identifier>();

    auto var_symbol = SymbolFactory::create_variable(
        id.name,
        nullptr,
        binding.is_mutable,
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    id.symbol = current_scope->define(var_symbol);
}

void Hoisting::visit(TernaryExpression& expr)
{
    enter_scope(ScopeType::BRANCH);
    visit(expr.test);
    visit(expr.then_expr);
    visit(expr.else_expr);
    leave_scope();
}

} // namespace Wasp
