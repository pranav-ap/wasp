#pragma once

#include "SourceCodePointer.h"
#include "Token.h"
#include <queue>
#include <string>
#include <vector>

namespace Wasp
{

class Lexer
{
public:
    std::vector<Token> run(std::string source_code);

private:
    std::string source;
    SourceCodePointer source_code_pointer;
    std::queue<Token> pending_tokens;

    Token next_token();

	// CONSUMERS

	Token consume_identifier_or_keyword();
    Token consume_number_literal();
	Token consume_string_literal();
    Token consume_operators();

	Token consume_plus();
	Token consume_minus();
	Token consume_star();
	Token consume_division();
	Token consume_reminder();
	Token consume_power();
	Token consume_bang();
	Token consume_equal();
	Token consume_colon();
	Token consume_dot();
	Token consume_question();
	Token consume_greater_than();
	Token consume_lesser_than();
	Token consume_single_char_punctuation(char ch);

	Token consume_eol();
    Token consume_space();
    Token consume_tab();

    // UTILS

    bool expect_current_char(char ch);
    char get_char_at(int index) const;
    char get_current_char() const;
    char get_right_char() const;
    void next();
    void previous();
};

} // namespace Wasp
