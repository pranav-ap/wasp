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
#include <optional>
#include <string>
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
    std::optional<std::string> variadic_name = std::nullopt;

    for (const auto& field : template_params)
    {
        Object_ptr constraint_type = visit(field.type);
        constraint_type = constraint_type->unwrap_type_alias();

        if (constraint_type->is<IntersectionType_ptr>())
        {
            for (auto& inner_type :
                 constraint_type->as<IntersectionType_ptr>()->types)
            {
                Doctor::get().assert(
                    inner_type->is<TraitType_ptr>(),
                    WaspStage::Semantics,
                    "Only an interesection of traits is supported"
                );
            }
        }
        else if (constraint_type->is<VariantType_ptr>())
        {
            for (auto& inner_type :
                 constraint_type->as<VariantType_ptr>()->types)
            {
                Doctor::get().assert(
                    type_system->is_native_type(inner_type),
                    WaspStage::Semantics,
                    "Only an union of primitives is supported"
                );
            }
        }

        if (field.is_variadic)
        {
            Doctor::get().assert(
                !variadic_name.has_value(),
                WaspStage::Semantics,
                "Multiple variadic template parameters are not allowed"
            );

            Doctor::get().assert(
                template_params_map.empty() ||
                    variadic_name.has_value() == false,
                WaspStage::Semantics,
                "Variadic template parameter must be the last parameter"
            );

            variadic_name = field.name;
        }

        auto generic_type_obj = make_object(
            std::make_shared<GenericType>(
                field.name,
                constraint_type,
                field.is_variadic
            )
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
        std::move(ordered_names),
        variadic_name
    );
}

} // namespace Wasp
