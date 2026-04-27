#pragma once

#include "yant_ast.h"
#include "yant_context.h"
#include "yant_vector.h"
#include "yant_types.h"

#define TYPE_IN(actual, ...) \
    _token_in_macro_helper((actual), (TokenType[]){__VA_ARGS__}, \
             sizeof((TokenType[]){__VA_ARGS__}) / sizeof(TokenType))

static inline bool _token_in_macro_helper(TokenType actual, const TokenType* options, usize count) {
    for (usize i = 0; i < count; i++) {
        if (actual == options[i]) return true;
    }
    return false;
}

typedef struct {
    YantContext* yant_ctx;
    Vector*      tokens;
    usize        current;
} Parser;

Parser parser_create(YantContext* yant_context, Vector* tokens);
Vector parse(Parser* parser);
