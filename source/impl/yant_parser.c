#include "../include/yant_token.h"
#include "../include/yant_ast.h"
#include "../include/yant_parser.h"
#include "../include/yant_context.h"
#include "../include/yant_vector.h"
#include "../include/yant_types.h"
#include "../include/logc.h"
#include <stdbool.h>

#define TYPE_IN(actual, ...) \
    _token_in_macro_helper((actual), (TokenType[]){__VA_ARGS__}, \
             sizeof((TokenType[]){__VA_ARGS__}) / sizeof(TokenType))

static inline Token peek(Parser* p) {
    return vec_at(Token, p->tokens, p->current);
}

static inline Token peek_at(Parser* p, usize offset) {
    LOG_ASSERT(
        offset + p->current < p->tokens->len,
        "Out-of-bounds access: current=%zu offset=%zu len=%zu",
        p->current, offset, p->tokens->len
    );
    return vec_at(Token, p->tokens, p->current + offset);
}

static inline Token advance(Parser* p) {
    Token tk = peek(p);
    p->current++;
    return tk;
}

static inline bool check(Parser* p, TokenType type) {
    return peek(p).type == type;
}

static inline bool match(Parser* p, TokenType type) {
    if (check(p, type)) {
        advance(p);
        return true;
    }
    return false;
}

static inline Token expect(Parser* p, TokenType expected) {
    if (!check(p, expected)) {
        LOG_FATAL(
            "expected %s, got %s at line %zu",
            token_type_str(expected), token_type_str(peek(p).type), peek(p).line
        );
    }
    return advance(p);
}

static inline bool is_eof(Parser* p) {
    return peek(p).type == TOKEN_EOF;
}

Parser parser_create(YantContext* yant_context, Vector* tokens) {
    LOG_ASSERT(yant_context && tokens, "Error while trying to create Parser due to possible null pointers");
    return (Parser) {
        .yant_ctx = yant_context,
        .tokens   = tokens,
        .current  = 0
    };
}

static inline bool _token_in_macro_helper(TokenType actual, const TokenType* options, usize count) {
    for (usize i = 0; i < count; i++) {
        if (actual == options[i]) return true;
    }
    return false;
}

static inline bool is_type_keyword(Parser* p) {
    return TYPE_IN(
        peek(p).type,
        TOKEN_KEYWORD_STRING,
        TOKEN_KEYWORD_INT,
        TOKEN_KEYWORD_FLOAT,
        TOKEN_KEYWORD_BOOLEAN
    );
}


static inline bool is_statement_keyword(Parser* p) {
    return TYPE_IN(peek(p).type,
        TOKEN_KEYWORD_SET,
        TOKEN_KEYWORD_FN,
        TOKEN_KEYWORD_IF
        //TOKEN_KEYWORD_LOOP
    );
}

static inline bool is_addition_op(Parser* p) {
    return TYPE_IN(
        peek(p).type,
        TOKEN_PLUS,
        TOKEN_MINUS
    );
}

static inline bool is_comparision_op(Parser* p) {
    return TYPE_IN(
        peek(p).type,
        TOKEN_GTE,
        TOKEN_LTE,
        TOKEN_GT,
        TOKEN_LT
    );
}

static inline bool is_multiplication_op(Parser* p) {
    return TYPE_IN(
        peek(p).type,
        TOKEN_STAR,
        TOKEN_SLASH
    );
}

// Statement Functions
static Node* parse_statement(Parser* p); // dispatcher
static Node* parse_declaration(Parser* p);
static Node* parse_assignment(Parser* p);

// Expression Functions
static Node* parse_expression(Parser* p); // dispatcher
static Node* parse_comparison(Parser* p);
static Node* parse_addition(Parser* p);
static Node* parse_multiplication(Parser* p);
static Node* parse_primary(Parser* p);

static Node* parse_expression(Parser* parser) {
    return parse_comparison(parser);
}

static Node* parse_comparison(Parser* p) {
    Node* left = parse_addition(p);

    while (is_comparision_op(p)) {
        Token op = advance(p);
        Node* right = parse_addition(p);

        left = Operation(p->yant_ctx, op.type, left, right);
    }

    return left;
}

