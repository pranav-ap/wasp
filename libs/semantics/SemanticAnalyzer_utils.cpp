#include "AST.h"
#include "Doctor.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <ctime>
#include <memory>

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

TemplateType_ptr SemanticAnalyzer::evaluate_template_params(
    const FieldDefinitionVector& template_params
)
{
    if (template_params.empty())
    {
        return nullptr;
    }

    ObjectStringMap template_params_map;
    StringVector ordered_names;

    for (const auto& field : template_params)
    {
        auto template_param_type = make_object(
            std::make_shared<GenericType>(field.name, visit(field.type))
        );

        Doctor::get().assert(
            !template_params_map.contains(field.name),
            WaspStage::Semantics,
            "Duplicate template parameter name " + field.name
        );

        template_params_map[field.name] = template_param_type;
        ordered_names.push_back(field.name);
    }

    return std::make_shared<TemplateType>(template_params_map, ordered_names);
}

} // namespace Wasp
