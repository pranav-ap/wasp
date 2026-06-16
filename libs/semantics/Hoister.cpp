#include "Hoister.h"
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

void Hoister::run(Module_ptr mod)
{
    current_module = mod;

    enter_scope(ScopeType::MODULE);
    visit(current_module->block);
    leave_scope();
}

void Hoister::enter_scope(ScopeType scope_type)
{
    auto new_scope = std::make_shared<SymbolScope>(
        scope_type,
        current_scope
    );
    current_scope = new_scope;
}

void Hoister::leave_scope()
{
    if (current_scope != nullptr)
    {
        current_scope = current_scope->enclosing_scope;
    }
}

void Hoister::visit(Block& block)
{
    for (auto& statement : block.statements)
    {
        visit(statement);
    }
}

void Hoister::visit(Statement_ptr statement)
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

void Hoister::visit(Import&)
{
}

void Hoister::visit(FunctionDefinition& def)
{
    auto symbol = SymbolFactory::create_function(
        def.name,
        nullptr,
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    auto overload_symbol = current_scope->overload(symbol);
    def.symbol = symbol;
    def.overload_symbol = overload_symbol;

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

        current_scope->define(var_symbol);
        param.symbol = symbol;
    }

    visit(def.block);

    leave_scope();
}

void Hoister::visit(OperatorDefinition& def)
{
    auto symbol = SymbolFactory::create_function(
        def.name,
        nullptr,
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    auto overload_symbol = current_scope->overload(symbol);
    def.symbol = symbol;
    def.overload_symbol = overload_symbol;

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

        current_scope->define(var_symbol);
        operand.symbol = var_symbol;
    }

    visit(def.block);

    leave_scope();
}

void Hoister::visit(ClassDefinition& def)
{
    auto symbol = SymbolFactory::create_type(
        def.name,
        nullptr,
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    current_scope->define(symbol);
    def.symbol = symbol;

    enter_scope(ScopeType::CLASS);

    for (auto& method : def.methods)
    {
        visit(method);
    }

    leave_scope();
}

void Hoister::visit(TraitDefinition& def)
{
    auto symbol = SymbolFactory::create_type(
        def.name,
        nullptr,
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    current_scope->define(symbol);
    def.symbol = symbol;

    enter_scope(ScopeType::CLASS);

    for (auto& method : def.methods)
    {
        visit(method);
    }

    leave_scope();
}

void Hoister::visit(PrimitiveDefinition& def)
{
    auto symbol = SymbolFactory::create_type(
        def.name,
        nullptr,
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    current_scope->define(symbol);
    def.symbol = symbol;

    enter_scope(ScopeType::CLASS);

    for (auto& method : def.methods)
    {
        visit(method);
    }

    leave_scope();
}

void Hoister::visit(EnumDefinition& def)
{
    auto symbol = SymbolFactory::create_type(
        def.name,
        nullptr,
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    current_scope->define(symbol);
    def.symbol = symbol;
}

void Hoister::visit(TypeAliasDefinition& def)
{
    auto symbol = SymbolFactory::create_type_alias(
        def.name,
        nullptr,
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    current_scope->define(symbol);
    def.symbol = symbol;
}

void Hoister::visit(Branch& stmt)
{
    enter_scope(ScopeType::BRANCH);
    visit(stmt.test);
    visit(stmt.block);
    leave_scope();
}

void Hoister::visit(SimpleLoop& stmt)
{
    enter_scope(ScopeType::LOOP);
    visit(stmt.test);
    visit(stmt.block);
    leave_scope();
}

void Hoister::visit(ForInLoop& stmt)
{
    enter_scope(ScopeType::LOOP);
    visit(stmt.lhs);
    visit(stmt.block);
    leave_scope();
}

void Hoister::visit(ExpressionStatement& statement)
{
    visit(statement.expression);
}

// ============================================================================
// Expressions
// ============================================================================

void Hoister::visit(Expression_ptr expression)
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

void Hoister::visit(Binding& binding)
{
    Doctor::parser().assert(
        binding.lhs->is<Identifier>(),
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

    current_scope->define(var_symbol);
    id.symbol = var_symbol;
}

void Hoister::visit(TernaryExpression& expr)
{
    enter_scope(ScopeType::BRANCH);
    visit(expr.test);
    visit(expr.then_expr);
    visit(expr.else_expr);
    leave_scope();
}

} // namespace Wasp
