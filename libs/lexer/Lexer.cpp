#include "Lexer.h"
#include "Token.h"

#include <cctype>
#include <cstddef>
#include <cwctype>
#include <queue>
#include <string>
#include <vector>

#define LINE_NUM token_position.get_line_num()
#define COL_NUM token_position.get_column_num()

namespace Wasp {
std::string convert_spaces_to_tabs(const std::string& input) {
    std::string result;

    bool inside_string = false;
    size_t i = 0;

    while (i < input.size()) {
        if (input[i] == '\'') {
            inside_string = !inside_string;
            result.push_back(input[i]);
            i++;
            continue;
        }

        // If we are not in a string, look for 4 consecutive spaces
        if (!inside_string && i + 3 < input.size() && input[i] == ' ' && input[i + 1] == ' ' &&
            input[i + 2] == ' ' && input[i + 3] == ' ') {
            result.push_back('\t');
            i += 4; // Skip the four spaces
        } else {
            // Otherwise, just copy the character as-is
            result.push_back(input[i]);
            i++;
        }
    }

    return result;
}

std::vector<Token> Lexer::run(const std::string source_code) {
    source = convert_spaces_to_tabs(source_code);
    std::vector<Token> tokens;

    while (true) {
        if (const char current_char = get_current_char(); current_char == '\0') {
            break;
        }

        Token token = next_token();
        tokens.push_back(token);
    }

    tokens.push_back(Token{TokenType::EOL, to_string(TokenType::EOL)});
    next();
    tokens.push_back(
        Token{TokenType::END_OF_FILE, to_string(TokenType::END_OF_FILE)}
    );

    return tokens;
}

Token Lexer::next_token() {
    if (!pending_tokens.empty())
    {
        Token t = pending_tokens.front();
        pending_tokens.pop();
        return t;
    }

    const char current_char = get_current_char();

    if (current_char == ' ') {
        return consume_space();
    }

    if (current_char == '\t') {
        return consume_tab();
    }

    if (current_char == '\n' || current_char == '\r') {
        return consume_eol();
    }

    if (std::isalpha(current_char) || current_char == '_') {
        return consume_identifier_or_keyword();
    }

    if (std::isdigit(current_char)) {
        return consume_number_literal();
    }

    if (current_char == '\"')
    {
        return consume_string_literal();
    }

    if (current_char == '#')
    {
        next();

        // Consume the rest of the line as a comment
        std::string comment;

        char current_char = get_current_char();
        while (current_char != '\n' && current_char != '\0')
        {
            comment += current_char;
            next();
            current_char = get_current_char();
        }

        return Token(TokenType::COMMENT, comment);
    }

    return consume_operators();
}

// CONSUMERS

Token Lexer::consume_identifier_or_keyword() {
    char current_char = get_current_char();
    std::string value;

    while (std::isalnum(current_char) || current_char == '_') {
        value += current_char;
        next();
        current_char = get_current_char();
    }

    auto type = TokenType::IDENTIFIER;

    if (is_keyword(value)) {
        type = get_keyword_token_type(value);
    }

    return Token{type, value};
}

Token Lexer::consume_number_literal() {
    char current_char = get_current_char();
    std::string value;
    bool has_decimal = false;

    while (std::isdigit(current_char) || (current_char == '.' && !has_decimal) ||
           current_char == '_') {
        if (current_char == '.') {
            // Check if next char is also a dot
            if (get_right_char() == '.') {
                break;
            }
            has_decimal = true;
            value += current_char;
        } else if (current_char == '_') {
            // Skip appending underscores but advance position
            next();
            current_char = get_current_char();
            continue;
        } else {
            // append digit to value
            value += current_char;
        }

        next();
        current_char = get_current_char();
    }

    return Token{TokenType::NUMBER_LITERAL, value};
}

Token Lexer::consume_string_literal()
{
    int start_line = token_position.get_line_num();
    int start_col = token_position.get_column_num();
    next(); // Skip opening quote

    std::string current_chars;
    std::vector<Token> local_pending;
    bool is_interpolated = false;

    auto flush_string = [&]()
    {
        if (!current_chars.empty())
        {
            local_pending.push_back(
                Token{TokenType::STRING_LITERAL, current_chars}
            );
            current_chars.clear();
        }
    };

    while (get_current_char() != '\"' && get_current_char() != '\0')
    {
        if (get_current_char() == '{')
        {
            is_interpolated = true;
            flush_string();

            // Push the '{'
            local_pending.push_back(Token{TokenType::OPEN_CURLY_BRACE, "{"});
            next();

            int brace_depth = 1;
            while (get_current_char() != '\0' && brace_depth > 0)
            {
                Token t = next_token(); // Lex the inner expression normally!
                if (t.type == TokenType::OPEN_CURLY_BRACE)
                {
                    brace_depth++;
                }
                if (t.type == TokenType::CLOSE_CURLY_BRACE)
                {
                    brace_depth--;
                }

                local_pending.push_back(t);
            }

            // Reset position trackers for the next string segment
            start_line = token_position.get_line_num();
            start_col = token_position.get_column_num();
            continue;
        }

        current_chars += get_current_char();
        next();
    }

    flush_string();

    if (get_current_char() == '\"')
    {
        next(); // Skip closing quote
    }

    // --- THE CLEAN RETURN ---
    // If it's just a normal string 'hello', return it directly!
    if (!is_interpolated && local_pending.size() == 1)
    {
        return local_pending[0];
    }
    // If it's an empty string ''
    if (!is_interpolated && local_pending.empty())
    {
        return Token{TokenType::STRING_LITERAL, ""};
    }

    // If it IS interpolated, wrap it in our boundary tokens and flush to the
    // main queue
    pending_tokens.push(Token{TokenType::INTERPOLATION_START, "\""});
    for (const auto& t : local_pending)
    {
        pending_tokens.push(t);
    }
    pending_tokens.push(Token{TokenType::INTERPOLATION_END, "\""});

    // Pop and return the first token from the queue
    Token first = pending_tokens.front();
    pending_tokens.pop();
    return first;
}

Token Lexer::consume_operators() {
    switch (const char current_char = get_current_char()) {
    case '+':
        return consume_plus();
    case '-':
        return consume_minus();
    case '*':
        return consume_star();
    case '/':
        return consume_division();
    case '%':
        return consume_reminder();
    case '^':
        return consume_power();
    case '!':
        return consume_bang();
    case '=':
        return consume_equal();
    case ':':
        return consume_colon();
    case '.':
        return consume_dot();
    case '?':
        return consume_question();
    case '>':
        return consume_greater_than();
    case '<':
        return consume_lesser_than();
    default:
        return consume_single_char_punctuation(current_char);
    }
}

Token Lexer::consume_plus() {
    next();

    if (expect_current_char('=')) {
        return Token(TokenType::PLUS_EQUAL, "+=");
    }

    if (expect_current_char('+')) {
        return Token(TokenType::PLUS_PLUS, "++");
    }

    return Token(TokenType::PLUS, "+");
}

Token Lexer::consume_minus() {
    next();
    if (expect_current_char('=')) {
        return Token(TokenType::MINUS_EQUAL, "-=");
    }

    if (expect_current_char('-')) {
        return Token(TokenType::MINUS_MINUS, "--");
    }

    return Token(TokenType::MINUS, "-");
}

Token Lexer::consume_star() {
    next();
    if (expect_current_char('=')) {
        return Token(TokenType::STAR_EQUAL, "*=");
    }
    return Token(TokenType::STAR, "*");
}

Token Lexer::consume_division() {
    next();
    if (expect_current_char('=')) {
        return Token(TokenType::DIVISION_EQUAL, "/=");
    }
    return Token(TokenType::DIVISION, "/");
}

Token Lexer::consume_reminder() {
    next();
    if (expect_current_char('=')) {
        return Token(TokenType::MOD_EQUAL, "%=");
    }
    return Token(TokenType::MOD, "%");
}

Token Lexer::consume_power() {
    next();
    if (expect_current_char('=')) {
        return Token(TokenType::POWER_EQUAL, "**=");
    }
    return Token(TokenType::POWER, "**");
}

Token Lexer::consume_bang() {
    next();
    if (expect_current_char('=')) {
        return Token(TokenType::BANG_EQUAL, "!=");
    }
    return Token(TokenType::BANG, "!");
}

Token Lexer::consume_equal() {
    next();
    if (expect_current_char('=')) {
        return Token(TokenType::EQUAL_EQUAL, "==");
    } else if (expect_current_char('>')) {
        return Token(TokenType::ARROW, "=>");
    }

    return Token(TokenType::EQUAL, "=");
}

Token Lexer::consume_colon() {
    next();
    if (expect_current_char(':')) {
        return Token(TokenType::COLON_COLON, "::");
    }
    return Token(TokenType::COLON, ":");
}

Token Lexer::consume_dot() {
    next();
    if (expect_current_char('.')) {
        if (expect_current_char('=')) {
            return Token(TokenType::DOT_DOT_EQUAL, "..=");
        }

        if (expect_current_char('<')) {
            return Token(TokenType::DOT_DOT_LESS, "..<");
        }

        return Token(TokenType::DOT_DOT, "..");
    }

    return Token(TokenType::DOT, ".");
}

Token Lexer::consume_question() {
    next();
    if (expect_current_char('.')) {
        return Token(TokenType::QUESTION_DOT, "?.");
    }

    if (expect_current_char('=')) {
        return Token(TokenType::QUESTION_EQUAL, "?=");
    }

    return Token(TokenType::QUESTION, "?");
}

Token Lexer::consume_greater_than() {
    next();
    if (expect_current_char('=')) {
        return Token(TokenType::GREATER_THAN_EQUAL, ">=");
    }
    if (expect_current_char('>')) {
        return Token(TokenType::GREATER_THAN_2, ">>");
    }
    return Token(TokenType::GREATER_THAN, ">");
}

Token Lexer::consume_lesser_than() {
    next();
    if (expect_current_char('=')) {
        if (expect_current_char('>')) {
            return Token(TokenType::ROCKET, "<=>");
        }
        if (expect_current_char('<')) {
            return Token(TokenType::LESSER_THAN_2, "<<");
        }
        return Token(TokenType::LESSER_THAN_EQUAL, "<=");
    }

    if (expect_current_char('>')) {
        return Token(TokenType::ZIP, "<>");
    }
    return Token(TokenType::LESSER_THAN, "<");
}

Token Lexer::consume_single_char_punctuation(char ch) {
    switch (ch) {
    case '(':
        next();
        return Token(TokenType::OPEN_PARENTHESIS, "(");
    case ')':
        next();
        return Token(TokenType::CLOSE_PARENTHESIS, ")");
    case '{':
        next();
        return Token(TokenType::OPEN_CURLY_BRACE, "{");
    case '}':
        next();
        return Token(TokenType::CLOSE_CURLY_BRACE, "}");
    case '[':
        next();
        return Token(TokenType::OPEN_SQUARE_BRACKET, "[");
    case ']':
        next();
        return Token(TokenType::CLOSE_SQUARE_BRACKET, "]");
    case '\\':
        next();
        return Token(TokenType::BACKWARD_SLASH, "\\");
    case ',':
        next();
        return Token(TokenType::COMMA, ",");
    case '|':
        next();
        return Token(TokenType::VERTICAL_BAR, "|");
    case '_':
        next();
        return Token(TokenType::UNDERSCORE, "_");
    case '$':
        next();
        return Token(TokenType::DOLLAR, "$");
    case '&':
        next();
        return Token(TokenType::AMPERSAND, "&");
    case '~':
        next();
        return Token(TokenType::TILDE, "~");
    case '@':
        next();
        return Token(TokenType::AT_SIGN, "@");
    default:
        return consume_unknown_token();
    }
}

Token Lexer::consume_eol() {
    next();
    return Token(TokenType::EOL, "\\n");
}

Token Lexer::consume_space() {
    next();
    return Token(TokenType::SPACE, " ");
}

Token Lexer::consume_tab() {
    next();
    return Token(TokenType::TAB, "\t");
}

Token Lexer::consume_unknown_token() {
    const char current_char = get_current_char();
    next();
    return Token(TokenType::UNKNOWN, std::string(1, current_char));
}

// UTILS

bool Lexer::expect_current_char(char ch) {
    if (ch == get_current_char()) {
        source_code_pointer.advance();
        return true;
    }

    return false;
}

char Lexer::get_char_at(int index) const {
    if (index >= static_cast<int>(source.size()) || index < 0)
        return '\0';

    const char ch = source.at(index);
    return ch;
}

char Lexer::get_current_char() const {
    const int index = source_code_pointer.get_index();
    return get_char_at(index);
}

char Lexer::get_right_char() const {
    const int index = source_code_pointer.get_index();
    return get_char_at(index + 1);
}

void Lexer::next() {
    source_code_pointer.advance();
    token_position.increment_column_number();
}

void Lexer::previous() {
    source_code_pointer.retreat();
    token_position.decrement_column_number();
}
} // namespace Wasp
