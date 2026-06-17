#include "TypeNode.h"
#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Parser.h"
#include "Token.h"

#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

using std::vector;

namespace Wasp
{

TypeNodeVector Parser::parse_types()
{
    TypeNodeVector types;

    do
    {
        auto type = parse_type();
        types.push_back(type);
    }
    while (token_pipe.consume_optional_in_line(TokenType::COMMA));

    return types;
}

TypeNode_ptr Parser::parse_type()
{
    return parse_variant_type();
}

TypeNode_ptr Parser::parse_variant_type()
{
    std::vector<TypeNode_ptr> types;
    types.push_back(parse_intersection_type());

    while (token_pipe.consume_optional_in_line(TokenType::VERTICAL_BAR))
    {
        types.push_back(parse_intersection_type());
    }

    if (types.size() > 1)
    {
        return make_type_node(VariantTypeNode(std::move(types)));
    }

    return types.front();
}

TypeNode_ptr Parser::parse_intersection_type()
{
    std::vector<TypeNode_ptr> types;
    types.push_back(parse_base_type());

    while (token_pipe.consume_optional_in_line(TokenType::AMPERSAND))
    {
        types.push_back(parse_base_type());
    }

    if (types.size() > 1)
    {
        return make_type_node(IntersectionTypeNode(std::move(types)));
    }

    return types.front();
}

TypeNode_ptr Parser::parse_base_type()
{
    TypeNode_ptr type;

    if (token_pipe.consume_optional_in_line(
            TokenType::OPEN_SQUARE_BRACKET
        ))
    {
        type = parse_list_type();
    }
    else if (
        token_pipe.consume_optional_in_line(TokenType::OPEN_CURLY_BRACE)
    )
    {
        type = parse_set_or_map_type();
    }
    else if (
        token_pipe.consume_optional_in_line(TokenType::OPEN_PARENTHESIS)
    )
    {
        type = parse_tuple_or_fun_type();
    }
    else
    {
        type = consume_datatype_word();

        if (token_pipe.consume_optional_in_line(TokenType::LESSER_THAN))
        {
            Doctor::parser().assert(
                type->is<TypeIdentifierNode>(),
                "Only type identifiers can be used as template types"
            );

            std::string name = type->as<TypeIdentifierNode>().name;

            std::vector<TypeNode_ptr> generic_args;

            do
            {
                generic_args.push_back(parse_type());
            }
            while (token_pipe.consume_optional_in_line(TokenType::COMMA));

            token_pipe.require(TokenType::GREATER_THAN);

            type = make_type_node(
                AngularTypeNode(name, std::move(generic_args))
            );
        }
    }

    return type;
}

TypeNode_ptr Parser::consume_datatype_word()
{
    auto token = token_pipe.current_in_line();
    Doctor::parser().fatal_if_nullopt(token);

    switch (token->type)
    {
    case TokenType::NUMBER_LITERAL: {
        token_pipe.advance_pointer();
        auto value = std::stod(token->lexeme);
        Expression_ptr literal_expr;

        if (std::fmod(value, 1.0) == 0.0)
        {
            literal_expr = make_expression(
                IntegerLiteral{static_cast<int>(value)}
            );
        }
        else
        {
            literal_expr = make_expression(FloatLiteral{value});
        }

        return make_type_node(LiteralTypeNode{std::move(literal_expr)});
    }
    case TokenType::STRING_LITERAL: {
        token_pipe.advance_pointer();
        auto literal_expr = make_expression(StringLiteral{token->lexeme});
        return make_type_node(LiteralTypeNode{std::move(literal_expr)});
    }
    case TokenType::TRUE_KEYWORD: {
        token_pipe.advance_pointer();
        auto literal_expr = make_expression(BooleanLiteral{true});
        return make_type_node(LiteralTypeNode{std::move(literal_expr)});
    }
    case TokenType::FALSE_KEYWORD: {
        token_pipe.advance_pointer();
        auto literal_expr = make_expression(BooleanLiteral{false});
        return make_type_node(LiteralTypeNode{std::move(literal_expr)});
    }
    case TokenType::IDENTIFIER: {
        token_pipe.advance_pointer();
        return make_type_node(TypeIdentifierNode(token->lexeme));
    }
    case TokenType::NONE: {
        token_pipe.advance_pointer();
        return make_type_node(NoneTypeNode{});
    }
    default: {
        Doctor::parser().fatal(
            "Unexpected token in datatype: " + to_string(token->type)
        );
    }
    }
}

TypeNode_ptr Parser::parse_list_type()
{
    auto type = parse_type();
    token_pipe.require_later(TokenType::CLOSE_SQUARE_BRACKET);
    return make_type_node(ListTypeNode(type));
}

TypeNode_ptr Parser::parse_tuple_or_fun_type()
{
    auto types = parse_types();
    token_pipe.require_later(TokenType::CLOSE_PARENTHESIS);

    if (token_pipe.consume_optional_in_line(TokenType::ARROW))
    {
        auto return_type = parse_type();
        return make_type_node(FunctionTypeNode(types, return_type));
    }

    return make_type_node(TupleTypeNode(types));
}

TypeNode_ptr Parser::parse_set_or_map_type()
{
    auto key_type = parse_type();

    if (token_pipe.consume_optional_later(TokenType::ARROW))
    {
        auto value_type = parse_type();
        token_pipe.require_later(TokenType::CLOSE_CURLY_BRACE);
        return make_type_node(MapTypeNode(key_type, value_type));
    }

    token_pipe.require_later(TokenType::CLOSE_CURLY_BRACE);
    return make_type_node(SetTypeNode(key_type));
}

} // namespace Wasp
