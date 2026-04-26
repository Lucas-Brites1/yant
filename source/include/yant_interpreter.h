#pragma once
#include "yant_hashmap.h"
#include "yant_parser.h"
#include "yant_ast.h"
#include "yant_token.h"
#include "yant_vector.h"
#include "yant_context.h"
#include "logc.h"

typedef struct {
    YantContext* yant_ctx;
    Vector*      nodes;
    Map          environ;
    usize        current;
} Interpreter;

Interpreter interpreter_create(YantContext* yant_context, Vector* nodes);
void        interpreter_free(Interpreter* i);

Vector      interpret(Interpreter* interpreter);
