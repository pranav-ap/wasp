#include "Doctor.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "Symbol.h"
#include "SymbolScope.h"

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

void SemanticAnalyzer::visit(FunctionDefinition& func_def)
{
    Object_ptr resolved_return_type = func_def.return_type ? visit(func_def.return_type)
                                                           : MAKE_OBJECT_VARIANT(NoneType());

    ObjectVector parameter_types;
    std::vector<std::string> parameter_names;

    // Resolve Parameter Types
    for (const auto& [param_name, type_annotation] : func_def.parameters)
    {
        Object_ptr resolved_param_type = MAKE_OBJECT_VARIANT(AnyType());

        if (type_annotation)
        {
            resolved_param_type = visit(type_annotation);
        }

        parameter_names.push_back(param_name);
        parameter_types.push_back(resolved_param_type);
    }

    type_checker->validate_new_overload(current_scope, func_def.name, parameter_types);

    auto function_signature = MAKE_OBJECT_VARIANT(
        FunctionType(parameter_types, resolved_return_type));

    if (func_def.symbol)
    {
        // Top Level Function, so we update the hoisted symbol created by the Hoister
        func_def.symbol->set_type(function_signature);
    }
    else
    {
        // Local nested function, so we create a new symbol
        auto local_symbol = SymbolFactory::create_function(
            func_def.name,
            function_signature,
            false,
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth());

        current_scope->define(local_symbol);
        func_def.symbol = local_symbol;
    }

    enter_scope(ScopeType::FUNCTION);
    return_type_stack.push_back(resolved_return_type);

    // Define Parameters as Local Variables in the New Scope
    for (size_t i = 0; i < parameter_names.size(); ++i)
    {
        auto param_symbol = SymbolFactory::create_variable(
            parameter_names[i],
            parameter_types[i],
            true,
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth());

        current_scope->define(param_symbol);
    }

    visit(func_def.body);

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
        "Return type mismatch. Expected " + Wasp::stringify_object(expected_type) + ", got " +
            Wasp::stringify_object(actual_type));
}

} // namespace Wasp
