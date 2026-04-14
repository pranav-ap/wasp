#include "AST.h"
#include "Doctor.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

// ============================================================================
// CLASS
// ============================================================================

void SemanticAnalyzer::visit(ClassDefinition& def)
{
    auto class_type = extract_class_type(def);
    Object_ptr target_type_obj = make_object(class_type);

    Doctor::get().fatal_if_nullptr(def.symbol, WaspStage::Semantics);
    def.symbol->set_type(target_type_obj);

    class_type_stack.push_back(target_type_obj);

    hoist_class_methods(def, target_type_obj, class_type);
    analyze_class_methods(def);

    class_type_stack.pop_back();
}

std::shared_ptr<ClassType> SemanticAnalyzer::extract_class_type(ClassDefinition& def)
{
    ObjectStringMap member_types;
    StringVector declaration_order;

    auto add_unique_declaration = [&](const std::string& name)
    {
        if (std::find(declaration_order.begin(), declaration_order.end(), name) ==
            declaration_order.end())
        {
            declaration_order.push_back(name);
        }
    };

    for (auto& stmt : def.members)
    {
        std::visit(
            overloaded{
                [&](FieldDefinition& field)
                {
                    auto field_type = visit(field.type);
                    member_types[field.name] = field_type;
                    declaration_order.push_back(field.name);
                },
                [&](MethodDefinition& method)
                {
                    add_unique_declaration(method.name);
                },
                [&](auto&)
                {
                    Doctor::get().fatal(WaspStage::Semantics, "Invalid statement in class body.");
                }
            },
            stmt->data
        );
    }

    return std::make_shared<ClassType>(
        def.name,
        std::move(member_types),
        std::move(declaration_order)
    );
}

void SemanticAnalyzer::hoist_class_methods(
    ClassDefinition& def,
    Object_ptr target_type_obj,
    std::shared_ptr<ClassType> class_type
)
{
    for (auto& stmt : def.members)
    {
        std::visit(
            overloaded{
                [&](MethodDefinition& m)
                {
                    hoist_method(m, def.name, target_type_obj, class_type);
                },
                [&](auto&)
                {
                }
            },
            stmt->data
        );
    }
}

void SemanticAnalyzer::hoist_method(
    AbstractFunctionDefinition& method_def,
    const std::string& container_name,
    Object_ptr target_type_obj,
    std::shared_ptr<ClassType> class_type
)
{
    std::string original_name = method_def.name;
    method_def.name = container_name + "::" + original_name;

    auto [return_type, parameter_types] = get_function_signature(method_def);

    Object_ptr signature = make_object(
        std::make_shared<MethodType>(parameter_types, return_type, target_type_obj)
    );

    Symbol_ptr method_symbol = SymbolFactory::create_method(
        method_def.name,
        signature,
        target_type_obj,
        false,
        current_scope->get_closure_depth(),
        current_scope->get_lexical_depth()
    );

    if (current_scope->contains_in_current_scope(method_def.name))
    {
        type_checker->validate_new_function_wrt_overload_group(
            current_scope,
            method_def.name,
            method_symbol
        );
    }

    current_scope->define(method_symbol);
    method_def.symbol = method_symbol;
    method_def.group_symbol = current_scope->lookup(method_def.name);

    class_type->members[original_name] = method_symbol->get_type();
}

void SemanticAnalyzer::analyze_class_methods(ClassDefinition& def)
{
    for (auto& stmt : def.members)
    {
        std::visit(
            overloaded{
                [&](MethodDefinition& m)
                {
                    visit(m);
                },
                [&](auto&)
                {
                }
            },
            stmt->data
        );
    }
}

// ============================================================================
// FUNCTIONS
// ============================================================================

void SemanticAnalyzer::visit(FunctionDefinition& fun_def)
{
    analyze_abstract_function_body(fun_def, false);
}

void SemanticAnalyzer::visit(MethodDefinition& method_def)
{
    analyze_abstract_function_body(method_def, true);
}

void SemanticAnalyzer::analyze_abstract_function_body(
    AbstractFunctionDefinition& fun_def,
    bool is_method
)
{
    Object_ptr return_type;
    ObjectVector param_types;

    if (fun_def.symbol->get_type() == nullptr)
    {
        // Top-level function (SymbolHoister left the type as nullptr)
        std::tie(return_type, param_types) = get_function_signature(fun_def);

        auto signature = make_object(std::make_shared<LocalFunctionType>(param_types, return_type));
        fun_def.symbol->set_type(signature);
        fun_def.group_symbol = current_scope->lookup(fun_def.name);
    }
    else
    {
        // Impl method or local function (already evaluated, extract types cleanly)
        std::tie(return_type, param_types) = get_function_signature(fun_def.symbol->get_type());
    }

    Doctor::get().fatal_if_nullptr(fun_def.group_symbol, WaspStage::Semantics);
    type_checker
        ->validate_new_function_wrt_overload_group(current_scope, fun_def.name, fun_def.symbol);

    enter_scope(ScopeType::FUNCTION);
    return_type_stack.push_back(return_type);
    fun_def.parameter_symbols.clear();

    auto define_param = [&](const std::string& name, Object_ptr type, bool is_mutable)
    {
        auto sym = SymbolFactory::create_variable(
            name,
            type,
            is_mutable,
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        current_scope->define(sym);
        fun_def.parameter_symbols.push_back(sym);
    };

    if (is_method && !class_type_stack.empty())
    {
        define_param("my", class_type_stack.back(), false);
    }

    for (size_t i = 0; i < fun_def.parameters.size(); ++i)
    {
        define_param(fun_def.parameters[i].first, param_types[i], true);
    }

    // Analyze Body

    // Mask class context for nested functions
    class_type_stack.push_back(nullptr);

    hoist_statements(fun_def.body);

    for (auto& stmt : fun_def.body)
    {
        visit(stmt);
    }

    class_type_stack.pop_back();
    return_type_stack.pop_back();

    leave_scope();
}

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
            [&](const std::shared_ptr<LocalFunctionType>& t)
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

void SemanticAnalyzer::visit(Return& statement)
{
    Doctor::get().assert(
        !return_type_stack.empty(),
        WaspStage::Semantics,
        "'return' statement used outside of a function."
    );

    Object_ptr expected = return_type_stack.back();
    Object_ptr actual = statement.expression ? visit(statement.expression.value())
                                             : workspace->pool->get_none_type();

    Doctor::get().assert(
        type_checker->assignable(current_scope, expected, actual),
        WaspStage::Semantics,
        "Return type mismatch. Expected " + stringify_object(expected) + ", got " +
            stringify_object(actual)
    );
}

// -------------------------------------------------------------------
// Other Visitors
// -------------------------------------------------------------------

void SemanticAnalyzer::visit(AliasDefinition& statement)
{
}

void SemanticAnalyzer::visit(EnumDefinition& statement)
{
}

void SemanticAnalyzer::visit(AnnotationDefinition& statement)
{
}

} // namespace Wasp
