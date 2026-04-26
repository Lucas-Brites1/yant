#include "../include/yant_file.h"
#include "../include/yant_types.h"
#include "../include/yant_token.h"
#include "../include/yant_vector.h"
#include "../include/lexer_helpers.h"
#include "../include/yant_strings.h"
#include "../include/logc.h"
#include <stdlib.h>
#include <string.h>

static inline void skip_irrelevant(Source* s) {
    while (!at_end(s)) {
        char c = peek(s);

        if (is_space(c)) {
            advance(s);
            continue;
        }

        if (c == '/' && peek_at(s, 1) == '/') {
            advance(s);
            advance(s);
            while (!at_end(s) && peek(s) != '\n') {
                advance(s);
            }
            continue;
        }

        if (c == '/' && peek_at(s, 1) == '*') {
            advance(s);
            advance(s);
            while (!at_end(s)) {
                if (peek(s) == '*' && peek_at(s, 1) == '/') {
                    advance(s);
                    advance(s);
                    break;
                }
                advance(s);
            }
            continue;
        }

        break;
    }
}

static inline void skip_whitespaces(Source* s) {
    while(is_space(peek(s))) {
        advance(s);
    }
}

static TokenType identify_keyword(StringSlice lexeme) {
    if (ss_eq_cstr(lexeme, "string"))  return TOKEN_KEYWORD_STRING;
    if (ss_eq_cstr(lexeme, "integer")) return TOKEN_KEYWORD_INT;
    if (ss_eq_cstr(lexeme, "float"))   return TOKEN_KEYWORD_FLOAT;
    if (ss_eq_cstr(lexeme, "set"))     return TOKEN_KEYWORD_SET;
    if (ss_eq_cstr(lexeme, "fn"))      return TOKEN_KEYWORD_FN;
    if (ss_eq_cstr(lexeme, "if"))      return TOKEN_KEYWORD_IF;

    return TOKEN_IDENTIFIER;
}

static Token scan_identifier(Source* s) {
    usize start_cursor = s->cursor;
    usize start_line   = s->line;
    usize start_column = s->column;

    while(is_ident_cont(peek(s))) {
        advance(s);
    }

    usize length = s->cursor - start_cursor;
    StringSlice lexeme = { .data = s->text + start_cursor, .length = length };

    return make_token(identify_keyword(lexeme), lexeme, start_line, start_column);
}

static Token scan_number(Source* s) {
    usize start_line   = s->line;
    usize start_column = s->column;
    usize start_cursor = s->cursor;

    while (is_digit(peek(s))) advance(s);

    bool is_float = false;
    if (peek(s) == '.' && is_digit(peek_at(s, 1))) {
        is_float = true;
        advance(s);
        while (is_digit(peek(s))) advance(s);
    }

    usize length       = s->cursor - start_cursor;
    StringSlice lexeme = (StringSlice) { .data = s->text + start_cursor, .length = length};

    char buffer[32];
    LOG_ASSERT(length < sizeof(buffer), "number literal too long");
    memcpy(buffer, lexeme.data, length);
    buffer[length] = '\0';

    if (is_float) {
        Token tk_float = make_token(TOKEN_LITERAL_FLOAT, lexeme, start_line, start_column);
        tk_float.literal.float_value = strtod(buffer, nil);
        return tk_float;
    }

    Token tk_integer = make_token(TOKEN_LITERAL_INTEGER, lexeme, start_line, start_column);
    tk_integer.literal.integer_value = strtoll(buffer, nil, 10); // base 10
    return tk_integer;
}

static Token scan_string(Source* s) {
    usize start_line   = s->line;
    usize start_column = s->column;

    advance(s); // consumes initial '"'
    usize content_start = s->cursor;

    while (!at_end(s) && peek(s) != '"') {
        advance(s);
    }

    usize       length = s->cursor - content_start;
    StringSlice lexeme = (StringSlice) { .data = s->text + content_start, .length = length };

    if (peek(s) == '"') advance(s);
    else LOG_FATAL("Unterminated string");

    return make_token(TOKEN_LITERAL_STRING, lexeme, start_line, start_column);
}

static Token scan_punctuation(Source* s) {
    usize start_line   = s->line;
    usize start_column = s->column;
    char  punct        = advance(s);

    switch (punct) {
        // syntax puncts
        case ':': return make_token(TOKEN_COLON,             SS(":"),  start_line, start_column);
        case ';': return make_token(TOKEN_SEMICOLON,         SS(";"),  start_line, start_column);
        case ',': return make_token(TOKEN_COMMA,             SS(","),  start_line, start_column);
        case '.': return make_token(TOKEN_DOT,               SS("."),  start_line, start_column);
        case '(': return make_token(TOKEN_LEFT_PARENTHESES,  SS("("),  start_line, start_column);
        case ')': return make_token(TOKEN_RIGHT_PARENTHESES, SS(")"),  start_line, start_column);
        case '{': return make_token(TOKEN_LEFT_BRACE,        SS("{"),  start_line, start_column);
        case '}': return make_token(TOKEN_RIGHT_BRACE,       SS("}"),  start_line, start_column);
        case '[': return make_token(TOKEN_LEFT_BRACKET,      SS("["),  start_line, start_column);
        case ']': return make_token(TOKEN_RIGHT_BRACKET,     SS("]"),  start_line, start_column);
        // operators puncts
        case '+': return make_token(TOKEN_PLUS,              SS("+"),  start_line, start_column);
        case '-': return make_token(TOKEN_MINUS,             SS("-"),  start_line, start_column);
        case '*': return make_token(TOKEN_STAR,              SS("*"),  start_line, start_column);
        case '/': return make_token(TOKEN_SLASH,             SS("/"),  start_line, start_column);
        case '=': {
            if (peek(s) == '=') {
                advance(s);
                return make_token(TOKEN_EQEQ, SS("=="), start_line, start_column);
            }
            return make_token(TOKEN_ASSIGN,   SS("="),  start_line, start_column);
            // in future (=>) for lambda func
        }
        case '!': {
            if (peek(s) == '=') {
                advance(s);
                return make_token(TOKEN_NOTEQ,  SS("!="), start_line, start_column);
            }
            return make_token(TOKEN_NOT, SS("!"),   start_line, start_column);
        }
        case '>': {
            if (peek(s) == '=') {
                advance(s);
                return make_token(TOKEN_GTE, SS(">="), start_line, start_column);
            }
            return make_token(TOKEN_GT,      SS(">"),  start_line, start_column);
        }
        case '<': {
            if (peek(s) == '=') {
                advance(s);
                return make_token(TOKEN_LTE, SS("<="), start_line, start_column);
            }
            return make_token(TOKEN_LT,      SS("<"),  start_line, start_column);
        }
        // ||, |>, ->
        default:
            LOG_FATAL("Unkown type of token punctuation: '%c'", punct);
    }
}

static Token next_token(Source* s) {
    skip_irrelevant(s);
    skip_whitespaces(s);
    if (at_end(s)) return make_token(TOKEN_EOF, SS(""), s->line, s->column);

    char current = peek(s);

    if (is_identifier_start(current)) return scan_identifier(s);
    if (is_digit(current)) return scan_number(s);
    if (current == '"') return scan_string(s);
    return scan_punctuation(s);
}

Vector tokenize(Source* s) {
    Vector tokens = vec_of(Token);

    LOG_DEBUG("Starting tokenizer");
    while(true) {
        Token token = next_token(s);
        vec_push(&tokens, &token);
        if (token.type == TOKEN_EOF) break;
    }

    return tokens;
}
