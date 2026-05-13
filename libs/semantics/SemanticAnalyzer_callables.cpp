#include <cstddef>
#include <memory>
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

void SemanticAnalyzer::analyze_callable(
    AbstractCallable& def,
    ScopeType scope_type,
    Object_ptr context_type,
    bool is_static
)
{
    auto signature = def.symbol->get_type()->as<Signature_ptr>();
    bool has_generics = prepare_generic_scope(signature->generics);

    enter_scope(scope_type);
    return_type_stack.push_back(signature->return_type);

    // Bind Context ('my' or 'our') for Methods
    if (context_type)
    {
        auto context_sym = SymbolFactory::create_variable(
            is_static ? "our" : "my",
            context_type,
            !def.is_pure, // Context is immutable in pure methods
            current_scope->get_closure_depth(),
            current_scope->get_lexical_depth()
        );

        def.context_symbol = current_scope->define(context_sym);
    }

    // Bind Parameters
    def.parameter_symbols.clear();
    for (size_t i = 0; i < def.parameters.size(); ++i)
    {
        auto param_symbol = SymbolFactory::create_variable(
            def.parameters[i].first,
            signature->parameter_types[i],
            !def.is_pure, // Pure callables keep parameters immutable
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

    if (has_generics)
    {
        if (def.symbol->payload_is<FunctionData>())
        {
            auto& data = def.symbol->get_payload_as<FunctionData>();

            data.ast_blueprint = FunctionDefinition(
                def.name,
                def.parameters,
                def.return_type,
                def.body,
                def.is_pure,
                def.template_params
            );

            data.definition_scope = current_scope;
        }
        else if (def.symbol->payload_is<MethodData>())
        {
            auto& data = def.symbol->get_payload_as<MethodData>();

            data.ast_blueprint = MethodDefinition(
                def.name,
                def.parameters,
                def.return_type,
                def.body,
                def.is_pure,
                is_static,
                def.template_params
            );

            data.definition_scope = current_scope;
        }
    }
    else
    {
        visit(def.body);
    }

    return_type_stack.pop_back();
    leave_scope();
    if (has_generics)
    {
        leave_scope();
    }
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
    if (def.fixity == TokenType::INFIX)
    {
        Doctor::get().assert(
            def.parameters.size() == 2,
            WaspStage::Semantics,
            "Infix operator '" + def.name + "' must have 2 parameters."
        );
    }
    else
    {
        Doctor::get().assert(
            def.parameters.size() == 1,
            WaspStage::Semantics,
            "Unary operator '" + def.name + "' must have 1 parameter."
        );
    }

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
    Object_ptr actual = statement.expression
                            ? visit(statement.expression.value())
                            : workspace->pool->get_none_type();

    Doctor::get().assert(
        type_system->assignable(current_scope, expected, actual),
        WaspStage::Semantics,
        "Return type mismatch"
    );
}

void SemanticAnalyzer::visit(Placeholder& statement)
{
    Doctor::get().assert(
        statement.type == TokenType::NATIVE,
        WaspStage::Semantics,
        "At this point, I only expected to see 'native' placeholders"
    );

    Doctor::get().fatal_if_nullptr(
        current_module,
        WaspStage::Semantics,
        "Current module is nullptr while analyzing native statement"
    );

    std::string path = current_module->absolute_filepath.generic_string();

    Doctor::get().assert(
        path.find("/libs/core/") != std::string::npos,
        WaspStage::Semantics,
        "The 'native' keyword is strictly reserved for internal core "
        "libraries."
    );
}

} // namespace Wasp
