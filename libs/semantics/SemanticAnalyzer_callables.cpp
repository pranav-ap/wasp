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
    CallableDefinition& def,
    ScopeType scope_type
)
{
    auto signature = def.symbol->get_type()->as<Signature_ptr>();

    enter_scope(scope_type);
    return_type_stack.push_back(signature->return_type);

    auto template_type = create_template_type(def.template_params);
    define_template_parameters(template_type);

    bind_parameters(def, signature, scope_type);

    if (handle_placeholder(def))
    {
        return_type_stack.pop_back();
        leave_scope();
        return;
    }

    // Analyze function body if not a template
    if (def.template_params.empty())
    {
        visit(def.body);
    }

    return_type_stack.pop_back();
    leave_scope();
}

void SemanticAnalyzer::bind_parameters(
    CallableDefinition& def,
    Signature_ptr signature,
    ScopeType scope_type
)
{
    def.parameter_symbols.clear();

    bool is_pure =
        (scope_type == ScopeType::PURE_FUNCTION ||
         scope_type == ScopeType::PURE_METHOD);

    for (size_t i = 0; i < def.parameters.size(); ++i)
    {
        auto& param = def.parameters[i];
        Object_ptr param_type = signature->parameter_types[i];

        auto symbol = SymbolFactory::create_variable(
            param.name,
            param_type,
            !is_pure, // mutable if not pure
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        def.parameter_symbols.push_back(current_scope->define(symbol));
    }
}

bool SemanticAnalyzer::handle_placeholder(CallableDefinition& def)
{
    auto placeholder = find_placeholder(def.body);

    if (!placeholder.has_value())
    {
        return false;
    }

    // Validate placeholder is alone
    Doctor::get().assert(
        def.body.size() == 1,
        WaspStage::Semantics,
        "The keywords 'native' or 'required' must be the only statement in the "
        "body."
    );

    switch (placeholder.value())
    {
    case TokenType::NATIVE:
        def.symbol->mark_as_native();
        validate_native_location();
        break;

    case TokenType::REQUIRED:
        def.symbol->mark_as_required();
        current_scope->mark_as_required();
        break;

    default:
        Doctor::get().fatal(WaspStage::Semantics, "Invalid placeholder type");
    }

    return true;
}

std::optional<TokenType> SemanticAnalyzer::find_placeholder(
    const StatementVector& body
)
{
    for (const auto& stmt : body)
    {
        if (stmt->is<Placeholder>())
        {
            return stmt->as<Placeholder>().type;
        }
    }

    return std::nullopt;
}

void SemanticAnalyzer::validate_native_location()
{
    Doctor::get().fatal_if_nullptr(
        current_module,
        WaspStage::Semantics,
        "Current module is nullptr while analyzing native statement"
    );

    std::string path = current_module->absolute_filepath.generic_string();
    Doctor::get().assert(
        path.find("/libs/core/") != std::string::npos,
        WaspStage::Semantics,
        "Native blocks are strictly reserved for internal core libraries."
    );
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

    Object_ptr expected = return_type_stack.back()->unwrap_completely();
    Object_ptr actual = workspace->pool->get_none_type();

    if (statement.expression)
    {
        actual = visit(statement.expression.value())->unwrap_completely();
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
        "Expected 'native' or 'required' placeholder, got: " +
            to_string(statement.type)
    );

    if (statement.type == TokenType::REQUIRED)
    {
        current_scope->mark_as_required();
    }
    else if (statement.type == TokenType::NATIVE)
    {
        validate_native_location();
    }
}

} // namespace Wasp
