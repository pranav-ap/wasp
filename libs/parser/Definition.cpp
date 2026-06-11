#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Parser.h"
#include "Statement.h"
#include "Token.h"
#include "TypeAnnotation.h"

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
// Templates
// ============================================================================

Statement_ptr Parser::parse_template_definition(int indent_level)
{
    token_pipe.advance_pointer(); // Consume 'template'
    token_pipe.require_in_line(TokenType::EOL);

    FieldVector generics;
    const int body_indent = indent_level + 1;

    while (true)
    {
        token_pipe.ignore_empty_lines();

        if (token_pipe.lookahead_indents() != body_indent)
        {
            break;
        }

        token_pipe.expect_n_indents(body_indent);

        bool is_variadic = token_pipe.consume_optional(TokenType::DOT_DOT_DOT)
                               .has_value();

        auto name_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
        token_pipe.require_in_line(TokenType::COLON);

        auto param_type = parse_type();
        token_pipe.require_in_line(TokenType::EOL);

        generics.emplace_back(name_token.lexeme, param_type, is_variadic);
    }

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
            [&](TypeDefinition& def)
            {
                def.generics = generics;
            },
            [&](TypeAliasDefinition& def)
            {
                def.generics = generics;
            },
            [&](auto&)
            {
                Doctor::get().fatal(
                    WaspStage::Parser,
                    "Invalid template target"
                );
            }
        },
        target->data
    );

    return target;
}

// ============================================================================
// Types & Enums
// ============================================================================

Statement_ptr Parser::parse_type_alias_definition()
{
    token_pipe.advance_pointer();

    auto name_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
    auto name = name_token.lexeme;

    token_pipe.require_in_line(TokenType::EQUAL);

    auto ref_type = parse_type();
    token_pipe.require_in_line(TokenType::EOL);

    return make_statement(TypeAliasDefinition(name, ref_type));
}

Statement_ptr Parser::parse_enum_definition(int indent_level)
{
    token_pipe.advance_pointer();

    Token identifier = token_pipe.require_in_line(TokenType::IDENTIFIER);
    token_pipe.require_in_line(TokenType::EOL);

    return make_statement(parse_enum_body(identifier.lexeme, indent_level + 1));
}

EnumDefinition Parser::parse_enum_body(std::string name, int indent_level)
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
                parse_enum_body(nested_name, indent_level + 1)
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

    return EnumDefinition(std::move(name), members, nested_enums);
}

// ============================================================================
// Callables
// ============================================================================

Statement_ptr Parser::parse_function_definition(
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

    TypeAnnotation_ptr return_type = nullptr;
    if (token_pipe.consume_optional_in_line(TokenType::ARROW))
    {
        return_type = parse_type();
    }

    token_pipe.require_in_line(TokenType::EOL);
    Block body = parse_block(indent_level + 1);

    return make_statement(FunctionDefinition(
        std::move(name),
        std::move(parameters),
        std::move(return_type),
        std::move(body),
        is_pure
    ));
}

Statement_ptr Parser::parse_operator_definition(
    TokenType fixity,
    int indent_level
)
{
    auto operator_token = token_pipe.current_in_line();
    Doctor::get().fatal_if_nullopt(operator_token, WaspStage::Parser);
    token_pipe.advance_pointer();

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

    TypeAnnotation_ptr return_type = nullptr;
    if (token_pipe.consume_optional_in_line(TokenType::ARROW))
    {
        return_type = parse_type();
    }

    token_pipe.require_in_line(TokenType::EOL);
    Block body = parse_block(indent_level + 1);

    return make_statement(OperatorDefinition(
        fixity,
        operator_token->type,
        get_operator_name(fixity, operator_token->type),
        std::move(parameters),
        std::move(return_type),
        std::move(body)
    ));
}

// ============================================================================
// OOP Definitions
// ============================================================================

std::tuple<std::string, TypeAnnotationVector, StatementVector> Parser::
    parse_membered_definition_base(int indent_level)
{
    token_pipe.advance_pointer();

    auto name_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
    std::string name = name_token.lexeme;

    TypeAnnotationVector traits;

    if (token_pipe.consume_optional_in_line(TokenType::IS))
    {
        auto t = parse_type();

        if (t->is<std::shared_ptr<IntersectionTypeNode>>())
        {
            auto& intersection = t->as<std::shared_ptr<IntersectionTypeNode>>();
            traits = std::move(intersection->types);
        }
        else
        {
            traits.push_back(std::move(t));
        }
    }

    token_pipe.require_in_line(TokenType::EOL);

    StatementVector members;
    const int body_indent = indent_level + 1;

    while (true)
    {
        token_pipe.ignore_empty_lines();

        if (token_pipe.lookahead_indents() != body_indent)
        {
            break;
        }

        token_pipe.expect_n_indents(body_indent);

        if (token_pipe.consume_optional(TokenType::COMMENT))
        {
            token_pipe.advance_pointer();
            continue;
        }

        if (token_pipe.consume_optional(TokenType::FUN))
        {
            members.push_back(
                parse_function_definition(body_indent, true, false, false)
            );
        }
        else if (token_pipe.consume_optional(TokenType::PURE))
        {
            token_pipe.require_in_line(TokenType::FUN);
            members.push_back(
                parse_function_definition(body_indent, true, false, true)
            );
        }
        else if (token_pipe.consume_optional(TokenType::SHARE))
        {
            bool is_pure = token_pipe.consume_optional_in_line(TokenType::PURE)
                               .has_value();
            token_pipe.require_in_line(TokenType::FUN);
            members.push_back(
                parse_function_definition(body_indent, true, true, is_pure)
            );
        }
        else
        {
            auto [member_name, member_type] = parse_name_type_pair();
            members.push_back(make_statement(Field(member_name, member_type)));
        }
    }

    return {std::move(name), std::move(traits), std::move(members)};
}

Statement_ptr Parser::parse_class_definition(int indent_level)
{

    auto [name, traits, members] = parse_membered_definition_base(indent_level);
    return make_statement(TypeDefinition(name, traits, members));
}

Statement_ptr Parser::parse_trait_definition(int indent_level)
{
    auto [name, traits, members] = parse_membered_definition_base(indent_level);
    return make_statement(TypeDefinition(name, traits, members));
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

std::pair<std::string, TypeAnnotation_ptr> Parser::parse_name_type_pair()
{
    auto name_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
    std::string name = name_token.lexeme;

    token_pipe.require_in_line(TokenType::COLON);

    auto type = parse_type();
    token_pipe.require_in_line(TokenType::EOL);

    return {name, type};
}

StatementVector Parser::parse_name_type_block(int expected_indent)
{
    StatementVector members;

    while (true)
    {
        token_pipe.ignore_empty_lines();

        if (token_pipe.lookahead_indents() != expected_indent)
        {
            break;
        }

        token_pipe.expect_n_indents(expected_indent);
        auto [member_name, member_type] = parse_name_type_pair();
        members.push_back(make_statement(Field(member_name, member_type)));
    }

    return members;
}

Field Parser::parse_field()
{
    auto name_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
    std::string name = name_token.lexeme;

    token_pipe.require_in_line(TokenType::COLON);

    auto type = parse_type();

    bool is_variadic = token_pipe
                           .consume_optional_in_line(TokenType::DOT_DOT_DOT)
                           .has_value();

    token_pipe.require_in_line(TokenType::EOL);

    return Field(name, type, is_variadic);
}

FieldVector Parser::parse_field_vector()
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
