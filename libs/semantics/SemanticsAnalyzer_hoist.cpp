#include "AST.h"
#include "SemanticsAnalyzer.h"
#include "Statement.h"
#include "SymbolFactory.h"
#include "Type.h"

#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

// ============================================================================
// Statements
// ============================================================================

void SemanticsAnalyzer::hoist(Block& block)
{
    for (Statement_ptr& statement : block.statements)
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

void SemanticsAnalyzer::hoist(EnumDefinition& def)
{
    Symbol_ptr symbol = SymbolFactory::create_type(
        def.name,
        make_shared_type<EnumType>(def.name),
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    current_scope->define(symbol);
    def.symbol = symbol;
}

void SemanticsAnalyzer::hoist(TypeAliasDefinition& def)
{
    Symbol_ptr symbol = SymbolFactory::create_type_alias(
        def.name,
        nullptr,
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    current_scope->define(symbol);
    def.symbol = symbol;
}

void SemanticsAnalyzer::hoist(FunctionDefinition& def)
{
    Symbol_ptr symbol = SymbolFactory::create_type(
        def.name,
        make_shared_type<FunctionType>(def.name),
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    current_scope->define(symbol);
    def.symbol = symbol;
}

void SemanticsAnalyzer::hoist(OperatorDefinition& def)
{
    Symbol_ptr symbol = SymbolFactory::create_type(
        def.name,
        make_shared_type<FunctionType>(def.name),
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    current_scope->define(symbol);
    def.symbol = symbol;
}

void SemanticsAnalyzer::hoist(MethodDefinition& def)
{
    Symbol_ptr symbol = SymbolFactory::create_type(
        def.name,
        make_shared_type<MethodType>(def.name),
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    current_scope->define(symbol);
    def.symbol = symbol;
}

void SemanticsAnalyzer::hoist(MethodDefinitionVector& methods)
{
    for (MethodDefinition& method : methods)
    {
        hoist(method);
    }
}

void SemanticsAnalyzer::hoist(ClassDefinition& def)
{
    Symbol_ptr symbol = SymbolFactory::create_type(
        def.name,
        make_shared_type<ClassType>(def.name),
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    current_scope->define(symbol);
    def.symbol = symbol;
}

void SemanticsAnalyzer::hoist(TraitDefinition& def)
{
    Symbol_ptr symbol = SymbolFactory::create_type(
        def.name,
        make_shared_type<TraitType>(def.name),
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    current_scope->define(symbol);
    def.symbol = symbol;
}

void SemanticsAnalyzer::hoist(PrimitiveDefinition& def)
{
    Symbol_ptr symbol = SymbolFactory::create_type(
        def.name,
        make_shared_type<PrimitiveType>(def.name),
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    current_scope->define(symbol);
    def.symbol = symbol;
}

} // namespace Wasp
