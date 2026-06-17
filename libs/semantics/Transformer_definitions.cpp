#include "AST.h"
#include "ASTCloner.h"
#include "Doctor.h"
#include "Statement.h"
#include "Transformer.h"
#include "Type.h"

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void Transformer::visit(FunctionDefinition& def)
{
    current_scope->define_overload(def.overload_symbol);

    if (!def.generics.empty())
    {
        forest[def.symbol] = ASTCloner::get().clone(def);
        scope_forest[def.symbol] = current_scope;
    }
}

void Transformer::visit(OperatorDefinition& def)
{
    current_scope->define_overload(def.overload_symbol);

    if (!def.generics.empty())
    {
        forest[def.symbol] = ASTCloner::get().clone(def);
        scope_forest[def.symbol] = current_scope;
    }
}

void Transformer::copy_trait_methods(TypeDefinition& def, OopsType_ptr oop_type)
{
    for (const auto& trait_obj : oop_type->traits)
    {
        auto trait = trait_obj->as<TraitType_ptr>();

        Symbol_ptr trait_symbol = current_scope->lookup(trait->name);
        Doctor::semantics().fatal_if_nullptr(trait_symbol);

        Statement_ptr ast = forest[trait_symbol];

        Doctor::semantics().fatal_if_nullptr(ast);

        Doctor::semantics().assert(
            ast->is<TraitDefinition>(),
            "Expected a TraitDefinition in the trait forest"
        );

        TraitDefinition trait_ast = ast->as<TraitDefinition>();

        for (FunctionDefinition& method_def : trait_ast.methods)
        {
            Statement_ptr x = ASTCloner::get().clone(method_def);
            FunctionDefinition clone = x->as<FunctionDefinition>();
            def.methods.push_back(clone);
        }
    }
}

void Transformer::visit(ClassDefinition& def)
{
    current_scope->define(def.symbol);

    copy_trait_methods(def, def.symbol->get_type()->as<ClassType_ptr>());

    if (!def.generics.empty())
    {
        forest[def.symbol] = ASTCloner::get().clone(def);
        scope_forest[def.symbol] = current_scope;
    }
}

void Transformer::visit(TraitDefinition& def)
{
    current_scope->define(def.symbol);

    copy_trait_methods(def, def.symbol->get_type()->as<TraitType_ptr>());

    forest[def.symbol] = ASTCloner::get().clone(def);
    scope_forest[def.symbol] = current_scope;
}

void Transformer::visit(PrimitiveDefinition& def)
{
    current_scope->define(def.symbol);

    copy_trait_methods(def, def.symbol->get_type()->as<PrimitiveType_ptr>());

    if (!def.generics.empty())
    {
        forest[def.symbol] = ASTCloner::get().clone(def);
        scope_forest[def.symbol] = current_scope;
    }
}

void Transformer::visit(EnumDefinition& def)
{
    current_scope->define(def.symbol);

    if (!def.generics.empty())
    {
        forest[def.symbol] = ASTCloner::get().clone(def);
        scope_forest[def.symbol] = current_scope;
    }
}

void Transformer::visit(TypeAliasDefinition& def)
{
    current_scope->define(def.symbol);

    if (!def.generics.empty())
    {
        forest[def.symbol] = ASTCloner::get().clone(def);
        scope_forest[def.symbol] = current_scope;
    }
}

} // namespace Wasp
