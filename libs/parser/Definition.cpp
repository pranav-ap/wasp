#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Parser.h"
#include "Statement.h"
#include "Token.h"
#include "TypeNode.h"

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
// Templates
// ============================================================================

Statement_ptr Parser::parse_template_definition(int indent_level)
{
    token_pipe.advance_pointer(); // Consume 'template'
    token_pipe.require_in_line(TokenType::EOL);

    FieldVector generics = parse_fields(indent_level + 1);
    Statement_ptr target = parse_statement(indent_level);

    std::visit(
        overloaded{
            [&](FunctionDefinition& def)
            {
                def.generics = generics;
            },
            [&](OperatorDefinition& def)
            {
                def.generics = generics;
            },
            [&](ClassDefinition& def)
            {
                def.generics = generics;
            },
            [&](TraitDefinition& def)
            {
                def.generics = generics;
            },
            [&](PrimitiveDefinition& def)
            {
                def.generics = generics;
            },
            [&](TypeAliasDefinition& def)
            {
                def.generics = generics;
            },

            [&](auto&)
            {
                Doctor::parser().fatal("Invalid template target");
            }
        },
        target->data
    );

    return target;
}

// ============================================================================
// Type Alias
// ============================================================================

Statement_ptr Parser::parse_type_alias_definition()
{
    token_pipe.advance_pointer();

    auto name_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
    auto name = name_token.lexeme;

    token_pipe.require_in_line(TokenType::EQUAL);

    auto ref_type = parse_type();
    token_pipe.require_in_line(TokenType::EOL);

    return make_statement(
        TypeAliasDefinition{std::move(name), FieldVector{}, std::move(ref_type)}
    );
}

// ============================================================================
// Enums
// ============================================================================

Statement_ptr Parser::parse_enum_definition(int indent_level)
{
    token_pipe.advance_pointer();

    Token identifier = token_pipe.require_in_line(TokenType::IDENTIFIER);
    token_pipe.require_in_line(TokenType::EOL);

    return make_statement(
        parse_enum_body(identifier.lexeme, FieldVector{}, indent_level + 1)
    );
}

EnumDefinition Parser::parse_enum_body(
    std::string name,
    FieldVector generics,
    int indent_level
)
{
    std::vector<std::string> members;
    std::vector<EnumDefinition> nested_enums;

    while (true)
    {
        token_pipe.ignore_empty_lines();

        if (token_pipe.lookahead_indents() != indent_level)
        {
            break;
        }

        token_pipe.expect_n_indents(indent_level);

        if (token_pipe.consume_optional(TokenType::ENUM))
        {
            auto nested_name = token_pipe.require_in_line(TokenType::IDENTIFIER)
                                   .lexeme;

            token_pipe.require_in_line(TokenType::EOL);

            nested_enums.push_back(
                parse_enum_body(nested_name, generics, indent_level + 1)
            );

            continue;
        }

        if (auto token = token_pipe.consume_optional(TokenType::IDENTIFIER))
        {
            members.push_back(token->lexeme);
            token_pipe.require_in_line(TokenType::EOL);
            continue;
        }

        break;
    }

    return EnumDefinition{std::move(name), generics, members, nested_enums};
}

// ============================================================================
// Callables
// ============================================================================

Statement_ptr Parser::parse_function_definition(int indent_level, bool is_pure)
{
    auto name = token_pipe.require_in_line(TokenType::IDENTIFIER).lexeme;

    token_pipe.require_in_line(TokenType::OPEN_PARENTHESIS);
    std::vector<Field> parameters;

    if (!token_pipe.consume_optional(TokenType::CLOSE_PARENTHESIS))
    {
        do
        {
            auto param_name = token_pipe.require_in_line(TokenType::IDENTIFIER)
                                  .lexeme;
            token_pipe.require_in_line(TokenType::COLON);
            auto param_type = parse_type();
            parameters.emplace_back(param_name, param_type);
        }
        while (token_pipe.consume_optional_in_line(TokenType::COMMA));

        token_pipe.require_in_line(TokenType::CLOSE_PARENTHESIS);
    }

    TypeNode_ptr return_type = nullptr;

    if (token_pipe.consume_optional_in_line(TokenType::ARROW))
    {
        return_type = parse_type();
    }

    token_pipe.require_in_line(TokenType::EOL);
    Block body = parse_block(indent_level + 1);

    return make_statement(FunctionDefinition(
        std::move(name),
        FieldVector{},
        std::move(parameters),
        std::move(return_type),
        std::move(body),
        is_pure
    ));
}

