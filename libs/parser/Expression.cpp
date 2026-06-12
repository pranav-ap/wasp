#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Parser.h"
#include "Token.h"

#include <map>
#include <memory>
#include <optional>
#include <utility>

namespace Wasp
{

Expression_ptr Parser::parse_expression()
{
    return parse_expression(0);
}

Expression_ptr Parser::parse_expression(const int precedence)
{
    auto token = token_pipe.current_in_line();

    if (!token)
    {
        return nullptr;
    }

    auto token_type = token->type;

    if (token_type == TokenType::LET)
    {
        return parse_variable_definition(true);
    }

    if (token_type == TokenType::CONST_KEYWORD)
    {
        return parse_variable_definition(false);
    }

    auto prefix_it = prefix_parselets.find(token_type);

    Doctor::get().assert(
        prefix_it != prefix_parselets.end(),
        WaspStage::Parser,
        "Expected the start of an expression but found : " + token->lexeme
    );

    Expression_ptr left = prefix_it->second->parse(*this, *token);

    Doctor::get().fatal_if_nullptr(left, WaspStage::Parser);

    while (precedence < get_next_operator_precedence())
    {
        token = token_pipe.current_in_line();
        Doctor::get().fatal_if_nullopt(token, WaspStage::Parser);

        token_pipe.advance_pointer();

        auto infix_it = infix_parselets.find(token->type);

        Doctor::get().assert(
            infix_it != infix_parselets.end(),
            WaspStage::Parser,
            "No matching infix parselet found for token : " + token->lexeme
        );

        left = infix_it->second->parse(*this, left, *token);
    }

    return left;
}

Expression_ptr Parser::parse_variable_definition(bool is_mutable)
{
    token_pipe.advance_pointer();

    auto id_token = token_pipe.current_in_line();
    Doctor::get().assert(
        id_token && id_token->type == TokenType::IDENTIFIER,
        WaspStage::Parser,
        "Expected an identifier after variable definition keyword."
    );

    Expression_ptr lhs = make_expression(Identifier(id_token->lexeme));

    token_pipe.advance_pointer();

    TypeAnnotation_ptr declared_type = nullptr;

    if (auto colon_token = token_pipe.current_in_line();
        colon_token && colon_token->type == TokenType::COLON)
    {
        token_pipe.advance_pointer();
        declared_type = parse_type();
    }

    auto equal_token = token_pipe.current_in_line();
    Doctor::get().assert(
        equal_token && equal_token->type == TokenType::EQUAL,
        WaspStage::Parser,
        "Variable definition must be initialized with '='."
    );

    token_pipe.advance_pointer();
    Expression_ptr rhs = parse_expression(0);

    return make_expression(Binding{lhs, rhs, declared_type, is_mutable});
}

ExpressionVector Parser::parse_expressions()
{
    ExpressionVector elements;

    while (auto element = parse_expression())
    {
        elements.push_back(std::move(element));

        if (const auto token = token_pipe.later();
            token.has_value() && token->type == TokenType::COMMA)
        {
            token_pipe.advance_pointer();
            continue;
        }

        break;
    }

    return elements;
}

int Parser::get_next_operator_precedence()
{
    if (const auto token = token_pipe.current_in_line())
    {
        auto it = infix_parselets.find(token->type);
        if (it != infix_parselets.end())
        {
            return it->second->get_precedence();
        }
    }

    return 0;
}

} // namespace Wasp
