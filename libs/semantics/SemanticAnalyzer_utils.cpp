#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <ctime>
#include <memory>
#include <utility>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void SemanticAnalyzer::enter_scope(ScopeType scope_type)
{
    current_scope = std::make_shared<SymbolScope>(scope_type, current_scope);
}

void SemanticAnalyzer::leave_scope()
{
    if (current_scope)
    {
        current_scope = current_scope->get_enclosing_scope();
    }
}

void SemanticAnalyzer::leave_scope_keep_symbol(Symbol_ptr symbol_to_keep)
{
    if (current_scope)
    {
        current_scope = current_scope->get_enclosing_scope();

        if (current_scope)
        {
            current_scope->define(symbol_to_keep);
        }
    }
}

void SemanticAnalyzer::bind_identifier(Identifier& id, Symbol_ptr symbol)
{
    id.symbol = symbol;

    if (symbol->should_be_captured(current_scope->get_closure_depth()))
    {
        id.must_be_captured = true;
    }
}

void SemanticAnalyzer::define_template_parameters(
    TemplateType_ptr template_type
)
{
    if (!template_type || template_type->ordered_parameter_names.empty())
    {
        return;
    }

    for (const auto& [name, generic_type_obj] :
         template_type->get_ordered_generics())
    {
        Doctor::get().assert(
            generic_type_obj->is<GenericType_ptr>(),
            WaspStage::Semantics,
            "Expected GenericType for template parameter: " + name
        );

        auto symbol = SymbolFactory::create_generic(name, generic_type_obj);

        current_scope->define(symbol);
    }
}

TemplateType_ptr SemanticAnalyzer::create_template_type(
    const FieldDefinitionVector& template_params
)
{
    ObjectStringMap template_params_map;
    StringVector ordered_names;

    for (const auto& field : template_params)
    {
        Object_ptr constraint_type = visit(field.type);
        constraint_type = constraint_type->unwrap_type_alias();

        auto generic_type_obj = make_object(
            std::make_shared<GenericType>(field.name, constraint_type)
        );

        Doctor::get().assert(
            !template_params_map.contains(field.name),
            WaspStage::Semantics,
            "Duplicate template parameter name: " + field.name
        );

        template_params_map[field.name] = generic_type_obj;
        ordered_names.push_back(field.name);
    }

    return std::make_shared<TemplateType>(
        std::move(template_params_map),
        std::move(ordered_names)
    );
}

Statement_ptr SemanticAnalyzer::get_ast(Symbol_ptr symbol) const
{
    Doctor::get().fatal_if_nullptr(
        symbol,
        WaspStage::Semantics,
        "Cannot get AST: symbol is null"
    );

    auto it = forest.find(symbol);

    Doctor::get().assert(
        it != forest.end(),
        WaspStage::Semantics,
        "AST not found for symbol: " + symbol->name
    );

    Doctor::get().fatal_if_nullptr(
        it->second,
        WaspStage::Semantics,
        "AST is null for symbol: " + symbol->name
    );

    return it->second;
}

} // namespace Wasp