Statement_ptr Parser::parse_method_definition(
    int indent_level,
    bool is_shared,
    bool is_pure
)
{
    auto name = token_pipe.require_in_line(TokenType::IDENTIFIER).lexeme;

    token_pipe.require_in_line(TokenType::OPEN_PARENTHESIS);
    std::vector<Field> parameters;

    if (!token_pipe.consume_optional(TokenType::CLOSE_PARENTHESIS))
    {
        do
        {
            auto param_name = token_pipe.require_in_line(TokenType::IDENTIFIER)
                                  .lexeme;
            token_pipe.require_in_line(TokenType::COLON);
            auto param_type = parse_type();
            parameters.emplace_back(param_name, param_type);
        }
        while (token_pipe.consume_optional_in_line(TokenType::COMMA));

        token_pipe.require_in_line(TokenType::CLOSE_PARENTHESIS);
    }

    TypeNode_ptr return_type = nullptr;

    if (token_pipe.consume_optional_in_line(TokenType::ARROW))
    {
        return_type = parse_type();
    }

    token_pipe.require_in_line(TokenType::EOL);
    Block body = parse_block(indent_level + 1);

    return make_statement(MethodDefinition(
        std::move(name),
        std::move(parameters),
        std::move(return_type),
        std::move(body),
        is_pure,
        is_shared
    ));
}

Statement_ptr Parser::parse_operator_definition(
    TokenType fixity,
    int indent_level
)
{
    auto operator_token = token_pipe.current_in_line();
    Doctor::parser().fatal_if_nullopt(operator_token);
    token_pipe.advance_pointer();

    token_pipe.require_in_line(TokenType::OPEN_PARENTHESIS);
    std::vector<Field> operands;

    if (!token_pipe.consume_optional(TokenType::CLOSE_PARENTHESIS))
    {
        do
        {
            auto param_name = token_pipe.require_in_line(TokenType::IDENTIFIER)
                                  .lexeme;
            token_pipe.require_in_line(TokenType::COLON);
            auto param_type = parse_type();
            operands.emplace_back(param_name, param_type);
        }
        while (token_pipe.consume_optional_in_line(TokenType::COMMA));

        token_pipe.require_in_line(TokenType::CLOSE_PARENTHESIS);
    }

    TypeNode_ptr return_type = nullptr;
    if (token_pipe.consume_optional_in_line(TokenType::ARROW))
    {
        return_type = parse_type();
    }

    token_pipe.require_in_line(TokenType::EOL);
    Block block = parse_block(indent_level + 1);

    std::string name = get_operator_name(fixity, operator_token->type);

    return make_statement(OperatorDefinition(
        name,
        FieldVector{},
        fixity,
        operator_token->type,
        std::move(operands),
        std::move(return_type),
        std::move(block)
    ));
}

// ============================================================================
// OOPS
// ============================================================================

