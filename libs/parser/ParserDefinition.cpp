#include "AST.h"
#include "Parser.h"
#include "Statement.h"
#include "Token.h"
#include "TypeAnnotation.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#define CASE(token_type, call)                                                                     \
    case token_type: {                                                                             \
        return call;                                                                               \
    }

#define MAKE_TYPE(x) std::make_shared<TypeAnnotation>(x)
#define MAKE_RECURSIVE_TYPE(T, ...)                                                                \
    std::make_shared<TypeAnnotation>(std::make_shared<T>(__VA_ARGS__))

using std::make_pair;

namespace Wasp
{
Statement_ptr Parser::parse_variable_definition(bool is_mutable)
{
    token_pipe.advance_pointer();

    auto expression = parse_expression();
    token_pipe.require_in_line(TokenType::EOL);

    return make_statement(VariableDefinition(expression, is_mutable));
}

Statement_ptr Parser::parse_alias_definition()
{
    token_pipe.advance_pointer();

    auto name_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
    auto name = name_token.value;

    token_pipe.require_in_line(TokenType::EQUAL);

    auto ref_type = parse_type();
    token_pipe.require_in_line(TokenType::EOL);

    return make_statement(TypeAliasDefinition(name, ref_type));
}

// Enum

Statement_ptr Parser::parse_enum_definition(int indent_level)
{
    token_pipe.advance_pointer();

    Token identifier = token_pipe.require_in_line(TokenType::IDENTIFIER);
    token_pipe.require_in_line(TokenType::EOL);

    return make_statement(parse_enum_body(identifier.value, indent_level + 1));
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
            auto nested_name = token_pipe.require_in_line(TokenType::IDENTIFIER).value;
            token_pipe.require_in_line(TokenType::EOL);

            nested_enums.push_back(parse_enum_body(nested_name, indent_level + 1));
            continue;
        }

        if (auto token = token_pipe.consume_optional(TokenType::IDENTIFIER))
        {
            members.push_back(token->value);
            token_pipe.require_in_line(TokenType::EOL);
            continue;
        }

        break;
    }

    return EnumDefinition(std::move(name), members, nested_enums);
}

// Others

Statement_ptr Parser::parse_annotation_definition()
{
    token_pipe.advance_pointer();

    auto name_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
    auto name = name_token.value;

    if (token_pipe.consume_optional_in_line(TokenType::OPEN_PARENTHESIS))
    {
        std::vector<Expression_ptr> args = parse_expressions();
        token_pipe.require_in_line(TokenType::CLOSE_PARENTHESIS);
        return make_statement(AnnotationDefinition(name, args));
    }

    return make_statement(AnnotationDefinition(name, {}));
}

Statement_ptr Parser::parse_function_definition(
    int indent_level,
    bool in_class_block,
    bool is_our,
    bool is_pure
)
{
    auto name_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
    auto name = name_token.value;

    token_pipe.require_in_line(TokenType::OPEN_PARENTHESIS);

    std::vector<std::pair<std::string, TypeAnnotation_ptr>> parameters;

    if (!token_pipe.consume_optional(TokenType::CLOSE_PARENTHESIS))
    {
        while (true)
        {
            auto param_name_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
            auto param_name = param_name_token.value;

            token_pipe.require_in_line(TokenType::COLON);

            auto param_type = parse_type();

            parameters.push_back(make_pair(param_name, param_type));

            if (token_pipe.consume_optional(TokenType::CLOSE_PARENTHESIS))
            {
                break;
            }

            token_pipe.require_in_line(TokenType::COMMA);
        }
    }

    TypeAnnotation_ptr return_type = nullptr;
    if (token_pipe.consume_optional_in_line(TokenType::ARROW))
    {
        return_type = parse_type();
    }

    token_pipe.require_in_line(TokenType::EOL);

    StatementVector body = parse_statements_block(indent_level + 1);

    if (in_class_block)
    {
        if (!is_our && is_pure)
        {
            return make_statement(PureMethodDefinition(name, parameters, return_type, body));
        }
        if (is_our && !is_pure)
        {
            return make_statement(OurMethodDefinition(name, parameters, return_type, body));
        }
        if (is_our && is_pure)
        {
            return make_statement(OurPureMethodDefinition(name, parameters, return_type, body));
        }

        return make_statement(MethodDefinition(name, parameters, return_type, body));
    }

    if (is_pure)
    {
        return make_statement(PureFunctionDefinition(name, parameters, return_type, body));
    }

    return make_statement(FunctionDefinition(name, parameters, return_type, body));
}

