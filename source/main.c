#define LOGC_IMPLEMENTATION
#define BLOBBERMAN_IMPLEMENTATION
#include "include/yant_ast.h"
#include "include/yant_file.h"
#include "./include/yant_context.h"
#include "./include/yant_lexer.h"
#include "./include/yant_token.h"
#include "./include/yant_parser.h"
#include "./include/yant_interpreter.h"
#include "./include/yant_value.h"
#include "./include/yant_types.h"
#include "./include/yant_vector.h"
#include "./include/yant_hashmap.h"
#include "./include/yant_strings.h"
#include "./include/logc.h"
#include "./include/blobberman.h"


int main(void) {
    YantContext  ctx = yant_context_init(Kib_(50), Kib_(50), Kib_(50), Kib_(50));
    Source      code = source_load("./yant_files/eval_test.yn");
    Vector      tokens = tokenize(&code);
    Parser      parser = parser_create(&ctx, &tokens);
    Vector      nodes  = parse(&parser);
    Interpreter interpreter = interpreter_create(&ctx, &nodes);
    Vector      values = interpret(&interpreter);

    vec_foreach(Node*, node, &nodes) {
        node_print(*node, 1);
    }
    LOG_BLANK;

    vec_foreach(Value, value, &values) {
        Value v = *value;
        switch(v.type) {
            case VALUE_FLOAT:
                LOG_DEBUG("%s(%f)", value_type_str(value->type), v.as_float);
                continue;
            case VALUE_INT:
                LOG_DEBUG("%s(%ld)", value_type_str(value->type), v.as_int);
                continue;
            case VALUE_STRING:
                LOG_DEBUG("%s(" SS_FMT")", value_type_str(value->type), SS_ARG(value->as_string));
                continue;
            case VALUE_NIL:
                LOG_DEBUG("%s", value_type_str(value->type));
                continue;
            default: LOG_DEBUG("%s not implemented yet", value_type_str(value->type));
        }
    }

    vec_free(&tokens);
    vec_free(&nodes);
    vec_free(&values);
    source_free(&code);
    interpreter_free(&interpreter);
    yant_context_free(&ctx);
}
