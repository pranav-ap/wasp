#include "ExpressionParselets.h"
#include "Doctor.h"
#include "Expression.h"
#include "Parser.h"
#include "Precedence.h"
#include "Token.h"
#include "TypeAnnotation.h"

#include <cmath>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>

#define MAKE_STATEMENT(x) std::make_shared<Statement>(Statement(x))
#define MAKE_EXPRESSION(x) std::make_shared<Expression>(Expression(x))

using std::map;

namespace Wasp
{
    Expression_ptr IdentifierParselet::parse(Parser &parser, const Token &token)
    {
        parser.token_pipe.advance_pointer();
        return MAKE_EXPRESSION(Identifier{token.value});
    }

    Expression_ptr LiteralParselet::parse(Parser &parser, const Token &token)
    {
        parser.token_pipe.advance_pointer();

        switch (token.type)
        {
        case TokenType::TRUE_KEYWORD:
            return MAKE_EXPRESSION(true);
        case TokenType::FALSE_KEYWORD:
            return MAKE_EXPRESSION(false);
        case TokenType::STRING_LITERAL:
            return MAKE_EXPRESSION(token.value);
        case TokenType::NUMBER_LITERAL:
        {
            double value = std::stod(token.value);
            // Check if it's an integer
            if (std::fmod(value, 1.0) == 0.0)
            {
                return MAKE_EXPRESSION(static_cast<int>(value));
            }
            return MAKE_EXPRESSION(value);
        }
        default: {
            Doctor::get().fatal(WaspStage::Parser, "Expected a literal value");
        }
        }
    }

    Expression_ptr PrefixOperatorParselet::parse(Parser &parser, const Token &token)
    {
        parser.token_pipe.advance_pointer();
        const auto right = parser.parse_expression();

        return MAKE_EXPRESSION((
            Prefix{token, right}));
    }

    Expression_ptr InfixOperatorParselet::parse(Parser &parser, const Expression_ptr left, const Token &token)
    {
        const auto right = parser.parse_expression(precedence - (is_right_associative ? 1 : 0));

        return MAKE_EXPRESSION((
            Infix{left, token, right}));
    }

    Expression_ptr SquareBracketParselet::parse(Parser &parser, const Token &token)
    {
        parser.token_pipe.advance_pointer();
        parser.token_pipe.ignore_spaces();

        if (parser.token_pipe.consume_optional(TokenType::CLOSE_SQUARE_BRACKET))
        {
            return MAKE_EXPRESSION(ListLiteral());
        }

        ExpressionVector expressions = parser.parse_expressions();
        parser.token_pipe.require(TokenType::CLOSE_SQUARE_BRACKET);
        return MAKE_EXPRESSION(ListLiteral(expressions));
    }

    Expression_ptr ParenthesisParselet::parse(Parser &parser, const Token &token)
    {
        parser.token_pipe.advance_pointer();
        parser.token_pipe.ignore_spaces();

        if (parser.token_pipe.consume_optional(TokenType::CLOSE_PARENTHESIS))
        {
            return MAKE_EXPRESSION(TupleLiteral());
        }

        ExpressionVector expressions = parser.parse_expressions();
        parser.token_pipe.require(TokenType::CLOSE_PARENTHESIS);
        return MAKE_EXPRESSION(TupleLiteral(expressions));
    }

    Expression_ptr CurlyBraceParselet::parse(Parser &parser, const Token &token)
    {
        parser.token_pipe.advance_pointer();
        parser.token_pipe.ignore_spaces();

        if (parser.token_pipe.consume_optional(TokenType::ARROW))
        {
            parser.token_pipe.require(TokenType::CLOSE_CURLY_BRACE);
            return MAKE_EXPRESSION(MapLiteral());
        }

        if (parser.token_pipe.consume_optional(TokenType::CLOSE_CURLY_BRACE))
        {
            return MAKE_EXPRESSION(SetLiteral());
        }

        parser.token_pipe.ignore_spaces();

        auto first_expr = parser.parse_expression();
        Doctor::get().fatal_if_nullptr(first_expr, WaspStage::Parser);

        parser.token_pipe.ignore_spaces();

        // Determine if it's a Map by checking for an ARROW after the first expr
        if (parser.token_pipe.consume_optional(TokenType::ARROW))
        {
            std::map<Expression_ptr, Expression_ptr> pairs;
            pairs[first_expr] = parser.parse_expression();
            parser.token_pipe.ignore_spaces();

            while (parser.token_pipe.consume_optional(TokenType::COMMA))
            {
                parser.token_pipe.ignore_spaces();

                auto key = parser.parse_expression();
                parser.token_pipe.require(TokenType::ARROW);
                pairs[key] = parser.parse_expression();
            }

            parser.token_pipe.ignore_spaces();
            parser.token_pipe.require(TokenType::CLOSE_CURLY_BRACE);
            return MAKE_EXPRESSION(MapLiteral(pairs));
        }

        // Otherwise, it's a Set
        ExpressionVector elements;
        elements.push_back(first_expr);

        parser.token_pipe.ignore_spaces();

        while (parser.token_pipe.consume_optional(TokenType::COMMA))
        {
            if (parser.token_pipe.current()->type == TokenType::CLOSE_CURLY_BRACE)
                break;

            elements.push_back(parser.parse_expression());
        }

        parser.token_pipe.ignore_spaces();
        parser.token_pipe.require(TokenType::CLOSE_CURLY_BRACE);

        return MAKE_EXPRESSION(SetLiteral(elements));
    }

