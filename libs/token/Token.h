#pragma once
#include <string>

#define TOKEN_LIST(X)                                                          \
    /* X(Enum Name, String Repr, Is Keyword?) */                               \
    X(UNKNOWN, "UNKNOWN", false)                                               \
    X(NUMBER_LITERAL, "NUMBER", false)                                         \
    X(STRING_LITERAL, "STRING", false)                                         \
    X(INTERPOLATION_START, "INTERPOLATION_START", false)                       \
    X(INTERPOLATION_END, "INTERPOLATION_END", false)                           \
    X(IDENTIFIER, "IDENTIFIER", false)                                         \
    X(OPEN_PARENTHESIS, "(", false)                                            \
    X(CLOSE_PARENTHESIS, ")", false)                                           \
    X(OPEN_CURLY_BRACE, "{", false)                                            \
    X(CLOSE_CURLY_BRACE, "}", false)                                           \
    X(OPEN_SQUARE_BRACKET, "[", false)                                         \
    X(CLOSE_SQUARE_BRACKET, "]", false)                                        \
    X(BACKWARD_SLASH, "\\", false)                                             \
    X(COMMA, ",", false)                                                       \
    X(COLON, ":", false)                                                       \
    X(COLON_COLON, "::", false)                                                \
    X(VERTICAL_BAR, "|", false)                                                \
    X(AMPERSAND, "&", false)                                                   \
    X(UNDERSCORE, "_", false)                                                  \
    X(DOLLAR, "$", false)                                                      \
    X(TILDE, "~", false)                                                       \
    X(AT_SIGN, "@", false)                                                     \
    X(DOT, ".", false)                                                         \
    X(DOT_DOT, "..", false)                                                    \
    X(DOT_DOT_LESS, "..<", false)                                              \
    X(DOT_DOT_EQUAL, "..=", false)                                             \
    X(QUESTION, "?", false)                                                    \
    X(QUESTION_DOT, "?.", false)                                               \
    X(QUESTION_EQUAL, "?=", false)                                             \
    X(PLUS, "+", false)                                                        \
    X(PLUS_PLUS, "++", false)                                                  \
    X(PLUS_EQUAL, "+=", false)                                                 \
    X(MINUS, "-", false)                                                       \
    X(MINUS_MINUS, "--", false)                                                \
    X(MINUS_EQUAL, "-=", false)                                                \
    X(ARROW, "=>", false)                                                      \
    X(STAR, "*", false)                                                        \
    X(STAR_EQUAL, "*=", false)                                                 \
    X(DIVISION, "/", false)                                                    \
    X(DIVISION_EQUAL, "/=", false)                                             \
    X(MOD, "%", false)                                                         \
    X(MOD_EQUAL, "%=", false)                                                  \
    X(POWER, "**", false)                                                      \
    X(POWER_EQUAL, "**=", false)                                               \
    X(EQUAL, "=", false)                                                       \
    X(EQUAL_EQUAL, "==", false)                                                \
    X(BANG, "!", false)                                                        \
    X(BANG_EQUAL, "!=", false)                                                 \
    X(LESSER_THAN, "<", false)                                                 \
    X(LESSER_THAN_2, "<<", false)                                              \
    X(LESSER_THAN_EQUAL, "<=", false)                                          \
    X(GREATER_THAN, ">", false)                                                \
    X(GREATER_THAN_2, ">>", false)                                             \
    X(GREATER_THAN_EQUAL, ">=", false)                                         \
    X(ROCKET, "<=>", false)                                                    \
    X(ZIP, "<>", false)                                                        \
    X(STEP, "step", true)                                                      \
    X(IMPORT, "import", true)                                                  \
    X(EXPOSE, "expose", true)                                                  \
    X(EXCEPT, "except", true)                                                  \
    X(IF, "if", true)                                                          \
    X(ELIF, "elif", true)                                                      \
    X(ELSE, "else", true)                                                      \
    X(THEN, "then", true)                                                      \
    X(REQUIRED, "required", true)                                              \
    X(NONE, "none", true)                                                      \
    X(NOT, "not", true)                                                        \
    X(AND, "and", true)                                                        \
    X(OR, "or", true)                                                          \
    X(XOR, "xor", true)                                                        \
    X(LET, "let", true)                                                        \
    X(CONST_KEYWORD, "const", true)                                            \
    X(WHILE, "while", true)                                                    \
    X(UNTIL, "until", true)                                                    \
    X(UNLESS, "unless", true)                                                  \
    X(FOR, "for", true)                                                        \
    X(DO, "do", true)                                                          \
    X(BREAK, "break", true)                                                    \
    X(CONTINUE, "continue", true)                                              \
    X(REDO, "redo", true)                                                      \
    X(IN_KEYWORD, "in", true)                                                  \
    X(ASSERT, "assert", true)                                                  \
    X(IS, "is", true)                                                          \
    X(AS, "as", true)                                                          \
    X(FUN, "fun", true)                                                        \
    X(RETURN_KEYWORD, "return", true)                                          \
    X(YIELD_KEYWORD, "yield", true)                                            \
    X(CLASS, "class", true)                                                    \
    X(TRAIT, "trait", true)                                                    \
    X(TEMPLATE, "template", true)                                              \
    X(PURE, "pure", true)                                                      \
    X(RECORD, "record", true)                                                  \
    X(MY, "my", true)                                                          \
    X(OUR, "our", true)                                                        \
    X(TOP, "top", true)                                                        \
    X(PKG, "pkg", true)                                                        \
    X(UP, "up", true)                                                          \
    X(TYPE, "type", true)                                                      \
    X(EXPORT, "export", true)                                                  \
    X(INFIX, "infix", true)                                                    \
    X(POSTFIX, "postfix", true)                                                \
    X(PREFIX, "prefix", true)                                                  \
    X(NATIVE, "native", true)                                                  \
    X(ENUM, "enum", true)                                                      \
    X(TRUE_KEYWORD, "true", true)                                              \
    X(FALSE_KEYWORD, "false", true)                                            \
    X(SPACE, "SPACE", false)                                                   \
    X(TAB, "TAB", false)                                                       \
    X(EOL, "EOL", false)                                                       \
    X(END_OF_FILE, "END_OF_FILE", false)                                       \
    X(COMMENT, "#", false)                                                     \
    X(SINGLE_QUOTE, "'", false)                                                \
    X(CHARACTER, "CHARACTER", false)