std::tuple<std::string, std::vector<std::string>, StatementVector> Parser::
    parse_membered_definition_base(int indent_level)
{
    token_pipe.advance_pointer();

    auto name_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
    std::string name = name_token.value;

    std::vector<std::string> traits;

    if (token_pipe.consume_optional_in_line(TokenType::IS))
    {
        do
        {
            auto trait_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
            traits.push_back(trait_token.value);
        }
        while (token_pipe.consume_optional_in_line(TokenType::AMPERSAND));
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

        if (token_pipe.consume_optional(TokenType::FUN))
        {
            members.push_back(parse_function_definition(body_indent, true, false, false));
        }
        else if (token_pipe.consume_optional(TokenType::PURE))
        {
            token_pipe.require_in_line(TokenType::FUN);
            members.push_back(parse_function_definition(body_indent, true, false, true));
        }
        else if (token_pipe.consume_optional(TokenType::OUR))
        {
            bool is_pure = token_pipe.consume_optional_in_line(TokenType::PURE).has_value();
            token_pipe.require_in_line(TokenType::FUN);
            members.push_back(parse_function_definition(body_indent, true, true, is_pure));
        }
        else
        {
            auto [member_name, member_type] = parse_name_type_pair(body_indent);
            members.push_back(make_statement(FieldDefinition(member_name, member_type)));
        }
    }

    return {std::move(name), std::move(traits), std::move(members)};
}

Statement_ptr Parser::parse_class_definition(int indent_level)
{
    auto [name, traits, members] = parse_membered_definition_base(indent_level);
    return make_statement(ClassDefinition(name, traits, members));
}

std::pair<std::string, TypeAnnotation_ptr> Parser::parse_name_type_pair(int member_indent)
{
    auto name_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
    std::string name = name_token.value;

    token_pipe.require_in_line(TokenType::COLON);

    if (token_pipe.consume_optional_in_line(TokenType::RECORD))
    {
        token_pipe.require_in_line(TokenType::EOL);
        auto parsed_block = parse_name_type_block(member_indent + 1);
        return {name, MAKE_RECURSIVE_TYPE(RecordTypeNode, std::move(parsed_block))};
    }

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
        auto [member_name, member_type] = parse_name_type_pair(expected_indent);
        members.push_back(make_statement(FieldDefinition(member_name, member_type)));
    }

    return members;
}

Statement_ptr Parser::parse_trait_definition(int indent_level)
{
    auto [name, traits, members] = parse_membered_definition_base(indent_level);
    return make_statement(TraitDefinition(name, traits, members));
}

// Templates

Statement_ptr Parser::parse_template_definition(int indent_level)
{
    // Consume the 'template' keyword
    token_pipe.advance_pointer();
    token_pipe.require_in_line(TokenType::EOL);

    std::vector<FieldDefinition> members;
    const int body_indent = indent_level + 1;

    while (true)
    {
        token_pipe.ignore_empty_lines();

        if (token_pipe.lookahead_indents() != body_indent)
        {
            break;
        }

        token_pipe.expect_n_indents(body_indent);

        auto name_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
        token_pipe.require_in_line(TokenType::COLON);

        auto param_type = parse_type();

        token_pipe.require_in_line(TokenType::EOL);

        members.emplace_back(name_token.value, param_type);
    }

    Statement_ptr target = parse_statement(indent_level);

    return make_statement(TemplateDefinition{std::move(members), std::move(target)});
}

} // namespace Wasp
