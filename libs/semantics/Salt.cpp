#include "Salt.h"
#include "AST.h"
#include "ASTFactory.h"
#include "Doctor.h"
#include "Expression.h"
#include "Statement.h"
#include "Token.h"
#include "TypeAnnotation.h"

#include <ctime>
#include <string>
#include <type_traits>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

StatementVector Salt::visit(const StatementVector statements)
{
    StatementVector updated_statements;

    for (const auto& stmt : statements)
    {
        auto salty = visit(stmt);
        updated_statements.push_back(salty);
    }

    return updated_statements;
}

Statement_ptr Salt::visit(const Statement_ptr statement)
{
    Doctor::get().fatal_if_nullptr(statement, WaspStage::Semantics);

    return std::visit(
        [&](auto& node) -> Statement_ptr
        {
            using T = std::decay_t<decltype(node)>;

            if constexpr (requires { visit(node); })
            {
                return visit(node);
            }
            else
            {
                return statement;
            }
        },
        statement->data
    );
}

static void add_context_params_to_method(
    MethodDefinition& method,
    const std::string& class_name
)
{
    std::string context_name = method.is_static ? "our" : "self";

    auto context_field = ASTFactory::create_field(
        context_name,
        make_type_annotation(TypeIdentifierNode(class_name))
    );

    method.parameters.insert(method.parameters.begin(), context_field);
}

Statement_ptr Salt::visit(ClassDefinition& statement)
{
    for (auto& member : statement.members)
    {
        if (auto* method = member->try_as<MethodDefinition>())
        {
            add_context_params_to_method(*method, statement.name);
        }
    }

    return make_statement(statement);
}

Statement_ptr Salt::visit(TraitDefinition& statement)
{
    for (auto& member : statement.members)
    {
        if (auto* method = member->try_as<MethodDefinition>())
        {
            add_context_params_to_method(*method, statement.name);
        }
    }

    return make_statement(statement);
}

Statement_ptr Salt::visit(OperatorDefinition& def)
{
    size_t expected = def.fixity == TokenType::INFIX ? 2 : 1;

    Doctor::get().assert(
        def.parameters.size() == expected,
        WaspStage::Semantics,
        "Expected " + std::to_string(expected) + " params for operator " +
            def.name
    );

    std::string function_name = get_operator_name(def.fixity, def.op_type);

    auto function_definition = ASTFactory::create_function_definition(
        function_name,
        def.parameters,
        def.return_type,
        def.body,
        def.is_pure,
        def.template_params
    );

    return function_definition;
}

} // namespace Wasp
