#pragma once
#include <string>
#include <memory>


#define TOKEN_LIST(X) \
	/* X(Enum Name, String Repr, Is Keyword?) */ \
	X(UNKNOWN,        "UNKNOWN",  false) \
	X(NUMBER_LITERAL, "NUMBER",   false) \
	X(STRING_LITERAL, "STRING",   false) \
	X(IDENTIFIER,     "IDENTIFIER",       false) \
	X(OPEN_PARENTHESIS, "(",      false) \
	X(CLOSE_PARENTHESIS, ")",     false) \
	X(OPEN_CURLY_BRACE, "{",      false) \
	X(CLOSE_CURLY_BRACE, "}",     false) \
	X(OPEN_SQUARE_BRACKET, "[",   false) \
	X(CLOSE_SQUARE_BRACKET, "]",  false) \
	X(OPEN_ANGLE_BRACKET, "<",    false) \
	X(CLOSE_ANGLE_BRACKET, ">",   false) \
	X(BACKWARD_SLASH, "\\",       false) \
	X(COMMA,          ",",        false) \
	X(COLON,          ":",        false) \
	X(COLON_COLON,    "::",       false) \
	X(VERTICAL_BAR,   "|",        false) \
	X(UNDERSCORE,     "_",        false) \
	X(DOLLAR,         "$",        false) \
	X(TILDE,          "~",        false) \
	X(AT_SIGN,        "@",        false) \
	X(DOT,            ".",        false) \
	X(DOT_DOT,        "..",       false) \
	X(DOT_DOT_DOT,    "...",      false) \
	X(QUESTION,       "?",        false) \
	X(QUESTION_DOT,   "?.",       false) \
	X(QUESTION_EQUAL, "?=",       false) \
	X(PLUS,           "+",        false) \
	X(PLUS_EQUAL,     "+=",       false) \
	X(MINUS,          "-",        false) \
	X(MINUS_EQUAL,    "-=",       false) \
	X(ARROW,          "->",       false) \
	X(STAR,           "*",        false) \
	X(STAR_EQUAL,     "*=",       false) \
	X(DIVISION,       "/",        false) \
	X(DIVISION_EQUAL, "/=",       false) \
	X(REMINDER,       "%",        false) \
	X(REMINDER_EQUAL, "%=",       false) \
	X(POWER,          "**",       false) \
	X(POWER_EQUAL,    "**=",      false) \
	X(EQUAL,          "=",        false) \
	X(EQUAL_EQUAL,    "==",       false) \
	X(BANG,           "!",        false) \
	X(BANG_EQUAL,     "!=",       false) \
	X(LESSER_THAN,    "<",        false) \
	X(LESSER_THAN_EQUAL, "<=",    false) \
	X(GREATER_THAN,   ">",        false) \
	X(GREATER_THAN_EQUAL, ">=",   false) \
	X(IMPORT,         "import",   true) \
	X(FROM,           "from",     true) \
	X(IF,             "if",       true) \
	X(ELIF,           "elif",     true) \
	X(ELSE,           "else",     true) \
	X(THEN,           "then",     true) \
	X(PASS,           "pass",     true) \
	X(NO,             "no",       true) \
	X(NONE,           "none",     true) \
	X(SOME,           "some",     true) \
	X(NOT,            "not",      true) \
	X(AND,            "and",      true) \
	X(OR,             "or",       true) \
	X(XOR,            "xor",      true) \
	X(LET,            "let",      true) \
	X(CONST_KEYWORD,  "const",    true) \
	X(WHILE,          "while",    true) \
	X(UNTIL,          "until",    true) \
	X(UNLESS,         "unless",   true) \
	X(FOR,            "for",      true) \
	X(DO,             "do",       true) \
	X(BREAK,          "break",    true) \
	X(CONTINUE,       "continue", true) \
	X(REDO,           "redo",     true) \
	X(RETRY,           "retry",     true) \
	X(IN_KEYWORD,     "in",       true) \
	X(WITH,           "with",     true) \
	X(WHEN,           "when",     true) \
	X(WHERE,          "where",    true) \
	X(ASSERT,         "assert",   true) \
	X(DELETE_KEYWORD, "delete",   true) \
	X(TYPE_OF,        "typeof",   true) \
	X(IS,             "is",       true) \
	X(AS,             "as",       true) \
	X(FUN,            "fun",      true) \
	X(RETURN_KEYWORD, "return",   true) \
	X(YIELD_KEYWORD,  "yield",    true) \
	X(CLASS,          "class",    true) \
	X(INTERFACE,      "interface", true) \
	X(SHARE,      	  "share",     true) \
	X(ME,      	  "me",     true) \
	X(US,      	  "us",     true) \
	X(TYPE,           "type",     true) \
	X(ALIAS,           "alias",     true) \
	X(EXPORT,         "export",   true) \
	X(OPERATOR,       "operator", true) \
	X(INT,            "int",      true) \
	X(FLOAT,          "float",    true) \
	X(STR,            "str",      true) \
	X(BOOL,           "bool",     true) \
	X(ANY,            "any",     true) \
	X(ENUM,           "enum",     true) \
	X(RECORD,         "record",   true) \
	X(TRUE_KEYWORD,   "true",     true) \
	X(FALSE_KEYWORD,  "false",    true) \
	X(TRY,            "try",      true) \
	X(CATCH,          "catch",    true) \
	X(ENSURE,         "ensure",   true) \
	X(SPACE,          "SPACE",    false) \
	X(TAB,            "TAB",      false) \
	X(EOL,            "EOL",      false) \
	X(END_OF_FILE,            "END_OF_FILE",      false) \
	X(COMMENT,        "#",        false)

namespace Wasp {

enum class TokenType {
	#define AS_ENUM(name, str, is_kw) name,
    	TOKEN_LIST(AS_ENUM)
	#undef AS_ENUM
};


constexpr std::string to_string(const TokenType t) {
    switch (t) {
		#define AS_CASE(name, str, is_kw) case TokenType::name: return str;
        	TOKEN_LIST(AS_CASE)
		#undef AS_CASE  
		default: return "UNKNOWN";      
    }
}


constexpr bool is_keyword(const TokenType t) {
    switch (t) {
        #define AS_KW_CASE(name, str, is_kw) case TokenType::name: return is_kw;
            TOKEN_LIST(AS_KW_CASE)
        #undef AS_KW_CASE
        default: return false;
    }
}


constexpr bool is_keyword(const std::string &s) {
    #define AS_KW_CHECK(name, str, is_kw) \
        if constexpr (is_kw) { if (s == str) return true; }
    
    TOKEN_LIST(AS_KW_CHECK)
    
    #undef AS_KW_CHECK
    return false;
}

constexpr TokenType get_keyword_token_type(const std::string &s) {
    #define AS_KW_TYPE_CHECK(name, str, is_kw) \
        if constexpr (is_kw) { if (s == str) return TokenType::name; }
    
    TOKEN_LIST(AS_KW_TYPE_CHECK)
    
    #undef AS_KW_TYPE_CHECK
    return TokenType::IDENTIFIER; // Return IDENTIFIER if not a keyword
}

struct Token {
    TokenType type;
    std::string value;

    int line;
    int column;
};

} 
