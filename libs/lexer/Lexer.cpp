#include "Lexer.h"
#include <cwctype>

#define LINE_NUM token_position.get_line_num()
#define COL_NUM token_position.get_column_num()

namespace Wasp
{
    std::string convert_spaces_to_tabs(const std::string &input)
    {
        std::string result;

        bool inside_string = false;
        size_t i = 0;

        while (i < input.size())
        {
            if (input[i] == '\'')
            {
                inside_string = !inside_string;
                result.push_back(input[i]);
                i++;
                continue;
            }

            // If we are not in a string, look for 4 consecutive spaces
            if (!inside_string && i + 3 < input.size() &&
                input[i] == ' ' && input[i + 1] == ' ' &&
                input[i + 2] == ' ' && input[i + 3] == ' ')
            {
                result.push_back('\t');
                i += 4; // Skip the four spaces
            }
            else
            {
                // Otherwise, just copy the character as-is
                result.push_back(input[i]);
                i++;
            }
        }

        return result;
    }

    std::vector<Token> Lexer::run(const std::string source_code)
    {
        source = convert_spaces_to_tabs(source_code);
        std::vector<Token> tokens;

        while (true)
        {
            if (const char current_char = get_current_char(); current_char == '\0')
            {
                break;
            }

            Token token = next_token();
            tokens.push_back(token);
        }

        tokens.push_back(Token{TokenType::EOL, to_string(TokenType::EOL), LINE_NUM, COL_NUM});
        next();
        tokens.push_back(Token{TokenType::END_OF_FILE, to_string(TokenType::END_OF_FILE), LINE_NUM, COL_NUM});

        return tokens;
    }

    Token Lexer::next_token()
    {
        const char current_char = get_current_char();

        if (current_char == ' ')
        {
            return consume_space();
        }

        if (current_char == '\t')
        {
            return consume_tab();
        }

        if (current_char == '\n')
        {
            return consume_eol();
        }

        if (std::isalpha(current_char) || current_char == '_')
        {
            return consume_identifier_or_keyword();
        }

        if (std::isdigit(current_char))
        {
            return consume_number_literal();
        }

        if (current_char == '\'')
        {
            return consume_string_literal();
        }

        return consume_operators();
    }

    // CONSUMERS

    Token Lexer::consume_identifier_or_keyword()
    {
        char current_char = get_current_char();
        std::string value;

        while (std::isalnum(current_char) || current_char == '_')
        {
            value += current_char;
            next();
            current_char = get_current_char();
        }

        auto type = TokenType::IDENTIFIER;

        if (is_keyword(value))
        {
            type = get_keyword_token_type(value);
        }

        return Token{type, value, LINE_NUM, COL_NUM};
    }

    Token Lexer::consume_number_literal()
    {
        char current_char = get_current_char();
        std::string value;
        bool has_decimal = false;

        while (std::isdigit(current_char) || (current_char == '.' && !has_decimal) || current_char == '_')
        {
            if (current_char == '.')
            {
                // Check if next char is also a dot
                if (get_right_char() == '.')
                {
                    break;
                }
                has_decimal = true;
                value += current_char;
            }
            else if (current_char == '_')
            {
                // Skip appending underscores but advance position
                next();
                current_char = get_current_char();
                continue;
            }
            else
            {
                // append digit to value
                value += current_char;
            }

            next();
            current_char = get_current_char();
        }

        return Token{TokenType::NUMBER_LITERAL, value, LINE_NUM, COL_NUM};
    }

    Token Lexer::consume_string_literal()
    {
        std::string value;
        // Skip the opening quote
        next();

        char current_char = get_current_char();
        while (current_char != '\'' && current_char != '\0')
        {
            value += current_char;
            next();
            current_char = get_current_char();
        }

        // Skip the closing quote
        next();
        return Token{TokenType::STRING_LITERAL, value, LINE_NUM, COL_NUM};
    }