std::tuple<std::string, TypeNodeVector, MethodDefinitionVector, FieldVector>
Parser::parse_membered_definition_base(int indent_level)
{
    token_pipe.advance_pointer();

    auto name_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
    std::string name = name_token.lexeme;

    TypeNodeVector traits;

    if (token_pipe.consume_optional_in_line(TokenType::IS))
    {
        auto t = parse_type();

        if (t->is<IntersectionTypeNode>())
        {
            auto& intersection = t->as<IntersectionTypeNode>();
            traits = std::move(intersection.types);
        }
        else
        {
            traits.push_back(std::move(t));
        }
    }

    token_pipe.require_in_line(TokenType::EOL);

    MethodDefinitionVector methods;
    FieldVector fields;

    const int BODY_INDENT = indent_level + 1;

    while (true)
    {
        token_pipe.ignore_empty_lines();

        if (token_pipe.lookahead_indents() != BODY_INDENT)
        {
            break;
        }

        token_pipe.expect_n_indents(BODY_INDENT);

        if (token_pipe.consume_optional(TokenType::FUN))
        {
            auto fun = parse_method_definition(BODY_INDENT, false, false);
            auto fun_def = fun->as<MethodDefinition>();
            methods.push_back(fun_def);
        }
        else if (token_pipe.consume_optional(TokenType::PURE))
        {
            token_pipe.require_in_line(TokenType::FUN);

            auto fun = parse_method_definition(BODY_INDENT, false, true);
            auto fun_def = fun->as<MethodDefinition>();
            methods.push_back(fun_def);
        }
        else if (token_pipe.consume_optional(TokenType::SHARE))
        {
            bool is_pure = token_pipe.consume_optional_in_line(TokenType::PURE)
                               .has_value();

            token_pipe.require_in_line(TokenType::FUN);

            auto fun = parse_method_definition(BODY_INDENT, true, is_pure);
            auto fun_def = fun->as<MethodDefinition>();
            methods.push_back(fun_def);
        }
        else
        {
            Field field = parse_field();
            fields.push_back(std::move(field));
        }
    }

    return {
        std::move(name),
        std::move(traits),
        std::move(methods),
        std::move(fields)
    };
}

Statement_ptr Parser::parse_class_definition(int indent_level)
{
    auto [name, traits, methods, fields] = parse_membered_definition_base(
        indent_level
    );

    return make_statement(
        ClassDefinition(name, FieldVector{}, fields, methods, traits)
    );
}

Statement_ptr Parser::parse_trait_definition(int indent_level)
{
    auto [name, traits, methods, fields] = parse_membered_definition_base(
        indent_level
    );

    return make_statement(
        TraitDefinition(name, FieldVector{}, fields, methods, traits)
    );
}

Statement_ptr Parser::parse_primitive_definition(int indent_level)
{
    auto [name, traits, methods, fields] = parse_membered_definition_base(
        indent_level
    );

    return make_statement(
        PrimitiveDefinition(name, FieldVector{}, fields, methods, traits)
    );
}

// ============================================================================
// Return
// ============================================================================

Statement_ptr Parser::parse_return_statement()
{
    token_pipe.advance_pointer();

    if (token_pipe.consume_optional_in_line(TokenType::EOL))
    {
        return make_statement(Return{});
    }

    token_pipe.ignore_spaces_tabs();

    auto expression = parse_expression();
    token_pipe.require_in_line(TokenType::EOL);
    return make_statement(Return{expression});
}

// ============================================================================
// Helpers
// ============================================================================

Field Parser::parse_field()
{
    bool is_variadic = token_pipe
                           .consume_optional_in_line(TokenType::DOT_DOT_DOT)
                           .has_value();

    auto name_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
    std::string name = name_token.lexeme;

    token_pipe.require_in_line(TokenType::COLON);

    auto type = parse_type();

    token_pipe.require_in_line(TokenType::EOL);

    return Field(name, type, is_variadic);
}

FieldVector Parser::parse_fields(int expected_indent)
{
    FieldVector fields;

    while (true)
    {
        token_pipe.ignore_empty_lines();

        if (token_pipe.lookahead_indents() != expected_indent)
        {
            break;
        }

        token_pipe.expect_n_indents(expected_indent);
        Field field = parse_field();
        fields.push_back(std::move(field));
    }

    return fields;
}

FieldVector Parser::parse_parameters()
{
    FieldVector fields;

    do
    {
        fields.push_back(parse_field());
    }
    while (token_pipe.consume_optional_in_line(TokenType::COMMA));

    return fields;
}

} // namespace Wasp
