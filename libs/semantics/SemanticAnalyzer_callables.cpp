#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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

void SemanticAnalyzer::visit(FunctionDefinition& def)
{
    analyze_callable(def, ScopeType::FUNCTION);
}

void SemanticAnalyzer::analyze_callable(
    AbstractCallable& def,
    ScopeType scope_type
)
{
    auto signature = def.symbol->get_type()->as<Signature_ptr>();

    enter_scope(scope_type);
    return_type_stack.push_back(signature->return_type);

    // Handle template parameters
    if (signature->template_type)
    {
        for (const auto& name :
             signature->template_type->ordered_parameter_names)
        {
            auto generic_type_obj = signature->template_type
                                        ->template_parameters.at(name);

            Doctor::get().assert(
                generic_type_obj->is<GenericType_ptr>(),
                WaspStage::Semantics,
                "Expected GenericType for template parameter: " + name
            );

            auto generic_type = generic_type_obj->as<GenericType_ptr>();
            auto symbol = SymbolFactory::create_template_parameter(
                name,
                generic_type->constraint_type
            );

            current_scope->define(symbol);
        }
    }

    // Bind Parameters
    def.parameter_symbols.clear();

    for (size_t i = 0; i < def.parameters.size(); ++i)
    {
        Object_ptr actual_type = signature->parameter_types[i];

        bool is_mutable =
            (scope_type != ScopeType::PURE_FUNCTION &&
             scope_type != ScopeType::PURE_METHOD);

        auto param_symbol = SymbolFactory::create_variable(
            def.parameters[i].name,
            actual_type,
            is_mutable,
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        def.parameter_symbols.push_back(current_scope->define(param_symbol));
    }

    // Lonely Placeholder Rule (native, required)
    std::optional<TokenType> placeholder;

    for (const auto& stmt : def.body)
    {
        if (stmt->is<Placeholder>())
        {
            placeholder = stmt->as<Placeholder>().type;
            break;
        }
    }

    if (placeholder.has_value())
    {
        Doctor::get().assert(
            def.body.size() == 1,
            WaspStage::Semantics,
            "The keywords 'native', 'pass' or 'required' must be the only "
            "statement in the body."
        );

        if (placeholder == TokenType::NATIVE)
        {
            def.symbol->mark_as_native();
        }
        else if (placeholder == TokenType::REQUIRED)
        {
            def.symbol->mark_as_required();
        }
    }

    // Only analyze body if no placeholder and no template parameters
    if (def.template_params.empty())
    {
        visit(def.body);
    }

    return_type_stack.pop_back();
    leave_scope();
}

void SemanticAnalyzer::visit(Return& statement)
{
    if (current_scope->is_required())
    {
        return;
    }

    Doctor::get().assert(
        !return_type_stack.empty(),
        WaspStage::Semantics,
        "'return' statement used outside of a function."
    );

    Object_ptr expected = return_type_stack.back();
    expected = expected->unwrap_completely();

    Object_ptr actual = workspace->pool->get_none_type();

    if (statement.expression)
    {
        actual = visit(statement.expression.value());
        actual = actual->unwrap_completely();
    }

    Doctor::get().assert(
        type_system->assignable(current_scope, expected, actual),
        WaspStage::Semantics,
        "Return type mismatch: expected '" + expected->to_string() +
            "', got '" + actual->to_string() + "'"
    );
}

void SemanticAnalyzer::visit(Placeholder& statement)
{
    Doctor::get().assert(
        statement.type == TokenType::NATIVE ||
            statement.type == TokenType::REQUIRED,
        WaspStage::Semantics,
        "Expected 'native', or 'required' placeholder"
    );

    if (statement.type == TokenType::REQUIRED)
    {
        current_scope->mark_as_required();
    }

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
            "The 'native' keyword is strictly reserved for internal core "
            "libraries."
        );
    }
}

} // namespace Wasp