namespace Wasp {

enum class TokenType {
#define AS_ENUM(name, str, is_kw) name,
  TOKEN_LIST(AS_ENUM)
#undef AS_ENUM
};

constexpr std::string to_string(const TokenType t) {
  switch (t) {
#define AS_CASE(name, str, is_kw)                                              \
  case TokenType::name:                                                        \
    return str;
    TOKEN_LIST(AS_CASE)
#undef AS_CASE
  default:
    return "UNKNOWN";
  }
}

constexpr bool is_keyword(const TokenType t) {
  switch (t) {
#define AS_KW_CASE(name, str, is_kw)                                           \
  case TokenType::name:                                                        \
    return is_kw;
    TOKEN_LIST(AS_KW_CASE)
#undef AS_KW_CASE
  default:
    return false;
  }
}

constexpr bool is_keyword(const std::string &s) {
#define AS_KW_CHECK(name, str, is_kw)                                          \
  if constexpr (is_kw) {                                                       \
    if (s == str)                                                              \
      return true;                                                             \
  }

  TOKEN_LIST(AS_KW_CHECK)

#undef AS_KW_CHECK
  return false;
}

constexpr TokenType get_keyword_token_type(const std::string &s) {
#define AS_KW_TYPE_CHECK(name, str, is_kw)                                     \
  if constexpr (is_kw) {                                                       \
    if (s == str)                                                              \
      return TokenType::name;                                                  \
  }

  TOKEN_LIST(AS_KW_TYPE_CHECK)

#undef AS_KW_TYPE_CHECK
  return TokenType::IDENTIFIER;
}

struct Token {
  TokenType type;
  std::string lexeme;
};

} // namespace Wasp
