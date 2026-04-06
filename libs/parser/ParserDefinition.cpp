#include "AST.h"
#include "Doctor.h"
#include "Parser.h"
#include "Statement.h"
#include "Token.h"
#include "TypeAnnotation.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
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
using std::optional;

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

    return make_statement(AliasDefinition(name, ref_type));
}

// Enum

Statement_ptr Parser::parse_enum_definition(int indent_level)
{
    token_pipe.advance_pointer();

    Token identifier = token_pipe.require_in_line(TokenType::IDENTIFIER);
    token_pipe.require_in_line(TokenType::EOL);

    std::vector<std::string> members = parse_enum_members(identifier.value, indent_level + 1);
    return make_statement(EnumDefinition(identifier.value, members));
}

std::vector<std::string> Parser::parse_enum_members(std::string stem, int indent_level)
{
    std::vector<std::string> members;

    while (true)
    {
        token_pipe.ignore_empty_lines();

        // Check indentation at the start of each line
        if (token_pipe.lookahead_indents() != indent_level)
        {
            // End of this enum block
            break;
        }

        token_pipe.expect_n_indents(indent_level);

        // Nested Enum
        if (token_pipe.consume_optional(TokenType::ENUM))
        {
            auto nested_name = token_pipe.require_in_line(TokenType::IDENTIFIER).value;
            token_pipe.require_in_line(TokenType::EOL);

            // Recurse with extended stem and deeper indentation
            auto nested_members = parse_enum_members(stem + "." + nested_name, indent_level + 1);
            members.insert(members.end(), nested_members.begin(), nested_members.end());
            continue;
        }

        // Leaf Member
        if (auto token = token_pipe.consume_optional(TokenType::IDENTIFIER))
        {
            members.push_back(stem + "." + token->value);
            token_pipe.require_in_line(TokenType::EOL);
            continue;
        }

        // No more members found
        break;
    }

    return members;
}

// Function

Statement_ptr Parser::parse_function_definition(int indent_level)
{
    token_pipe.advance_pointer();

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
    return make_statement(FunctionDefinition(name, parameters, return_type, body));
}

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

// Class

std::pair<std::map<std::string, TypeAnnotation_ptr>, std::vector<std::string>> Parser::
    parse_name_type_block(int expected_indent)
{
    std::map<std::string, TypeAnnotation_ptr> members;
    std::vector<std::string> members_declaration_order;

    while (true)
    {
        token_pipe.ignore_empty_lines();

        if (token_pipe.lookahead_indents() != expected_indent)
        {
            break;
        }

        token_pipe.expect_n_indents(expected_indent);

        auto [member_name, member_type] = parse_name_type_pair(expected_indent);
        members[member_name] = member_type;

        members_declaration_order.push_back(member_name);
    }

    return make_pair(members, members_declaration_order);
}

std::pair<std::string, TypeAnnotation_ptr> Parser::parse_name_type_pair(int member_indent)
{
    auto name_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
    std::string name = name_token.value;

    if (token_pipe.consume_optional_in_line(TokenType::RECORD))
    {
        token_pipe.require_in_line(TokenType::EOL);
        const int record_indent = member_indent + 1;
        std::map<std::string, TypeAnnotation_ptr> record_members;

        while (true)
        {
            token_pipe.ignore_empty_lines();

            if (token_pipe.lookahead_indents() != record_indent)
            {
                break;
            }

            token_pipe.expect_n_indents(record_indent);
            auto [member_name, member_type] = parse_name_type_pair(record_indent);
            record_members[member_name] = member_type;
        }

        return make_pair(name, MAKE_RECURSIVE_TYPE(RecordTypeNode, record_members));
    }

    token_pipe.require_in_line(TokenType::COLON);
    auto type = parse_type();

    return make_pair(name, type);
}

Statement_ptr Parser::parse_class_definition(int indent_level)
{
    token_pipe.advance_pointer(); // Consume 'class' keyword

    auto name_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
    auto class_name = name_token.value;

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

    auto [members, members_declaration_order] = parse_name_type_block(indent_level + 1);

    return make_statement(ClassDefinition(class_name, members, members_declaration_order, traits));
}

Statement_ptr Parser::parse_trait_definition(int indent_level)
{
    token_pipe.advance_pointer(); // Consume 'trait' keyword

    auto name_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
    auto trait_name = name_token.value;

    token_pipe.require_in_line(TokenType::EOL);

    auto [members, members_declaration_order] = parse_name_type_block(indent_level + 1);

    return make_statement(TraitDefinition(trait_name, members, members_declaration_order));
}

Statement_ptr Parser::parse_impl_definition(int indent_level)
{
    // Consume 'impl' keyword
    token_pipe.advance_pointer();

    // Parse the class name
    auto class_token = token_pipe.require_in_line(TokenType::IDENTIFIER);
    std::string class_name = class_token.value;

    token_pipe.require_in_line(TokenType::EOL);

    // Parse the indented block of methods
    std::vector<Statement_ptr> methods;
    const int method_indent = indent_level + 1;

    while (true)
    {
        token_pipe.ignore_empty_lines();

        // Break if we drop out of the impl block's indentation level
        if (token_pipe.lookahead_indents() != method_indent)
        {
            break;
        }

        token_pipe.expect_n_indents(method_indent);

        if (token_pipe.consume_optional(TokenType::FUN))
        {
            auto func = parse_function_definition(method_indent);
            methods.push_back(func);
        }
        else
        {
            Doctor::get().fatal(
                WaspStage::Parser,
                "Expected function definition ('fun') inside impl block."
            );
        }
    }

    return make_statement(ImplDefinition(class_name, methods));
}
} // namespace Wasp
