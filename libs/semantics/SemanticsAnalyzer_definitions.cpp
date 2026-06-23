#include "AST.h"
#include "SemanticsAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void SemanticsAnalyzer::visit(FunctionDefinition& def)
{
    ScopeType scope_type = def.is_pure ? ScopeType::PURE_FUNCTION
                                       : ScopeType::FUNCTION;

    enter_scope(scope_type);

    for (const auto& parameter : def.parameters)
    {
        current_scope->define(parameter.symbol);
    }

    visit(def.block);

    leave_scope();

    scope_forest[def.symbol] = current_scope;
}

void SemanticsAnalyzer::visit(MethodDefinition& def)
{
    ScopeType scope_type = def.is_pure ? ScopeType::PURE_METHOD : ScopeType::METHOD;
    enter_scope(scope_type);

    current_scope->define(def.our_context_symbol);

    if (def.self_context_symbol != nullptr)
    {
        current_scope->define(def.self_context_symbol);
    }

    for (const auto& parameter : def.parameters)
    {
        current_scope->define(parameter.symbol);
    }

    visit(def.block);
    leave_scope();
}

void SemanticsAnalyzer::visit(OperatorDefinition& def)
{
    enter_scope(ScopeType::PURE_FUNCTION);

    for (const auto& operand : def.operands)
    {
        current_scope->define(operand.symbol);
    }

    visit(def.block);

    leave_scope();

    scope_forest[def.symbol] = current_scope;
}

void SemanticsAnalyzer::visit(ClassDefinition& def)
{
    enter_scope(ScopeType::CLASS);

    for (auto& method : def.methods)
    {
        visit(method);
    }

    leave_scope();

    scope_forest[def.symbol] = current_scope;
}

void SemanticsAnalyzer::visit(TraitDefinition& def)
{
    enter_scope(ScopeType::TRAIT);

    for (auto& method : def.methods)
    {
        visit(method);
    }

    leave_scope();

    scope_forest[def.symbol] = current_scope;
}

void SemanticsAnalyzer::visit(PrimitiveDefinition& def)
{
    enter_scope(ScopeType::PRIMITIVE);

    for (auto& method : def.methods)
    {
        visit(method);
    }

    leave_scope();

    scope_forest[def.symbol] = current_scope;
}

void SemanticsAnalyzer::visit(EnumDefinition& def)
{
    scope_forest[def.symbol] = current_scope;
}

void SemanticsAnalyzer::visit(TypeAliasDefinition& def)
{
    scope_forest[def.symbol] = current_scope;
}

} // namespace Wasp
