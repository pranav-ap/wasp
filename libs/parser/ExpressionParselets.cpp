#include "ExpressionParselets.h"
#include "AST.h"
#include "Doctor.h"
#include "Expression.h"
#include "Parser.h"
#include "Precedence.h"
#include "Token.h"

#include <cmath>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <variant>

template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wasp {
Expression_ptr IdentifierParselet::parse(Parser& parser, const Token& token) {
    parser.token_pipe.advance_pointer();
    return make_expression(Identifier{token.value});
}

Expression_ptr LiteralParselet::parse(Parser& parser, const Token& token) {
    parser.token_pipe.advance_pointer();

    switch (token.type) {
    case TokenType::TRUE_KEYWORD:
        return make_expression(true);
    case TokenType::FALSE_KEYWORD:
        return make_expression(false);
    case TokenType::STRING_LITERAL:
        return make_expression(token.value);
    case TokenType::NUMBER_LITERAL: {
        double value = std::stod(token.value);
        // Check if it's an integer
        if (std::fmod(value, 1.0) == 0.0) {
            return make_expression(static_cast<int>(value));
        }
        return make_expression(value);
    }
    default: {
        Doctor::get().fatal(WaspStage::Parser, "Expected a literal value");
    }
    }
}

Expression_ptr PrefixOperatorParselet::parse(Parser& parser, const Token& token) {
    parser.token_pipe.advance_pointer();
    const auto right = parser.parse_expression();

    return make_expression((Prefix{token, right}));
}

Expression_ptr InfixOperatorParselet::parse(
    Parser& parser, const Expression_ptr left, const Token& token
) {
    const auto right = parser.parse_expression(precedence - (is_right_associative ? 1 : 0));

    return make_expression((Infix{left, token, right}));
}

Expression_ptr SquareBracketParselet::parse(Parser& parser, const Token& token) {
    parser.token_pipe.advance_pointer();
    parser.token_pipe.ignore_spaces();

    if (parser.token_pipe.consume_optional(TokenType::CLOSE_SQUARE_BRACKET)) {
        return make_expression(ListLiteral());
    }

    ExpressionVector expressions = parser.parse_expressions();
    parser.token_pipe.require(TokenType::CLOSE_SQUARE_BRACKET);
    return make_expression(ListLiteral(expressions));
}

Expression_ptr ParenthesisParselet::parse(Parser& parser, const Token& token) {
    parser.token_pipe.advance_pointer();
    parser.token_pipe.ignore_spaces();

    if (parser.token_pipe.consume_optional(TokenType::CLOSE_PARENTHESIS)) {
        return make_expression(TupleLiteral());
    }

    ExpressionVector expressions = parser.parse_expressions();
    parser.token_pipe.require(TokenType::CLOSE_PARENTHESIS);
    return make_expression(TupleLiteral(expressions));
}

Expression_ptr CurlyBraceParselet::parse(Parser& parser, const Token& token) {
    parser.token_pipe.advance_pointer();
    parser.token_pipe.ignore_spaces();

    if (parser.token_pipe.consume_optional(TokenType::ARROW)) {
        parser.token_pipe.require(TokenType::CLOSE_CURLY_BRACE);
        return make_expression(MapLiteral());
    }

    if (parser.token_pipe.consume_optional(TokenType::CLOSE_CURLY_BRACE)) {
        return make_expression(SetLiteral());
    }

    parser.token_pipe.ignore_spaces();

    auto first_expr = parser.parse_expression();
    Doctor::get().fatal_if_nullptr(first_expr, WaspStage::Parser);

    parser.token_pipe.ignore_spaces();

    // Determine if it's a Map by checking for an ARROW after the first expr
    if (parser.token_pipe.consume_optional(TokenType::ARROW)) {
        std::map<Expression_ptr, Expression_ptr> pairs;
        pairs[first_expr] = parser.parse_expression();
        parser.token_pipe.ignore_spaces();

        while (parser.token_pipe.consume_optional(TokenType::COMMA)) {
            parser.token_pipe.ignore_spaces();

            auto key = parser.parse_expression();
            parser.token_pipe.require(TokenType::ARROW);
            pairs[key] = parser.parse_expression();
        }

        parser.token_pipe.ignore_spaces();
        parser.token_pipe.require(TokenType::CLOSE_CURLY_BRACE);
        return make_expression(MapLiteral(pairs));
    }

    // Otherwise, it's a Set
    ExpressionVector elements;
    elements.push_back(first_expr);

    parser.token_pipe.ignore_spaces();

    while (parser.token_pipe.consume_optional(TokenType::COMMA)) {
        if (parser.token_pipe.current()->type == TokenType::CLOSE_CURLY_BRACE)
            break;

        elements.push_back(parser.parse_expression());
    }

    parser.token_pipe.ignore_spaces();
    parser.token_pipe.require(TokenType::CLOSE_CURLY_BRACE);

    return make_expression(SetLiteral(elements));
}

Expression_ptr TypePatternParselet::parse(Parser& parser, Expression_ptr left, const Token& token) {
    parser.token_pipe.advance_pointer();

    TypeAnnotation_ptr type = parser.parse_type();
    return make_expression(TypePattern(left, type));
}

Expression_ptr AssignmentParselet::parse(Parser& parser, Expression_ptr left, const Token& token) {
    parser.token_pipe.advance_pointer();

    Expression_ptr right = parser.parse_expression(static_cast<int>(Precedence::ASSIGNMENT) - 1);
    Doctor::get().fatal_if_nullptr(right, WaspStage::Parser);

    if (left->is<TypePattern>()) {
        const auto& pattern = left->as<TypePattern>();

        return make_expression(TypedAssignment(pattern.expression, right, pattern.type_node));
    }

    return make_expression(UntypedAssignment(left, right));
}

Expression_ptr TernaryConditionParselet::parse(Parser& parser, const Token& token) {
    parser.token_pipe.advance_pointer();

    Expression_ptr condition = parser.parse_expression();
    parser.token_pipe.require(TokenType::THEN);
    auto expression = parser.parse_ternary_condition(TokenType::IF, condition);
    return expression;
}

Expression_ptr PrefixRangeParselet::parse(Parser& parser, const Token& token) {
    parser.token_pipe.advance_pointer();

    Expression_ptr end = nullptr;
    Expression_ptr step = nullptr;

    auto next = parser.token_pipe.current_in_line();

    if (next && next->type != TokenType::EOL && next->type != TokenType::STEP &&
        next->type != TokenType::CLOSE_PARENTHESIS &&
        next->type != TokenType::CLOSE_SQUARE_BRACKET &&
        next->type != TokenType::CLOSE_CURLY_BRACE && next->type != TokenType::COMMA) {
        end = parser.parse_expression(static_cast<int>(Precedence::RANGE));
    }

    if (parser.token_pipe.consume_optional_in_line(TokenType::STEP)) {
        step = parser.parse_expression(static_cast<int>(Precedence::RANGE));
    }

    return make_expression(RangeLiteral(nullptr, end, step, this->is_inclusive));
}

Expression_ptr InfixRangeParselet::parse(Parser& parser, Expression_ptr left, const Token& token) {
    Expression_ptr end = nullptr;
    Expression_ptr step = nullptr;

    auto next = parser.token_pipe.current_in_line();

    if (next && next->type != TokenType::EOL && next->type != TokenType::STEP &&
        next->type != TokenType::CLOSE_PARENTHESIS &&
        next->type != TokenType::CLOSE_SQUARE_BRACKET &&
        next->type != TokenType::CLOSE_CURLY_BRACE && next->type != TokenType::COMMA) {
        end = parser.parse_expression(static_cast<int>(Precedence::RANGE));
    }

    if (parser.token_pipe.consume_optional_in_line(TokenType::STEP)) {
        step = parser.parse_expression(static_cast<int>(Precedence::RANGE));
    }

    return make_expression(RangeLiteral(left, end, step, this->is_inclusive));
}

Expression_ptr PlaceholderDotParselet::parse(Parser& parser, const Token& token) {
    parser.token_pipe.advance_pointer();
    return make_expression(DotLiteral{});
}

Expression_ptr CallParselet::parse(Parser& parser, const Expression_ptr left, const Token& token) {
    Doctor::get().assert(
        left->is<Identifier>() || left->is<MemberAccess>() || left->is<Call>(),
        WaspStage::Parser,
        "Callee requires an identifier or member access expression");

    ExpressionVector arguments;

    if (!parser.token_pipe.consume_optional_in_line(TokenType::CLOSE_PARENTHESIS)) {
        arguments = parser.parse_expressions();
        parser.token_pipe.require(TokenType::CLOSE_PARENTHESIS);
    }

    return std::visit(
        overloaded{
            [&](Identifier& id) { return make_expression(SimpleCall(id, arguments)); },
            [&](MemberAccess& ma) { return make_expression(ComplexCall(left, arguments)); },

            [](auto&) -> Expression_ptr
            {
                Doctor::get().fatal(
                    WaspStage::Parser,
                    "Expected an identifier or member access as the callable.");
            }},
        left->data);
}

Expression_ptr MemberAccessParselet::parse(
    Parser& parser, Expression_ptr left, const Token& token
) {
    Expression_ptr right = parser.parse_expression(get_precedence());
    Doctor::get().fatal_if_nullptr(right, WaspStage::Parser);

    return make_expression(MemberAccess(left, right));
}

// get_precedence

int MemberAccessParselet::get_precedence() const {
    return static_cast<int>(Precedence::MEMBER_ACCESS);
}

int PrefixOperatorParselet::get_precedence() const { return precedence; }

int InfixOperatorParselet::get_precedence() const { return precedence; }

int TypePatternParselet::get_precedence() const {
    return static_cast<int>(Precedence::TYPE_PATTERN);
}

int AssignmentParselet::get_precedence() const { return static_cast<int>(Precedence::ASSIGNMENT); }

int TernaryConditionParselet::get_precedence() const {
    return static_cast<int>(Precedence::TERNARY_CONDITION);
}

int CallParselet::get_precedence() const { return static_cast<int>(Precedence::CALL); }

int InfixRangeParselet::get_precedence() const { return static_cast<int>(Precedence::RANGE); }

int PrefixRangeParselet::get_precedence() const { return static_cast<int>(Precedence::RANGE); }
} // namespace Wasp
