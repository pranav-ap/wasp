#include "ExpressionParselets.h"
#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Parser.h"
#include "Precedence.h"
#include "Token.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <utility>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp
{

Expression_ptr IdentifierParselet::parse(Parser& parser, const Token& token)
{
    parser.token_pipe.advance_pointer();
    return make_expression(Identifier{token.lexeme});
}

Expression_ptr LiteralParselet::parse(Parser& parser, const Token& token)
{
    parser.token_pipe.advance_pointer();

    switch (token.type)
    {
    case TokenType::TRUE_KEYWORD:
        return make_expression(BooleanLiteral(true));
    case TokenType::FALSE_KEYWORD:
        return make_expression(BooleanLiteral(false));
    case TokenType::STRING_LITERAL:
        return make_expression(StringLiteral(token.lexeme));
    case TokenType::NUMBER_LITERAL: {
        double value = std::stod(token.lexeme);
        if (std::fmod(value, 1.0) == 0.0)
        {
            return make_expression(IntegerLiteral(static_cast<int>(value)));
        }
        return make_expression(FloatLiteral(value));
    }
    case TokenType::NONE: {
        return make_expression(NoneLiteral());
    }
    default:
        Doctor::get().fatal(WaspStage::Parser, "Expected a literal value");
    }
}

Expression_ptr PrefixOperatorParselet::parse(Parser& parser, const Token& token)
{
    parser.token_pipe.advance_pointer();
    const auto right = parser.parse_expression(precedence);
    return make_expression(Prefix{token, right});
}

Expression_ptr InfixOperatorParselet::parse(
    Parser& parser,
    const Expression_ptr left,
    const Token& token
)
{
    const auto right = parser.parse_expression(precedence - (is_right_associative ? 1 : 0));
    return make_expression(Infix{left, token, right});
}

Expression_ptr SquareBracketParselet::parse(Parser& parser, const Token&)
{
    parser.token_pipe.advance_pointer();
    parser.token_pipe.ignore_spaces();

    if (parser.token_pipe.consume_optional(TokenType::CLOSE_SQUARE_BRACKET))
    {
        return make_expression(ListLiteral());
    }

    ExpressionVector expressions = parser.parse_expressions();
    parser.token_pipe.require(TokenType::CLOSE_SQUARE_BRACKET);
    return make_expression(ListLiteral(expressions));
}

Expression_ptr ParenthesisParselet::parse(Parser& parser, const Token&)
{
    parser.token_pipe.advance_pointer();
    parser.token_pipe.ignore_spaces();

    if (parser.token_pipe.consume_optional(TokenType::CLOSE_PARENTHESIS))
    {
        return make_expression(TupleLiteral());
    }

    ExpressionVector expressions = parser.parse_expressions();
    parser.token_pipe.require(TokenType::CLOSE_PARENTHESIS);
    return make_expression(TupleLiteral(expressions));
}

Expression_ptr CurlyBraceParselet::parse(Parser& parser, const Token&)
{
    parser.token_pipe.advance_pointer();
    parser.token_pipe.ignore_spaces();

    if (parser.token_pipe.consume_optional(TokenType::ARROW))
    {
        parser.token_pipe.require(TokenType::CLOSE_CURLY_BRACE);
        return make_expression(MapLiteral());
    }

    if (parser.token_pipe.consume_optional(TokenType::CLOSE_CURLY_BRACE))
    {
        return make_expression(SetLiteral());
    }

    parser.token_pipe.ignore_spaces();

    auto first_expr = parser.parse_expression();
    Doctor::get().fatal_if_nullptr(first_expr, WaspStage::Parser);

    parser.token_pipe.ignore_spaces();

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
        return make_expression(MapLiteral(pairs));
    }

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

    return make_expression(SetLiteral(elements));
}

Expression_ptr AssignmentParselet::parse(
    Parser& parser,
    Expression_ptr left,
    const Token& token
)
{
    parser.token_pipe.advance_pointer();

    Expression_ptr right = parser.parse_expression(
        static_cast<int>(Precedence::ASSIGNMENT) - 1
    );

    Doctor::get().fatal_if_nullptr(right, WaspStage::Parser);

    if (token.type != TokenType::EQUAL)
    {
        Token op_token = token;

        switch (token.type)
        {
        case TokenType::PLUS_EQUAL:
            op_token.type = TokenType::PLUS;
            op_token.lexeme = "+";
            break;
        case TokenType::MINUS_EQUAL:
            op_token.type = TokenType::MINUS;
            op_token.lexeme = "-";
            break;
        case TokenType::STAR_EQUAL:
            op_token.type = TokenType::STAR;
            op_token.lexeme = "*";
            break;
        case TokenType::DIVISION_EQUAL:
            op_token.type = TokenType::DIVISION;
            op_token.lexeme = "/";
            break;
        case TokenType::MOD_EQUAL:
            op_token.type = TokenType::MOD;
            op_token.lexeme = "%";
            break;
        case TokenType::POWER_EQUAL:
            op_token.type = TokenType::POWER;
            op_token.lexeme = "**";
            break;
        default:
            break;
        }

        right = make_expression(Infix(left, op_token, right));
    }

    return make_expression(Assignment(left, right));
}

Expression_ptr TernaryConditionParselet::parse(Parser& parser, const Token&)
{
    parser.token_pipe.advance_pointer();

    Expression_ptr condition = parser.parse_expression();
    parser.token_pipe.require(TokenType::THEN);
    return parser.parse_ternary_condition(condition);
}

Expression_ptr MemberAccessParselet::parse(
    Parser& parser,
    Expression_ptr left,
    const Token&
)
{
    Expression_ptr right = parser.parse_expression(get_precedence());
    Doctor::get().fatal_if_nullptr(right, WaspStage::Parser);
    return make_expression(MemberAccess(left, right));
}

