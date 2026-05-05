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
    return make_expression(Identifier{token.value});
}

Expression_ptr LiteralParselet::parse(Parser& parser, const Token& token)
{
    parser.token_pipe.advance_pointer();

    switch (token.type)
    {
    case TokenType::TRUE_KEYWORD:
        return make_expression(true);
    case TokenType::FALSE_KEYWORD:
        return make_expression(false);
    case TokenType::STRING_LITERAL:
        return make_expression(token.value);
    case TokenType::NUMBER_LITERAL: {
        double value = std::stod(token.value);
        if (std::fmod(value, 1.0) == 0.0)
        {
            return make_expression(static_cast<int>(value));
        }
        return make_expression(value);
    }
    case TokenType::NONE: {
        return make_expression(NoneLiteral{});
    }
    default:
        Doctor::get().fatal(WaspStage::Parser, "Expected a literal value");
    }

    return nullptr;
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

Expression_ptr SquareBracketParselet::parse(Parser& parser, const Token& token)
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

Expression_ptr ParenthesisParselet::parse(Parser& parser, const Token& token)
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

Expression_ptr CurlyBraceParselet::parse(Parser& parser, const Token& token)
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

Expression_ptr TypePatternParselet::parse(Parser& parser, Expression_ptr left, const Token& token)
{
    parser.token_pipe.advance_pointer();
    TypeAnnotation_ptr type = parser.parse_type();
    return make_expression(TypePattern(left, type));
}

Expression_ptr AssignmentParselet::parse(Parser& parser, Expression_ptr left, const Token& token)
{
    parser.token_pipe.advance_pointer();

    Expression_ptr right = parser.parse_expression(static_cast<int>(Precedence::ASSIGNMENT) - 1);
    Doctor::get().fatal_if_nullptr(right, WaspStage::Parser);

    if (token.type != TokenType::EQUAL)
    {
        Token op_token = token;

        switch (token.type)
        {
        case TokenType::PLUS_EQUAL:
            op_token.type = TokenType::PLUS;
            op_token.value = "+";
            break;
        case TokenType::MINUS_EQUAL:
            op_token.type = TokenType::MINUS;
            op_token.value = "-";
            break;
        case TokenType::STAR_EQUAL:
            op_token.type = TokenType::STAR;
            op_token.value = "*";
            break;
        case TokenType::DIVISION_EQUAL:
            op_token.type = TokenType::DIVISION;
            op_token.value = "/";
            break;
        case TokenType::MOD_EQUAL:
            op_token.type = TokenType::MOD;
            op_token.value = "%";
            break;
        case TokenType::POWER_EQUAL:
            op_token.type = TokenType::POWER;
            op_token.value = "**";
            break;
        default:
            break;
        }

        // Transform: right = (left op right)
        right = make_expression(Infix(left, op_token, right));
    }

    if (left->is<TypePattern>())
    {
        const auto& pattern = left->as<TypePattern>();
        return make_expression(TypedAssignment(pattern.expression, right, pattern.type_node));
    }

    return make_expression(UntypedAssignment(left, right));
}

Expression_ptr TernaryConditionParselet::parse(Parser& parser, const Token& token)
{
    parser.token_pipe.advance_pointer();

    Expression_ptr condition = parser.parse_expression();
    parser.token_pipe.require(TokenType::THEN);
    return parser.parse_ternary_condition(TokenType::IF, condition);
}

Expression_ptr PrefixRangeParselet::parse(Parser& parser, const Token& token)
{
    parser.token_pipe.advance_pointer();

    Expression_ptr end = nullptr;
    Expression_ptr step = nullptr;

    auto next = parser.token_pipe.current_in_line();

    if (next && next->type != TokenType::EOL && next->type != TokenType::STEP &&
        next->type != TokenType::CLOSE_PARENTHESIS &&
        next->type != TokenType::CLOSE_SQUARE_BRACKET &&
        next->type != TokenType::CLOSE_CURLY_BRACE && next->type != TokenType::COMMA)
    {
        end = parser.parse_expression(static_cast<int>(Precedence::RANGE));
    }

    if (parser.token_pipe.consume_optional_in_line(TokenType::STEP))
    {
        step = parser.parse_expression(static_cast<int>(Precedence::RANGE));
    }

    return make_expression(RangeLiteral(nullptr, end, step, this->is_inclusive));
}

Expression_ptr InfixRangeParselet::parse(Parser& parser, Expression_ptr left, const Token& token)
{
    Expression_ptr end = nullptr;
    Expression_ptr step = nullptr;

    auto next = parser.token_pipe.current_in_line();

    if (next && next->type != TokenType::EOL && next->type != TokenType::STEP &&
        next->type != TokenType::CLOSE_PARENTHESIS &&
        next->type != TokenType::CLOSE_SQUARE_BRACKET &&
        next->type != TokenType::CLOSE_CURLY_BRACE && next->type != TokenType::COMMA)
    {
        end = parser.parse_expression(static_cast<int>(Precedence::RANGE));
    }

    if (parser.token_pipe.consume_optional_in_line(TokenType::STEP))
    {
        step = parser.parse_expression(static_cast<int>(Precedence::RANGE));
    }

    return make_expression(RangeLiteral(left, end, step, this->is_inclusive));
}

Expression_ptr PlaceholderDotParselet::parse(Parser& parser, const Token& token)
{
    parser.token_pipe.advance_pointer();
    return make_expression(DotLiteral{});
}

Expression_ptr MemberAccessParselet::parse(Parser& parser, Expression_ptr left, const Token& token)
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

        // If we hit the end of the line or file before closing '>', it's not a generic.
        if (!t || t->type == TokenType::EOL || t->type == TokenType::END_OF_FILE)
            return false;

        // Skip whitespace
        if (t->type == TokenType::SPACE || t->type == TokenType::TAB)
            continue;

        if (t->type == TokenType::GREATER_THAN)
        {
            depth--;
            if (depth == 0)
                return true;
        }
        else if (t->type == TokenType::LESSER_THAN)
        {
            depth++;
        }
        else if (
            t->type == TokenType::IDENTIFIER || t->type == TokenType::COMMA ||
            t->type == TokenType::VERTICAL_BAR || t->type == TokenType::AMPERSAND ||
            t->type == TokenType::OPEN_SQUARE_BRACKET ||
            t->type == TokenType::CLOSE_SQUARE_BRACKET || t->type == TokenType::OPEN_PARENTHESIS ||
            t->type == TokenType::CLOSE_PARENTHESIS || t->type == TokenType::OPEN_CURLY_BRACE ||
            t->type == TokenType::CLOSE_CURLY_BRACE || t->type == TokenType::DOT ||
            // Base types
            t->type == TokenType::INT || t->type == TokenType::FLOAT || t->type == TokenType::STR ||
            t->type == TokenType::BOOL || t->type == TokenType::ANY || t->type == TokenType::NONE
        )
        {
            continue;
        }
        else
        {
            // We hit a token that doesn't belong in a type signature (e.g., +, -, =, literals)
            // It must be a comparison operation instead.
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

        return make_expression(ConcreteTemplate(left, generic_args));
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

    if (expr->is<ConcreteTemplate>())
    {
        return is_target_capitalized(expr->as<ConcreteTemplate>().target);
    }

    return false;
}

Expression_ptr CallParselet::parse(Parser& parser, const Expression_ptr left, const Token& token)
{
    Doctor::get().assert(
        left->is<Identifier>() || left->is<MemberAccess>() || left->is<Call>() ||
            left->is<Constructor>() || left->is<ConcreteTemplate>(),
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
int LesserThanParselet::get_precedence() const
{
    return static_cast<int>(Precedence::COMPARISON);
}

} // namespace Wasp
