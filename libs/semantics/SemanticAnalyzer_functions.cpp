#include "Doctor.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"

#include "SymbolScope.h"
#include "Workspace.h"

#include <cstddef>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

#define MAKE_OBJECT_VARIANT(x) std::make_shared<Object>(x)

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void SemanticAnalyzer::visit(FunctionDefinition& function_definition)
{
    Object_ptr return_type = function_definition.return_type
                                 ? visit(function_definition.return_type)
                                 : MAKE_OBJECT_VARIANT(NoneType());

    std::vector<std::string> parameter_names;
    ObjectVector parameter_types;

    if (this->current_bound_instance_type != nullptr)
    {
        parameter_names.push_back("my");
        parameter_types.push_back(this->current_bound_instance_type);
    }

    for (const auto& [parameter_name, type_annotation] : function_definition.parameters)
    {
        Object_ptr parameter_type = MAKE_OBJECT_VARIANT(AnyType());

        if (type_annotation)
        {
            parameter_type = visit(type_annotation);
        }

        parameter_names.push_back(parameter_name);
        parameter_types.push_back(parameter_type);
    }

    auto function_signature = MAKE_OBJECT_VARIANT(FunctionType(parameter_types, return_type));

    Symbol_ptr actual_function_symbol;

    if (function_definition.symbol)
    {
        // Top Level Function: Hoister already defined it.
        actual_function_symbol = function_definition.symbol;
        actual_function_symbol->set_type(function_signature);

        if (this->current_bound_instance_type != nullptr)
        {
            actual_function_symbol->get_payload_as<FunctionData>()
                .bound_instance_type = this->current_bound_instance_type;
        }

        type_checker->validate_overload_group(
            current_scope,
            function_definition.name,
            actual_function_symbol
        );

        function_definition.group_symbol = current_scope->lookup(function_definition.name);
    }
    else
    {
        actual_function_symbol = SymbolFactory::create_function(
            function_definition.name,
            function_signature,
            false,
            this->current_bound_instance_type,
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        // Validate it if an overload group already exists with this name
        if (current_scope->contains_in_current_scope(function_definition.name))
        {
            type_checker->validate_overload_group(
                current_scope,
                function_definition.name,
                actual_function_symbol
            );
        }

        current_scope->define(actual_function_symbol);
        function_definition.symbol = actual_function_symbol;
        function_definition.group_symbol = current_scope->lookup(function_definition.name);
    }

    enter_scope(ScopeType::FUNCTION);
    return_type_stack.push_back(return_type);

    function_definition.parameter_symbols.clear();

    for (size_t i = 0; i < parameter_names.size(); ++i)
    {
        auto parameter_symbol = SymbolFactory::create_variable(
            parameter_names[i],
            parameter_types[i],
            true,
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        current_scope->define(parameter_symbol);
        function_definition.parameter_symbols.push_back(parameter_symbol);
    }

    visit(function_definition.body);

    return_type_stack.pop_back();
    leave_scope();
}

void SemanticAnalyzer::visit(Return& statement)
{
    Doctor::get().assert(
        !return_type_stack.empty(),
        WaspStage::Semantics,
        "'return' statement used outside of a function.");

    Object_ptr expected_type = return_type_stack.back();

    Object_ptr actual_type = statement.expression.has_value() ? visit(statement.expression.value())
                                                              : MAKE_OBJECT_VARIANT(NoneType());

    Doctor::get().assert(
        type_checker->assignable(current_scope, expected_type, actual_type),
        WaspStage::Semantics,
        "Return type mismatch. Expected " + stringify_object(expected_type) + ", got " +
            Wasp::stringify_object(actual_type));
}

} // namespace Wasp