bool LesserThanParselet::looks_like_generic_args(Parser& parser) const
{
    int offset = 0;
    int depth = 1;

    while (true)
    {
        const Token* t = parser.token_pipe.peek(offset++);

        if (!t || t->type == TokenType::EOL || t->type == TokenType::END_OF_FILE)
        {
            return false;
        }

        if (t->type == TokenType::SPACE || t->type == TokenType::TAB)
        {
            continue;
        }

        if (t->type == TokenType::GREATER_THAN)
        {
            depth--;
            if (depth == 0)
            {
                return true;
            }
        }
        else if (t->type == TokenType::LESSER_THAN)
        {
            depth++;
        }
        else if (
            t->type == TokenType::IDENTIFIER || t->type == TokenType::COMMA ||
            t->type == TokenType::VERTICAL_BAR || t->type == TokenType::AMPERSAND ||
            t->type == TokenType::OPEN_SQUARE_BRACKET ||
            t->type == TokenType::CLOSE_SQUARE_BRACKET ||
            t->type == TokenType::OPEN_PARENTHESIS ||
            t->type == TokenType::CLOSE_PARENTHESIS ||
            t->type == TokenType::OPEN_CURLY_BRACE ||
            t->type == TokenType::CLOSE_CURLY_BRACE || t->type == TokenType::DOT ||
            t->type == TokenType::NONE || t->type == TokenType::NATIVE
        )
        {
            continue;
        }
        else
        {
            return false;
        }
    }
}

Expression_ptr LesserThanParselet::parse(Parser& parser, Expression_ptr left, const Token& token)
{
    if ((left->is<Identifier>() || left->is<MemberAccess>()) && looks_like_generic_args(parser))
    {
        TypeAnnotationVector generic_args;

        do
        {
            parser.token_pipe.ignore_spaces();
            generic_args.push_back(parser.parse_type());
            parser.token_pipe.ignore_spaces();
        }
        while (parser.token_pipe.consume_optional(TokenType::COMMA));

        parser.token_pipe.require(TokenType::GREATER_THAN);

        return make_expression(TemplateAngular(left, generic_args));
    }

    Expression_ptr right = parser.parse_expression(get_precedence());
    return make_expression(Infix(left, token, right));
}

static bool is_target_capitalized(const Expression_ptr& expr)
{
    if (expr->is<Identifier>())
    {
        const auto& name = expr->as<Identifier>().name;
        return !name.empty() && std::isupper(name.front());
    }

    if (expr->is<MemberAccess>())
    {
        return is_target_capitalized(expr->as<MemberAccess>().right);
    }

    if (expr->is<TemplateAngular>())
    {
        return is_target_capitalized(expr->as<TemplateAngular>().target);
    }

    return false;
}

Expression_ptr CallParselet::parse(
    Parser& parser,
    const Expression_ptr left,
    const Token&
)
{
    Doctor::get().assert(
        left->is<Identifier>() || left->is<MemberAccess>() || left->is<Call>() ||
            left->is<Constructor>() || left->is<TemplateAngular>(),
        WaspStage::Parser,
        "Expected identifier, member access, call, constructor, or generic "
        "instantiation on the "
        "left side of a function call"
    );

    ExpressionVector arguments;

    if (!parser.token_pipe.consume_optional_in_line(TokenType::CLOSE_PARENTHESIS))
    {
        arguments = parser.parse_expressions();
        parser.token_pipe.require(TokenType::CLOSE_PARENTHESIS);
    }

    if (is_target_capitalized(left))
    {
        return make_expression(Constructor(left, arguments));
    }

    return make_expression(Call(left, arguments));
}

Expression_ptr InterpolatedStringParselet::parse(
    Parser& parser,
    const Token& token
)
{
    Token end_token = token;
    InterpolatedString node;

    parser.token_pipe.advance_pointer();

    while (auto current = parser.token_pipe.current_in_line())
    {
        // Stop condition!
        if (current->type == TokenType::INTERPOLATION_END)
        {
            end_token = *current;
            parser.token_pipe.advance_pointer();
            break;
        }

        if (current->type == TokenType::STRING_LITERAL)
        {
            node.parts.push_back(make_expression(StringLiteral{current->lexeme}));
            parser.token_pipe.advance_pointer();
        }
        else if (current->type == TokenType::OPEN_CURLY_BRACE)
        {
            // Consume '{'
            parser.token_pipe.advance_pointer();

            // Parse whatever expression is inside the braces!
            node.parts.push_back(parser.parse_expression(0));

            auto close_brace = parser.token_pipe.current_in_line();
            Doctor::get().assert(
                close_brace &&
                    close_brace->type == TokenType::CLOSE_CURLY_BRACE,
                WaspStage::Parser,
                "Expected '}' at the end of an interpolation expression"
            );

            // Consume '}'
            parser.token_pipe.advance_pointer();
        }
        else
        {
            Doctor::get().fatal(
                WaspStage::Parser,
                "Unexpected token inside interpolated string : " +
                    current->lexeme
            );
        }
    }

    return make_expression(std::move(node));
}

// ============================================================================
// Precedence Getters
// ============================================================================

int MemberAccessParselet::get_precedence() const
{
    return static_cast<int>(Precedence::MEMBER_ACCESS);
}
int PrefixOperatorParselet::get_precedence() const
{
    return precedence;
}
int InfixOperatorParselet::get_precedence() const
{
    return precedence;
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
int LesserThanParselet::get_precedence() const
{
    return static_cast<int>(Precedence::COMPARISON);
}

} // namespace Wasp
