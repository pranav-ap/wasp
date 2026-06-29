#include "AST.h"
#include "ASTCloner.h"
#include "Doctor.h"
#include "SemanticsAnalyzer.h"
#include "Solidifier.h"
#include "Statement.h"
#include "Symbol.h"
#include "SymbolFactory.h"
#include "SymbolScope.h"
#include "Type.h"
#include "Workspace.h"

#include <memory>
#include <string>
#include <utility>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void SemanticsAnalyzer::enter_scope(ScopeType scope_type)
{
    auto new_scope = std::make_shared<SymbolScope>(scope_type, current_scope);
    current_scope = new_scope;
}

void SemanticsAnalyzer::leave_scope()
{
    if (current_scope != nullptr)
    {
        current_scope = current_scope->enclosing_scope;
    }
}

std::pair<Statement_ptr, SymbolScope_ptr> SemanticsAnalyzer::get_tree(
    Symbol_ptr symbol
)
{
    auto it = forest.find(symbol);

    Doctor::semantics().check(
        it != forest.end(),
        "No AST found for symbol '" + symbol->name + "'"
    );

    return {it->second.first, it->second.second};
}

bool SemanticsAnalyzer::contains_tree(Symbol_ptr symbol) const
{
    return forest.find(symbol) != forest.end();
}

void SemanticsAnalyzer::add_tree(
    Symbol_ptr symbol,
    Statement_ptr tree,
    SymbolScope_ptr scope
)
{
    Doctor::semantics().check(
        forest.find(symbol) == forest.end(),
        "AST already exists for symbol '" + symbol->name + "'"
    );

    forest[symbol] = {tree, scope};
    scope->solid_trees.push_back(tree);
}

Symbol_ptr SemanticsAnalyzer::solidify_template(
    Symbol_ptr template_symbol,
    const std::string& mangled_name,
    TypeSubstitutionMap& substitutions
)
{
    Symbol_ptr existing = current_scope->lookup(mangled_name);

    if (existing)
    {
        return existing;
    }

    Type_ptr template_type = template_symbol->get_type();
    Type_ptr solid_type = Solidifier::get().substitute_type(template_type, substitutions);

    Symbol_ptr solid_symbol = SymbolFactory::create_type(
        mangled_name,
        solid_type,
        current_scope->closure_depth,
        current_scope->lexical_depth
    );

    current_scope->define(solid_symbol);
    solid_symbol->mangled_name = mangled_name;

    auto [template_ast, definition_scope] = get_tree(template_symbol);
    Doctor::semantics().fatal_if_nullptr(template_ast, "Template AST not found");

    Statement_ptr template_ast_copy = ASTCloner::get().clone(template_ast);
    Statement_ptr solid_ast = Solidifier::get().visit(template_ast_copy, substitutions);

    std::visit(
        overloaded{
            [&](FunctionDefinition& func) -> void
            {
                func.symbol = solid_symbol;
                func.name = mangled_name;
            },
            [&](ClassDefinition& cls) -> void
            {
                cls.symbol = solid_symbol;
                cls.name = mangled_name;
            },
            [&](TraitDefinition& trait) -> void
            {
                trait.symbol = solid_symbol;
                trait.name = mangled_name;
            },
            [&](PrimitiveDefinition& prim) -> void
            {
                prim.symbol = solid_symbol;
                prim.name = mangled_name;
            },
            [&](auto&) -> void
            {
                Doctor::semantics().fatal("Unsupported definition type for template solidification");
            }
        },
        solid_ast->data
    );

    add_tree(solid_symbol, solid_ast, current_scope);

    return solid_symbol;
}

} // namespace Wasp