static Node* parse_addition(Parser* p) {
    Node* left = parse_multiplication(p);

    while (is_addition_op(p)) {
        Token op = advance(p);
        Node* right = parse_multiplication(p);

        left = Operation(p->yant_ctx, op.type, left, right);
    }

    return left;
}

static Node* parse_multiplication(Parser* p) {
    Node* left = parse_primary(p);

    while (is_multiplication_op(p)) {
        Token op    = advance(p);
        Node* right = parse_primary(p);

        left = Operation(p->yant_ctx, op.type, left, right);
    }

    return left;
}

static Node* parse_primary(Parser* p) {
    if (check(p, TOKEN_IDENTIFIER)) {
        Token tk_identifier = advance(p);
        return Identifier(p->yant_ctx, tk_identifier.lexeme, tk_identifier.line, tk_identifier.column);
    }
    if (check(p, TOKEN_LITERAL_INTEGER)) {
        Token tk_int = advance(p);
        return LiteralInteger(p->yant_ctx, tk_int.literal.integer_value, tk_int.line, tk_int.column);
    }
    if (check(p, TOKEN_LITERAL_FLOAT)) {
        Token tk_float = advance(p);
        return LiteralFloat(p->yant_ctx, tk_float.literal.float_value, tk_float.line, tk_float.column);
    }
    if (check(p, TOKEN_LITERAL_STRING)) {
        Token tk_string = advance(p);
        return LiteralString(p->yant_ctx, tk_string.lexeme, tk_string.line, tk_string.column);
    }
    if (check(p, TOKEN_LITERAL_BOOLEAN)) {
        Token tk_bool = advance(p);
        return LiteralBoolean(p->yant_ctx, tk_bool.literal.boolean_value, tk_bool.line, tk_bool.column);
    }
    if (check(p, TOKEN_LEFT_PARENTHESES)) {
        advance(p);
        Node* expr = parse_addition(p);
        expect(p, TOKEN_RIGHT_PARENTHESES);
        return expr;
    }

    LOG_FATAL("Parser: expected expression at line %zu", peek(p).line);
    return nil;
}


static Node* parse_statement(Parser* p) {
    if (is_type_keyword(p)) return parse_declaration(p);
    if (is_statement_keyword(p)) {
        if (check(p, TOKEN_KEYWORD_SET)) return parse_assignment(p);
        LOG_FATAL("Unreachable code");
    }

    return parse_expression(p);
}

static Node* parse_declaration(Parser* p) {
    // [TOKEN_KEYWORD_..., TOKEN_COLON, TOKEN_IDENTIFIER, TOKEN_LPARENTHESES, VALUE, TOKEN_RPARENTHESES]
    Token kind = advance(p);
    expect(p, TOKEN_COLON); // :
    Token identifier = expect(p, TOKEN_IDENTIFIER);
    expect(p, TOKEN_LEFT_PARENTHESES);
    Node* value = parse_expression(p);
    expect(p, TOKEN_RIGHT_PARENTHESES);
    return Declare(p->yant_ctx, kind.type, identifier.lexeme, value, kind.line, kind.column);
}

static Node* parse_assignment(Parser* p) {
    // [TOKEN_SET, TOKEN_COLON, TOKEN_IDENTIFIER, TOKEN_LPARENTHESES, VALUE, TOKEN_RPARENTHESES]
    Token set = advance(p); // consumes Token::Set
    expect(p, TOKEN_COLON);
    Token identifier = advance(p);
    expect(p, TOKEN_LEFT_PARENTHESES);
    Node* value = parse_expression(p);
    expect(p, TOKEN_RIGHT_PARENTHESES);
    return Assign(p->yant_ctx, identifier.lexeme, value, set.line, set.column);
}

Vector parse(Parser* p) {
    LOG_ASSERT(p && p->yant_ctx && p->tokens, "Failed trying to parse due to possible nil pointer");
    Vector nodes = vec_of(Node*);

    while (!is_eof(p)) {
        Node* node = parse_statement(p);
        vec_push(&nodes, &node);
    }

    return nodes;
}
