#pragma once

#include "yant_types.h"
#include "yant_strings.h"

// integer:age = 12;
// Keyword_Int, Colon, Literal, Recieve, Value, Semicolon

// | 0: ENUM_LABEL
// | 1: Stringfied Label
// | 2: Identifiers Matchers
#define TOKENS_TABLE(extends) \
    extends(TOKEN_KEYWORD_INT, "Keyword::Integer") \
    extends(TOKEN_KEYWORD_FLOAT, "Keyword::Float") \
    extends(TOKEN_KEYWORD_STRING, "Keyword::String") \
    extends(TOKEN_KEYWORD_SET, "Keyword::Set") \
    extends(TOKEN_KEYWORD_FN, "Keyword::Function") \
    extends(TOKEN_KEYWORD_IF, "Keyword::If-Statement") \
    \
    extends(TOKEN_COLON, "Syntax::Colon") \
    extends(TOKEN_RIGHT_PARENTHESES, "Syntax::RightParentheses") \
    extends(TOKEN_LEFT_PARENTHESES, "Syntax::LeftParentheses") \
    extends(TOKEN_SEMICOLON, "Syntax::Semicolon") \
    extends(TOKEN_DOT, "Syntax::Dot")\
    extends(TOKEN_COMMA, "Syntax::Comma") \
    extends(TOKEN_LEFT_BRACE, "Syntax::LeftBrace") \
    extends(TOKEN_RIGHT_BRACE, "Syntax::RightBrace") \
    extends(TOKEN_LEFT_BRACKET, "Syntax::LeftBracket") \
    extends(TOKEN_RIGHT_BRACKET, "Syntax::RightBracket") \
    extends(TOKEN_PLUS, "Op::Plus")\
    extends(TOKEN_MINUS, "Op::Minus") \
    extends(TOKEN_STAR,  "Op::Star") \
    extends(TOKEN_SLASH, "Op::Slash") \
    extends(TOKEN_ASSIGN, "Op::Assign") \
    extends(TOKEN_EQEQ,   "Op::Equal") \
    extends(TOKEN_NOTEQ,  "Op::NotEqual") \
    extends(TOKEN_NOT,    "Op::Negate") \
    extends(TOKEN_OR,     "Op::Or") \
    extends(TOKEN_LTE,    "Op::LessOrEqual") \
    extends(TOKEN_LT,     "Op::Less") \
    extends(TOKEN_GTE,    "Op::GreaterOrEqual") \
    extends(TOKEN_GT,     "Op::Greater") \
    \
    extends(TOKEN_LITERAL_STRING, "Literal::String") \
    extends(TOKEN_LITERAL_INTEGER, "Literal::Integer") \
    extends(TOKEN_LITERAL_FLOAT, "Literal::Float")\
    \
    extends(TOKEN_IDENTIFIER, "Ref::Identifier") \
    extends(TOKEN_COMMENT,    "Special::Comment")\
    extends(TOKEN_EOF, "Special::Eof")

#define X_AS_ENUM(label, str) label,
typedef enum {
    TOKENS_TABLE(X_AS_ENUM)
    TOKEN_COUNT
} TokenType;
#undef X_AS_ENUM

#define X_AS_STRING(label, str) case label: return str;
static inline const char* token_type_str(TokenType t) {
    switch (t) {
        TOKENS_TABLE(X_AS_STRING)
        default: return "Unkown";
    }
}
#undef X_AS_STRING

typedef struct {
    TokenType type;
    StringSlice lexeme;
    usize line;
    usize column;

    union {
        i64 integer_value;
        f64 float_value;
    } literal;
} Token;

static inline Token make_token(TokenType type, StringSlice lexeme, usize line, usize column) {
    return (Token){
        .type   = type,
        .lexeme = lexeme,
        .line   = line,
        .column = column
    };
}
