#include "AST.h"
#include "Doctor.h"
#include "Parser.h"
#include "Token.h"
#include "TypeAnnotation.h"

#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using std::vector;

namespace Wasp
{

TypeAnnotation_ptr Parser::parse_type()
{
    vector<TypeAnnotation_ptr> variant_types;

    while (true)
    {
        TypeAnnotation_ptr type;

        if (token_pipe.consume_optional_in_line(TokenType::OPEN_SQUARE_BRACKET))
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
            bool is_native = token_pipe
                                 .consume_optional_in_line(TokenType::NATIVE)
                                 .has_value();

            type = consume_datatype_word();

            if (is_native)
            {
                Doctor::get().assert(
                    type->is<TypeIdentifierNode>(),
                    WaspStage::Parser,
                    "Only identifier types can be marked as 'native'."
                );

                type = make_type_annotation(NativeTypeNode{type});
            }
            else if (
                token_pipe.consume_optional_in_line(TokenType::LESSER_THAN)
            )
            {
                vector<TypeAnnotation_ptr> generic_args;

                do
                {
                    generic_args.push_back(parse_type());
                }
                while (token_pipe.consume_optional_in_line(TokenType::COMMA));

                token_pipe.require(TokenType::GREATER_THAN);

                type = make_type_annotation(
                    std::make_shared<TemplateAngularTypeNode>(
                        type,
                        std::move(generic_args)
                    )
                );
            }
        }

        variant_types.push_back(type);

        if (token_pipe.consume_optional_in_line(TokenType::VERTICAL_BAR))
        {
            continue;
        }

        break;
    }

    if (variant_types.size() > 1)
    {
        return make_type_annotation(
            std::make_shared<VariantTypeNode>(std::move(variant_types))
        );
    }

    return std::move(variant_types.front());
}

TypeAnnotationVector Parser::parse_types()
{
    TypeAnnotationVector types;

    do
    {
        auto type = parse_type();
        types.push_back(type);
    }
    while (token_pipe.consume_optional_in_line(TokenType::COMMA));

    return types;
}

TypeAnnotation_ptr Parser::consume_datatype_word()
{
    auto token = token_pipe.current_in_line();
    Doctor::get().fatal_if_nullopt(token, WaspStage::Parser);

    switch (token->type)
    {
    case TokenType::NUMBER_LITERAL: {
        token_pipe.advance_pointer();
        auto value = std::stod(token->value);
        if (std::fmod(value, 1.0) == 0.0)
        {
            return make_type_annotation(
                IntLiteralTypeNode{static_cast<int>(value)}
            );
        }
        return make_type_annotation(FloatLiteralTypeNode{value});
    }
    case TokenType::STRING_LITERAL: {
        token_pipe.advance_pointer();
        return make_type_annotation(StringLiteralTypeNode{token->value});
    }
    case TokenType::TRUE_KEYWORD: {
        token_pipe.advance_pointer();
        return make_type_annotation(BoolLiteralTypeNode{true});
    }
    case TokenType::FALSE_KEYWORD: {
        token_pipe.advance_pointer();
        return make_type_annotation(BoolLiteralTypeNode{false});
    }
    case TokenType::IDENTIFIER: {
        token_pipe.advance_pointer();
        return make_type_annotation(TypeIdentifierNode{token->value});
    }
    case TokenType::NONE: {
        token_pipe.advance_pointer();
        return make_type_annotation(NoneTypeNode{});
    }
    default: {
        Doctor::get().fatal(
            WaspStage::Parser,
            "Unexpected token in datatype: " + to_string(token->type),
            token->line,
            token->column
        );
    }
    }
}

TypeAnnotation_ptr Parser::parse_list_type()
{
    auto type = parse_type();
    token_pipe.require_later(TokenType::CLOSE_SQUARE_BRACKET);
    return make_type_annotation(std::make_shared<ListTypeNode>(type));
}

TypeAnnotation_ptr Parser::parse_tuple_or_fun_type()
{
    auto types = parse_types();
    token_pipe.require_later(TokenType::CLOSE_PARENTHESIS);

    if (token_pipe.consume_optional_in_line(TokenType::ARROW))
    {
        auto return_type = parse_type();
        return make_type_annotation(
            std::make_shared<FunctionTypeNode>(types, return_type)
        );
    }

    return make_type_annotation(std::make_shared<TupleTypeNode>(types));
}

TypeAnnotation_ptr Parser::parse_set_or_map_type()
{
    auto key_type = parse_type();

    if (token_pipe.consume_optional_later(TokenType::ARROW))
    {
        auto value_type = parse_type();
        token_pipe.require_later(TokenType::CLOSE_CURLY_BRACE);
        return make_type_annotation(
            std::make_shared<MapTypeNode>(key_type, value_type)
        );
    }

    token_pipe.require_later(TokenType::CLOSE_CURLY_BRACE);
    return make_type_annotation(std::make_shared<SetTypeNode>(key_type));
}

} // namespace Wasp
