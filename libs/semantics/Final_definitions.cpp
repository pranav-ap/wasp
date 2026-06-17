#include "AST.h"
#include "Final.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Type.h"

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void Final::visit(FunctionDefinition& def)
{
    current_scope->define_overload(def.overload_symbol);

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

void Final::visit(MethodDefinition& def)
{
    current_scope->define_overload(def.overload_symbol);

    ScopeType scope_type = def.is_pure ? ScopeType::PURE_METHOD : ScopeType::METHOD;

    enter_scope(scope_type);

    for (const auto& parameter : def.parameters)
    {
        current_scope->define(parameter.symbol);
    }

    visit(def.block);

    leave_scope();
}

void Final::visit(OperatorDefinition& def)
{
    enter_scope(ScopeType::PURE_FUNCTION);

    current_scope->define_overload(def.overload_symbol);

    for (const auto& operand : def.operands)
    {
        current_scope->define(operand.symbol);
    }

    visit(def.block);

    leave_scope();
}

void Final::visit(ClassDefinition& def)
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

void Final::visit(TraitDefinition& def)
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

void Final::visit(PrimitiveDefinition& def)
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

void Final::visit(EnumDefinition& def)
{
    current_scope->define(def.symbol);
}

void Final::visit(TypeAliasDefinition& def)
{
    current_scope->define(def.symbol);
}

} // namespace Wasp
