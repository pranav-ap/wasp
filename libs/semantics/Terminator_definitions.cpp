#include "AST.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Terminator.h"
#include "Type.h"

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void Terminator::visit(FunctionDefinition& def)
{
    current_scope->define_function_overload(def.overload_symbol);

    ScopeType scope_type = def.is_pure ? ScopeType::PURE_FUNCTION
                                       : ScopeType::FUNCTION;

    enter_scope(scope_type);

    for (const auto& parameter : def.parameters)
    {
        current_scope->define(parameter.symbol);
    }

    visit(def.block);

    leave_scope();
}

void Terminator::visit(MethodDefinition& def)
{
    current_scope->define_method_overload(def.overload_symbol);

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

void Terminator::visit(OperatorDefinition& def)
{
    current_scope->define_function_overload(def.overload_symbol);

    enter_scope(ScopeType::PURE_FUNCTION);

    for (const auto& operand : def.operands)
    {
        current_scope->define(operand.symbol);
    }

    visit(def.block);

    leave_scope();
}

void Terminator::visit(ClassDefinition& def)
{
    current_scope->define(def.symbol);

    enter_scope(ScopeType::CLASS);

    current_scope->define(
        def.symbol->get_type()->as<ClassType_ptr>()->template_type
    );

    for (auto& method : def.methods)
    {
        visit(method);
    }

    leave_scope();
}

void Terminator::visit(TraitDefinition& def)
{
    current_scope->define(def.symbol);

    enter_scope(ScopeType::TRAIT);

    current_scope->define(
        def.symbol->get_type()->as<TraitType_ptr>()->template_type
    );

    for (auto& method : def.methods)
    {
        visit(method);
    }

    leave_scope();
}

void Terminator::visit(PrimitiveDefinition& def)
{
    current_scope->define(def.symbol);

    enter_scope(ScopeType::PRIMITIVE);

    current_scope->define(
        def.symbol->get_type()->as<PrimitiveType_ptr>()->template_type
    );

    for (auto& method : def.methods)
    {
        visit(method);
    }

    leave_scope();
}

void Terminator::visit(EnumDefinition& def)
{
    current_scope->define(def.symbol);
}

void Terminator::visit(TypeAliasDefinition& def)
{
    current_scope->define(def.symbol);
}

} // namespace Wasp
