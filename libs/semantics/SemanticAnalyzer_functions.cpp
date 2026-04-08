#include "AST.h"
#include "Doctor.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Workspace.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Wasp
{

std::pair<Object_ptr, ObjectVector> SemanticAnalyzer::evaluate_signature(FunctionDefinition& func)
{
    Object_ptr return_type = func.return_type ? visit(func.return_type) : make_object(NoneType());
    ObjectVector param_types;

    for (const auto& [param_name, type_ann] : func.parameters)
    {
        param_types.push_back(type_ann ? visit(type_ann) : make_object(AnyType()));
    }

    return {return_type, param_types};
}

void SemanticAnalyzer::evaluate_function_definition_body(StatementVector statements)
{
    // PASS 1: Hoist Local Functions
    for (auto& stmt : statements)
    {
        if (!stmt->is<FunctionDefinition>())
            continue;

        auto& nested_func = stmt->as<FunctionDefinition>();
        auto [ret_type, param_types] = evaluate_signature(nested_func);
        auto signature = make_object(std::make_shared<FunctionType>(param_types, ret_type));

        auto local_func_symbol = SymbolFactory::create_function(
            nested_func.name,
            signature,
            false,
            nullptr,
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        if (current_scope->contains_in_current_scope(nested_func.name))
        {
            type_checker
                ->validate_overload_group(current_scope, nested_func.name, local_func_symbol);
        }

        current_scope->define(local_func_symbol);
        nested_func.symbol = local_func_symbol;
        nested_func.group_symbol = current_scope->lookup(nested_func.name);
    }

    // PASS 2: Analyse all statements in the function body
    for (auto& stmt : statements)
    {
        visit(stmt);
    }
}

void SemanticAnalyzer::visit(FunctionDefinition& func)
{
    auto [return_type, param_types] = evaluate_signature(func);
    auto signature = make_object(std::make_shared<FunctionType>(param_types, return_type));

    if (!func.symbol)
    {
        func.symbol = SymbolFactory::create_function(
            func.name,
            signature,
            false,
            current_my_instance_type,
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        if (current_scope->contains_in_current_scope(func.name))
        {
            type_checker->validate_overload_group(current_scope, func.name, func.symbol);
        }

        current_scope->define(func.symbol);
    }
    else
    {
        func.symbol->set_type(signature);

        if (current_my_instance_type)
        {
            func.symbol->get_payload_as<FunctionData>()
                .bound_instance_type = current_my_instance_type;
        }

        type_checker->validate_overload_group(current_scope, func.name, func.symbol);
    }

    func.group_symbol = current_scope->lookup(func.name);
    Doctor::get().fatal_if_nullptr(func.group_symbol, WaspStage::Semantics);

    // Prepare Scope
    enter_scope(ScopeType::FUNCTION);
    return_type_stack.push_back(return_type);
    func.parameter_symbols.clear();

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
        func.parameter_symbols.push_back(sym);
    };

    if (current_my_instance_type)
    {
        define_param("my", current_my_instance_type, false);
    }

    for (size_t i = 0; i < func.parameters.size(); ++i)
    {
        define_param(func.parameters[i].first, param_types[i], true);
    }

    // Evaluate Body
    Object_ptr prev_bound_type = current_my_instance_type;
    current_my_instance_type = nullptr;

    evaluate_function_definition_body(func.body);

    current_my_instance_type = prev_bound_type;
    return_type_stack.pop_back();
    leave_scope();
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
                                             : make_object(NoneType());

    Doctor::get().assert(
        type_checker->assignable(current_scope, expected, actual),
        WaspStage::Semantics,
        "Return type mismatch. Expected " + stringify_object(expected) + ", got " +
            stringify_object(actual)
    );
}

} // namespace Wasp
