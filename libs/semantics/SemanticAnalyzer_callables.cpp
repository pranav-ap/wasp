#include <cstddef>
#include <string>

#include "AST.h"
#include "Doctor.h"
#include "Objects.h"
#include "SemanticAnalyzer.h"
#include "Statement.h"
#include "SymbolScope.h"
#include "Token.h"
#include "Workspace.h"

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

void SemanticAnalyzer::analyze_callable(
    AbstractCallable& def,
    ScopeType scope_type,
    Object_ptr context_type,
    bool is_static
)
{
    auto signature = def.symbol->get_type()->as<Signature_ptr>();

    enter_scope(scope_type);
    return_type_stack.push_back(signature->return_type);

    auto [template_params, ordered_names] = evaluate_template_params(
        def.template_params
    );

    for (const auto& [name, generic_type] : template_params)
    {
        auto symbol = SymbolFactory::create_template_parameter(name, generic_type);
        current_scope->define(symbol);
    }

    // Bind Context ('my' or 'our') for Methods
    if (context_type)
    {
        auto context_sym = SymbolFactory::create_variable(
            is_static ? "our" : "my",
            context_type,
            !def.is_pure,
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        def.context_symbol = current_scope->define(context_sym);
    }

    // Bind Parameters
    def.parameter_symbols.clear();

    for (size_t i = 0; i < def.parameters.size(); ++i)
    {
        Object_ptr actual_type = visit(def.parameters[i].second);

        auto param_symbol = SymbolFactory::create_variable(
            def.parameters[i].first,
            actual_type,
            !def.is_pure,
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        def.parameter_symbols.push_back(current_scope->define(param_symbol));
    }

    if (def.body.size() == 1 && def.body.front()->is<Placeholder>())
    {
        if (def.body.front()->as<Placeholder>().type == TokenType::NATIVE)
        {
            def.symbol->mark_as_native();
        }
    }

    if (def.template_params.empty())
    {
        visit(def.body);
    }

    return_type_stack.pop_back();
    leave_scope();
}

void SemanticAnalyzer::visit(FunctionDefinition& def)
{
    analyze_callable(
        def,
        def.is_pure ? ScopeType::PURE_FUNCTION : ScopeType::FUNCTION,
        nullptr,
        false
    );
}

void SemanticAnalyzer::visit(OperatorDefinition& def)
{
    size_t expected = def.fixity == TokenType::INFIX ? 2 : 1;

    Doctor::get().assert(
        def.parameters.size() == expected,
        WaspStage::Semantics,
        "Operator '" + def.name + "' must have " + std::to_string(expected) +
            " parameter(s)."
    );

    analyze_callable(def, ScopeType::PURE_FUNCTION, nullptr, false);
}

void SemanticAnalyzer::visit(Return& statement)
{
    Doctor::get().assert(
        !return_type_stack.empty(),
        WaspStage::Semantics,
        "'return' statement used outside of a function."
    );

    Object_ptr expected = return_type_stack.back();
    expected = unwrap_completely(expected);

    Object_ptr actual = workspace->pool->get_none_type();

    if (statement.expression)
    {
        actual = visit(statement.expression.value());
        actual = unwrap_completely(actual);
    }

    Doctor::get().assert(
        type_system->assignable(current_scope, expected, actual),
        WaspStage::Semantics,
        "Return type mismatch"
    );
}

void SemanticAnalyzer::visit(Placeholder& statement)
{
    Doctor::get().assert(
        statement.type == TokenType::NATIVE || statement.type == TokenType::PASS ||
            statement.type == TokenType::REQUIRED,
        WaspStage::Semantics,
        "Expected 'native', 'pass', or 'required' placeholder"
    );

    Doctor::get().fatal_if_nullptr(
        current_module,
        WaspStage::Semantics,
        "Current module is nullptr while analyzing native statement"
    );

    if (statement.type == TokenType::NATIVE)
    {
        std::string path = current_module->absolute_filepath.generic_string();

        Doctor::get().assert(
            path.find("/libs/core/") != std::string::npos,
            WaspStage::Semantics,
            "The 'native' keyword is strictly reserved for internal core libraries."
        );
    }
}

} // namespace Wasp
