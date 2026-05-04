#pragma once
#include "yant_hashmap.h"
#include "yant_parser.h"
#include "yant_ast.h"
#include "yant_token.h"
#include "yant_vector.h"
#include "yant_context.h"
#include "logc.h"

typedef struct Interpreter Interpreter;
struct Interpreter {
    YantContext* yant_ctx;
    Vector*      nodes;
    Vector       scopes; // Vector<Map*>
    usize        current;
} ;

Interpreter interpreter_create(YantContext* yant_context, Vector* nodes);
void        interpreter_free(Interpreter* i);

void        interpret(Interpreter* interpreter);
