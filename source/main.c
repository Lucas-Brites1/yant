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
#include <time.h>
#include <stdlib.h>
#include <math.h>

int main(void) {
    srand(time(nil));

    logc_set_level(LOGC_DEBUG);
    logc_set_show_location(true);
    logc_set_show_time(false);

    YantContext ctx = yant_context_init(Kib_(50), Kib_(50), Kib_(50), Kib_(50));
    Source      src = source_load("./yant_files/testes.yn");

    Vector   tokens = tokenize(&src);

    vec_foreach(Token, tk, &tokens) {
        LOG_DEBUG("%s", token_type_str(tk->type));
    }

    Parser   parser = parser_create(&ctx, &tokens);
    Vector   nodes  = parse(&parser);
    Interpreter intr= interpreter_create(&ctx, &nodes);
    interpret(&intr);

    vec_foreach(Node*, nd, &nodes) {
        node_free(*nd);
    }

    /*
    LOG_DEBUG("Strings blob:");
    blob_print_stats(ctx.strings);
    LOG_LINE;

    LOG_DEBUG("Tokens blob:");
    blob_print_stats(ctx.tokens);
    LOG_LINE;

    LOG_DEBUG("Ast blob:");
    blob_print_stats(ctx.ast);
    LOG_LINE;

    LOG_DEBUG("Runtime blob:");
    blob_print_stats(ctx.runtime);
    LOG_LINE;
    */

    vec_free(&tokens);
    vec_free(&nodes);
    interpreter_free(&intr);
    source_free(&src);
    yant_context_free(&ctx);
    return 0;
}
