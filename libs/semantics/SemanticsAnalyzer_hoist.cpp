#include "AST.h"
#include "SemanticsAnalyzer.h"
#include "Statement.h"
#include "SymbolFactory.h"
#include "SymbolScope.h"
#include "Type.h"

#include <string>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

namespace
{

void hoist_generic_types(FieldVector& generics, SymbolScope_ptr current_scope)
{
    for (Field& generic : generics)
    {
        Type_ptr generic_type = make_shared_type<GenericType>(generic.name);

        Symbol_ptr symbol = SymbolFactory::create_type(
            generic.name,
            generic_type,
            current_scope->closure_depth,
            current_scope->lexical_depth
        );

        generic.symbol = symbol;
        current_scope->define(symbol);
    }
}

} // namespace

// ============================================================================
// Statements
// ============================================================================

void SemanticsAnalyzer::hoist(Block& block)
{
    for (auto& statement : block.statements)
    {
        hoist(statement);
    }
}

void SemanticsAnalyzer::hoist(Statement_ptr statement)
{
    std::visit(
        overloaded{
            [&](Import& imp)
            {
                import_symbols(imp);
            },
            [&](auto& node)
            {
                if constexpr (requires { hoist(node); })
                {
                    hoist(node);
                }
            }
        },
        statement->data
    );
}

void SemanticsAnalyzer::hoist(FunctionDefinition& def)
{
    auto symbol = SymbolFactory::create_function(
        def.name,
        make_shared_type<FunctionType>(def.name),
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    auto overload_symbol = current_scope->overload_function(symbol);
    def.symbol = symbol;
    def.overload_symbol = overload_symbol;

    enter_scope(ScopeType::FUNCTION);

    hoist_generic_types(def.generics, current_scope);

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
        param.symbol = var_symbol;
    }

    leave_scope();
}

void SemanticsAnalyzer::hoist(MethodDefinition& def)
{
    auto symbol = SymbolFactory::create_method(
        def.name,
        make_shared_type<MethodType>(def.name),
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    auto overload_symbol = current_scope->overload_method(symbol);
    def.symbol = symbol;
    def.overload_symbol = overload_symbol;

    enter_scope(ScopeType::METHOD);

    Type_ptr context_type = def.class_symbol->get_type();

    Symbol_ptr our_context_symbol = SymbolFactory::create_variable(
        "our",
        context_type,
        false,
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    current_scope->define(our_context_symbol);
    def.our_context_symbol = our_context_symbol;

    if (!def.is_shared)
    {
        Symbol_ptr self_context_symbol = SymbolFactory::create_variable(
            "self",
            context_type,
            false,
            current_scope->closure_depth,
            current_scope->lexical_depth
        );

        current_scope->define(self_context_symbol);
        def.self_context_symbol = self_context_symbol;
    }

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
        param.symbol = var_symbol;
    }

    leave_scope();
}

void SemanticsAnalyzer::hoist(OperatorDefinition& def)
{
    auto symbol = SymbolFactory::create_function(
        def.name,
        nullptr,
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    auto overload_symbol = current_scope->overload_function(symbol);
    def.symbol = symbol;
    def.overload_symbol = overload_symbol;

    enter_scope(ScopeType::FUNCTION);

    hoist_generic_types(def.generics, current_scope);

    for (auto& operand : def.operands)
    {
        auto var_symbol = SymbolFactory::create_variable(
            operand.name,
            make_shared_type<FunctionType>(def.name),
            false,
            current_scope->closure_depth,
            current_scope->lexical_depth
        );

        current_scope->define(var_symbol);
        operand.symbol = var_symbol;
    }

    leave_scope();
}

void SemanticsAnalyzer::hoist(ClassDefinition& def)
{
    auto symbol = SymbolFactory::create_type(
        def.name,
        make_shared_type<ClassType>(def.name),
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    current_scope->define(symbol);
    def.symbol = symbol;

    enter_scope(ScopeType::CLASS);

    hoist_generic_types(def.generics, current_scope);

    for (auto& method : def.methods)
    {
        method.class_symbol = symbol;
        hoist(method);
    }

    leave_scope();
}

void SemanticsAnalyzer::hoist(TraitDefinition& def)
{
    auto symbol = SymbolFactory::create_type(
        def.name,
        make_shared_type<TraitType>(def.name),
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    current_scope->define(symbol);
    def.symbol = symbol;

    enter_scope(ScopeType::TRAIT);

    hoist_generic_types(def.generics, current_scope);

    for (auto& method : def.methods)
    {
        method.class_symbol = symbol;
        hoist(method);
    }

    leave_scope();
}

void SemanticsAnalyzer::hoist(PrimitiveDefinition& def)
{
    auto symbol = SymbolFactory::create_type(
        def.name,
        make_shared_type<PrimitiveType>(def.name),
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    current_scope->define(symbol);
    def.symbol = symbol;

    enter_scope(ScopeType::PRIMITIVE);

    hoist_generic_types(def.generics, current_scope);

    for (auto& method : def.methods)
    {
        method.class_symbol = symbol;
        hoist(method);
    }

    leave_scope();
}

void SemanticsAnalyzer::hoist(EnumDefinition& def)
{
    auto symbol = SymbolFactory::create_type(
        def.name,
        make_shared_type<EnumType>(def.name),
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    current_scope->define(symbol);
    def.symbol = symbol;

    enter_scope(ScopeType::CLASS);
    hoist_generic_types(def.generics, current_scope);
    leave_scope();
}

void SemanticsAnalyzer::hoist(TypeAliasDefinition& def)
{
    auto symbol = SymbolFactory::create_type_alias(
        def.name,
        nullptr,
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    current_scope->define(symbol);
    def.symbol = symbol;

    enter_scope(ScopeType::CLASS);
    hoist_generic_types(def.generics, current_scope);
    leave_scope();
}

} // namespace Wasp
