#include <stdbool.h>
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
    logc_set_level(LOGC_DEBUG);
    logc_set_show_location(true);
    logc_set_blank_marker("$:");

    YantContext  ctx = yant_context_init(Kib_(50), Kib_(50), Kib_(50), Kib_(50));
    Source      code = source_load("./yant_files/eval_test.yn");
    Vector      tokens = tokenize(&code);

    LOG_BLANK;
    vec_foreach(Token, tk, &tokens) {
        LOG_DEBUG("%s", token_type_str(tk->type));
    }

    Parser      parser = parser_create(&ctx, &tokens);
    Vector      nodes  = parse(&parser);
    Interpreter interpreter = interpreter_create(&ctx, &nodes);

    LOG_BLANK;
    vec_foreach(Node*, node, &nodes) {
        node_print(*node, 1);
    }
    LOG_BLANK;
    interpret(&interpreter);
    hmap_print(&interpreter.environ, value_print);

    vec_free(&tokens);
    vec_free(&nodes);
    source_free(&code);
    interpreter_free(&interpreter);
    yant_context_free(&ctx);
}
