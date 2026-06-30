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
#include "TypeSystem.h"
#include "Workspace.h"

#include <cstddef>
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

void SemanticsAnalyzer::deduce_from_type(
    Type_ptr type,
    const Type_ptr& arg_type,
    TypeSubstitutionMap& substitutions,
    bool& ok
) const
{
    if (!ok || !type || !arg_type)
    {
        return;
    }

    if (type->is<GenericType_ptr>())
    {
        auto generic = type->as<GenericType_ptr>();
        const auto& name = generic->name;

        if (generic->constraint_type &&
            !TypeSystem::assignable(current_scope, generic->constraint_type, arg_type))
        {
            ok = false;
            return;
        }

        auto it = substitutions.find(name);
        if (it != substitutions.end())
        {
            if (!TypeSystem::equal(current_scope, it->second, arg_type))
            {
                ok = false;
            }
        }
        else
        {
            substitutions[name] = arg_type;
        }
        return;
    }

    // Composite types
    if (type->is<ListType_ptr>() && arg_type->is<ListType_ptr>())
    {
        deduce_from_type(
            type->as<ListType_ptr>()->element_type,
            arg_type->as<ListType_ptr>()->element_type,
            substitutions,
            ok
        );
        return;
    }

    if (type->is<SetType_ptr>() && arg_type->is<SetType_ptr>())
    {
        deduce_from_type(
            type->as<SetType_ptr>()->element_type,
            arg_type->as<SetType_ptr>()->element_type,
            substitutions,
            ok
        );

        return;
    }

    if (type->is<MapType_ptr>() && arg_type->is<MapType_ptr>())
    {
        auto t = type->as<MapType_ptr>();
        auto a = arg_type->as<MapType_ptr>();

        deduce_from_type(t->key_type, a->key_type, substitutions, ok);

        if (ok)
        {
            deduce_from_type(t->value_type, a->value_type, substitutions, ok);
        }

        return;
    }

    if (type->is<TupleType_ptr>() && arg_type->is<TupleType_ptr>())
    {
        auto t = type->as<TupleType_ptr>();
        auto a = arg_type->as<TupleType_ptr>();
        if (t->element_types.size() != a->element_types.size())
        {
            ok = false;
            return;
        }
        for (size_t i = 0; i < t->element_types.size() && ok; ++i)
        {
            deduce_from_type(t->element_types[i], a->element_types[i], substitutions, ok);
        }
        return;
    }

    if (type->is<VariantType_ptr>() && arg_type->is<VariantType_ptr>())
    {
        auto t = type->as<VariantType_ptr>();
        auto a = arg_type->as<VariantType_ptr>();
        if (t->types.size() != a->types.size())
        {
            ok = false;
            return;
        }

        for (size_t i = 0; i < t->types.size() && ok; ++i)
        {
            deduce_from_type(t->types[i], a->types[i], substitutions, ok);
        }

        return;
    }

    if (type->is<IntersectionType_ptr>() && arg_type->is<IntersectionType_ptr>())
    {
        auto t = type->as<IntersectionType_ptr>();
        auto a = arg_type->as<IntersectionType_ptr>();

        if (t->types.size() != a->types.size())
        {
            ok = false;
            return;
        }

        for (size_t i = 0; i < t->types.size() && ok; ++i)
        {
            deduce_from_type(t->types[i], a->types[i], substitutions, ok);
        }

        return;
    }

    if (!TypeSystem::assignable(current_scope, type, arg_type))
    {
        ok = false;
    }
}

} // namespace Wasp
