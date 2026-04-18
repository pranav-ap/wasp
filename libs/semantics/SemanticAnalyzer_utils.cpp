#include "AST.h"
#include "Doctor.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"


#include <ctime>
#include <memory>
#include <string>
#include <utility>
#include <variant>

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

std::pair<Object_ptr, ObjectVector> SemanticAnalyzer::get_function_signature(
    AbstractFunctionDefinition& func
)
{
    Object_ptr return_type = func.return_type ? visit(func.return_type)
                                              : workspace->pool->get_none_type();

    ObjectVector param_types;
    param_types.reserve(func.parameters.size());

    for (const auto& [param_name, type_ann] : func.parameters)
    {
        param_types.push_back(type_ann ? visit(type_ann) : workspace->pool->get_any_type());
    }

    return {return_type, param_types};
}

std::pair<Object_ptr, ObjectVector> SemanticAnalyzer::get_function_signature(Object_ptr type_obj)
{
    Object_ptr return_type = nullptr;
    ObjectVector param_types;

    std::visit(
        overloaded{
            [&](const std::shared_ptr<FunctionType>& t)
            {
                return_type = t->return_type;
                param_types = t->parameter_types;
            },
            [&](const std::shared_ptr<MethodType>& t)
            {
                return_type = t->return_type;
                param_types = t->parameter_types;
            },
            [&](const auto&)
            {
                Doctor::get().fatal(WaspStage::Semantics, "Expected concrete function type.");
            }
        },
        type_obj->value
    );

    return {return_type, param_types};
}

Object_ptr SemanticAnalyzer::get_function_return_type(Symbol_ptr symbol)
{
    Object_ptr type_obj = symbol->get_type();

    if (!type_obj)
    {
        return nullptr;
    }

    if (auto p = type_obj->try_as<std::shared_ptr<FunctionType>>())
    {
        return (*p)->return_type;
    }
    if (auto p = type_obj->try_as<std::shared_ptr<MethodType>>())
    {
        return (*p)->return_type;
    }

    Doctor::get().fatal(WaspStage::Semantics, "Expected a valid function signature type.");
}

// ============================================================================
// SCOPE MANAGEMENT
// ============================================================================

void SemanticAnalyzer::enter_scope(ScopeType scope_type)
{
    current_scope = std::make_shared<SymbolScope>(scope_type, current_scope);
}

void SemanticAnalyzer::leave_scope()
{
    if (current_scope)
    {
        current_scope = current_scope->get_enclosing();
    }
}

void SemanticAnalyzer::leave_scope_keep_symbol(Symbol_ptr symbol_to_keep)
{
    if (current_scope)
    {
        current_scope = current_scope->get_enclosing();

        if (current_scope)
        {
            current_scope->define(symbol_to_keep);
        }
    }
}
} // namespace Wasp