    Token Lexer::consume_operators()
    {
        switch (const char current_char = get_current_char())
        {
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

    Token Lexer::consume_plus()
    {
        next();

        if (expect_current_char('='))
        {
            return Token(TokenType::PLUS_EQUAL, "+=", LINE_NUM, COL_NUM);
        }

        if (expect_current_char('+'))
        {
            return Token(TokenType::PLUS_PLUS, "++", LINE_NUM, COL_NUM);
        }

        return Token(TokenType::PLUS, "+", LINE_NUM, COL_NUM);
    }

    Token Lexer::consume_minus()
    {
        next();
        if (expect_current_char('='))
        {
            return Token(TokenType::MINUS_EQUAL, "-=", LINE_NUM, COL_NUM);
        }

        if (expect_current_char('-'))
        {
            return Token(TokenType::MINUS_MINUS, "--", LINE_NUM, COL_NUM);
        }

        return Token(TokenType::MINUS, "-", LINE_NUM, COL_NUM);
    }

    Token Lexer::consume_star()
    {
        next();
        if (expect_current_char('='))
        {
            return Token(TokenType::STAR_EQUAL, "*=", LINE_NUM, COL_NUM);
        }
        return Token(TokenType::STAR, "*", LINE_NUM, COL_NUM);
    }

    Token Lexer::consume_division()
    {
        next();
        if (expect_current_char('='))
        {
            return Token(TokenType::DIVISION_EQUAL, "/=", LINE_NUM, COL_NUM);
        }
        return Token(TokenType::DIVISION, "/", LINE_NUM, COL_NUM);
    }

    Token Lexer::consume_reminder()
    {
        next();
        if (expect_current_char('='))
        {
            return Token(TokenType::REMINDER_EQUAL, "%=", LINE_NUM, COL_NUM);
        }
        return Token(TokenType::REMINDER, "%", LINE_NUM, COL_NUM);
    }

    Token Lexer::consume_power()
    {
        next();
        if (expect_current_char('='))
        {
            return Token(TokenType::POWER_EQUAL, "**=", LINE_NUM, COL_NUM);
        }
        return Token(TokenType::POWER, "**", LINE_NUM, COL_NUM);
    }

    Token Lexer::consume_bang()
    {
        next();
        if (expect_current_char('='))
        {
            return Token(TokenType::BANG_EQUAL, "!=", LINE_NUM, COL_NUM);
        }
        return Token(TokenType::BANG, "!", LINE_NUM, COL_NUM);
    }

    Token Lexer::consume_equal()
    {
        next();
        if (expect_current_char('='))
        {
            return Token(TokenType::EQUAL_EQUAL, "==", LINE_NUM, COL_NUM);
        }
        else if (expect_current_char('>'))
        {
            return Token(TokenType::ARROW, "=>", LINE_NUM, COL_NUM);
        }

        return Token(TokenType::EQUAL, "=", LINE_NUM, COL_NUM);
    }

    Token Lexer::consume_colon()
    {
        next();
        if (expect_current_char(':'))
        {
            return Token(TokenType::COLON_COLON, "::", LINE_NUM, COL_NUM);
        }
        return Token(TokenType::COLON, ":", LINE_NUM, COL_NUM);
    }

    Token Lexer::consume_dot()
    {
        next();
        if (expect_current_char('.'))
        {
            if (expect_current_char('='))
            {
                return Token(TokenType::DOT_DOT_EQUAL, "..=", LINE_NUM, COL_NUM);
            }

            if (expect_current_char('<'))
            {
                return Token(TokenType::DOT_DOT_LESS, "..<", LINE_NUM, COL_NUM);
            }

            return Token(TokenType::DOT_DOT, "..", LINE_NUM, COL_NUM);
        }

        return Token(TokenType::DOT, ".", LINE_NUM, COL_NUM);
    }

    Token Lexer::consume_question()
    {
        next();
        if (expect_current_char('.'))
        {
            return Token(TokenType::QUESTION_DOT, "?.", LINE_NUM, COL_NUM);
        }

        if (expect_current_char('='))
        {
            return Token(TokenType::QUESTION_EQUAL, "?=", LINE_NUM, COL_NUM);
        }

        return Token(TokenType::QUESTION, "?", LINE_NUM, COL_NUM);
    }

    Token Lexer::consume_greater_than()
    {
        next();
        if (expect_current_char('='))
        {
            return Token(TokenType::GREATER_THAN_EQUAL, ">=", LINE_NUM, COL_NUM);
        }
        if (expect_current_char('>'))
        {
            return Token(TokenType::GREATER_THAN_2, ">>", LINE_NUM, COL_NUM);
        }
        return Token(TokenType::GREATER_THAN, ">", LINE_NUM, COL_NUM);
    }

    Token Lexer::consume_lesser_than()
    {
        next();
        if (expect_current_char('='))
        {
            if (expect_current_char('>'))
            {
                return Token(TokenType::ROCKET, "<=>", LINE_NUM, COL_NUM);
            }
            if (expect_current_char('<'))
            {
                return Token(TokenType::LESSER_THAN_2, "<<", LINE_NUM, COL_NUM);
            }
            return Token(TokenType::LESSER_THAN_EQUAL, "<=", LINE_NUM, COL_NUM);
        }

        if (expect_current_char('>'))
        {
            return Token(TokenType::ZIP, "<>", LINE_NUM, COL_NUM);
        }
        return Token(TokenType::LESSER_THAN, "<", LINE_NUM, COL_NUM);
    }

    Token Lexer::consume_single_char_punctuation(char ch)
    {
        switch (ch)
        {
        case '(':
            next();
            return Token(TokenType::OPEN_PARENTHESIS, "(", LINE_NUM, COL_NUM);
        case ')':
            next();
            return Token(TokenType::CLOSE_PARENTHESIS, ")", LINE_NUM, COL_NUM);
        case '{':
            next();
            return Token(TokenType::OPEN_CURLY_BRACE, "{", LINE_NUM, COL_NUM);
        case '}':
            next();
            return Token(TokenType::CLOSE_CURLY_BRACE, "}", LINE_NUM, COL_NUM);
        case '[':
            next();
            return Token(TokenType::OPEN_SQUARE_BRACKET, "[", LINE_NUM, COL_NUM);
        case ']':
            next();
            return Token(TokenType::CLOSE_SQUARE_BRACKET, "]", LINE_NUM, COL_NUM);
        case '\\':
            next();
            return Token(TokenType::BACKWARD_SLASH, "\\", LINE_NUM, COL_NUM);
        case ',':
            next();
            return Token(TokenType::COMMA, ",", LINE_NUM, COL_NUM);
        case '|':
            next();
            return Token(TokenType::VERTICAL_BAR, "|", LINE_NUM, COL_NUM);
        case '_':
            next();
            return Token(TokenType::UNDERSCORE, "_", LINE_NUM, COL_NUM);
        case '$':
            next();
            return Token(TokenType::DOLLAR, "$", LINE_NUM, COL_NUM);
        case '&':
            next();
            return Token(TokenType::AMPERSAND, "&", LINE_NUM, COL_NUM);
        case '~':
            next();
            return Token(TokenType::TILDE, "~", LINE_NUM, COL_NUM);
        case '@':
            next();
            return Token(TokenType::AT_SIGN, "@", LINE_NUM, COL_NUM);
        case '#':
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

            return Token(TokenType::COMMENT, comment, LINE_NUM, COL_NUM);
        }
        default:
            return consume_unknown_token();
        }
    }

    Token Lexer::consume_eol()
    {
        next();
        return Token(TokenType::EOL, "\\n", LINE_NUM, COL_NUM);
    }

    Token Lexer::consume_space()
    {
        next();
        return Token(TokenType::SPACE, " ", LINE_NUM, COL_NUM);
    }

    Token Lexer::consume_tab()
    {
        next();
        return Token(TokenType::TAB, "\t", LINE_NUM, COL_NUM);
    }

    Token Lexer::consume_unknown_token()
    {
        const char current_char = get_current_char();
        next();
        return Token(TokenType::UNKNOWN, std::string(1, current_char), LINE_NUM, COL_NUM);
    }

    // UTILS

    bool Lexer::expect_current_char(char ch)
    {
        if (ch == get_current_char())
        {
            source_code_pointer.advance();
            return true;
        }

        return false;
    }

    char Lexer::get_char_at(int index) const
    {
        if (index >= static_cast<int>(source.size()) || index < 0)
            return '\0';

        const char ch = source.at(index);
        return ch;
    }

    char Lexer::get_current_char() const
    {
        const int index = source_code_pointer.get_index();
        return get_char_at(index);
    }

    char Lexer::get_right_char() const
    {
        const int index = source_code_pointer.get_index();
        return get_char_at(index + 1);
    }

    void Lexer::next()
    {
        source_code_pointer.advance();
        token_position.increment_column_number();
    }

    void Lexer::previous()
    {
        source_code_pointer.retreat();
        token_position.decrement_column_number();
    }
}