    Expression_ptr TypePatternParselet::parse(Parser &parser, Expression_ptr left, const Token &token)
    {
        parser.token_pipe.advance_pointer();

        TypeAnnotation_ptr type = parser.parse_type();
        return MAKE_EXPRESSION(TypePattern(left, type));
    }

    Expression_ptr AssignmentParselet::parse(Parser &parser, Expression_ptr left, const Token &token)
    {
        parser.token_pipe.advance_pointer();

        Expression_ptr right = parser.parse_expression(static_cast<int>(Precedence::ASSIGNMENT) - 1);
        Doctor::get().fatal_if_nullptr(right, WaspStage::Parser);

        if (left->is<TypePattern>())
        {
            const auto &pattern = left->as<TypePattern>();

            return MAKE_EXPRESSION(TypedAssignment(
                pattern.expression,
                right,
                pattern.type_node));
        }

        return MAKE_EXPRESSION(UntypedAssignment(left, right));
    }

    Expression_ptr TernaryConditionParselet::parse(Parser &parser, const Token &token)
    {
        parser.token_pipe.advance_pointer();

        Expression_ptr condition = parser.parse_expression();
        parser.token_pipe.require(TokenType::THEN);
        auto expression = parser.parse_ternary_condition(TokenType::IF, condition);
        return expression;
    }

    Expression_ptr CallParselet::parse(Parser &parser, const Expression_ptr left, const Token &token)
    {
        bool is_valid_callee = left->is<Identifier>();

        if (left->is<Infix>() && left->as<Infix>().op.type == TokenType::DOT)
        {
            is_valid_callee = true;
        }

        Doctor::get().assert_true(
            is_valid_callee,
            WaspStage::Parser,
            "Call requires an identifier or member access expression"
        );

        ExpressionVector arguments;

        if (!parser.token_pipe.consume_optional_in_line(TokenType::CLOSE_PARENTHESIS))
        {
            arguments = parser.parse_expressions();
            parser.token_pipe.require(TokenType::CLOSE_PARENTHESIS);
        }

        return MAKE_EXPRESSION(Call(left, arguments));
    }

    Expression_ptr PrefixRangeParselet::parse(Parser &parser, const Token &token)
    {
        parser.token_pipe.advance_pointer();

        Expression_ptr end = nullptr;
        Expression_ptr step = nullptr;

        auto next = parser.token_pipe.current_in_line();

        if (next &&
            next->type != TokenType::EOL &&
            next->type != TokenType::STEP &&
            next->type != TokenType::CLOSE_PARENTHESIS &&
            next->type != TokenType::CLOSE_SQUARE_BRACKET &&
            next->type != TokenType::CLOSE_CURLY_BRACE &&
            next->type != TokenType::COMMA)
        {
            end = parser.parse_expression(static_cast<int>(Precedence::RANGE));
        }

        if (parser.token_pipe.consume_optional_in_line(TokenType::STEP))
        {
            step = parser.parse_expression(static_cast<int>(Precedence::RANGE));
        }

        return MAKE_EXPRESSION(RangeLiteral(nullptr, end, step, this->is_inclusive));
    }

    Expression_ptr InfixRangeParselet::parse(Parser &parser, Expression_ptr left, const Token &token)
    {
        Expression_ptr end = nullptr;
        Expression_ptr step = nullptr;

        auto next = parser.token_pipe.current_in_line();

        if (next &&
            next->type != TokenType::EOL &&
            next->type != TokenType::STEP &&
            next->type != TokenType::CLOSE_PARENTHESIS &&
            next->type != TokenType::CLOSE_SQUARE_BRACKET &&
            next->type != TokenType::CLOSE_CURLY_BRACE &&
            next->type != TokenType::COMMA)
        {
            end = parser.parse_expression(static_cast<int>(Precedence::RANGE));
        }

        if (parser.token_pipe.consume_optional_in_line(TokenType::STEP))
        {
            step = parser.parse_expression(static_cast<int>(Precedence::RANGE));
        }

        return MAKE_EXPRESSION(RangeLiteral(left, end, step, this->is_inclusive));
    }

    Expression_ptr PlaceholderDotParselet::parse(Parser &parser, const Token &token)
    {
        parser.token_pipe.advance_pointer();
        return MAKE_EXPRESSION(DotLiteral{});
    }

    // get_precedence

    int PrefixOperatorParselet::get_precedence() const
    {
        return precedence;
    }

    int InfixOperatorParselet::get_precedence() const
    {
        return precedence;
    }

    int TypePatternParselet::get_precedence() const
    {
        return static_cast<int>(Precedence::TYPE_PATTERN);
    }

    int AssignmentParselet::get_precedence() const
    {
        return static_cast<int>(Precedence::ASSIGNMENT);
    }

    int TernaryConditionParselet::get_precedence() const
    {
        return static_cast<int>(Precedence::TERNARY_CONDITION);
    }

    int CallParselet::get_precedence() const
    {
        return static_cast<int>(Precedence::CALL);
    }

    int InfixRangeParselet::get_precedence() const
    {
        return static_cast<int>(Precedence::RANGE);
    }

    int PrefixRangeParselet::get_precedence() const
    {
        return static_cast<int>(Precedence::RANGE);
    }
}
