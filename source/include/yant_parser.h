#pragma once

#include "yant_ast.h"
#include "yant_context.h"
#include "yant_vector.h"
#include "yant_types.h"

typedef struct {
    YantContext* yant_ctx;
    Vector*      tokens;
    usize        current;
} Parser;

Parser parser_create(YantContext* yant_context, Vector* tokens);
Vector parse(Parser* parser);
