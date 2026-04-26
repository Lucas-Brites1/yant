#include "../include/yant_context.h"
#include "../include/blobberman.h"
#include "../include/logc.h"
#include "../include/yant_types.h"

YantContext yant_context_init(Memory tokens_blob_size, Memory string_blob_size, Memory ast_blob_size, Memory runtime_blob_size) {
    return (YantContext) {
        .tokens  = create_new_blob(tokens_blob_size,  REGSIZE),
        .strings = create_new_blob(string_blob_size,  REGSIZE),
        .ast     = create_new_blob(ast_blob_size,     REGSIZE),
        .runtime = create_new_blob(runtime_blob_size, REGSIZE),
    };
}

void yant_context_free(YantContext* ctx) {
    LOG_ASSERT(ctx && ctx->ast && ctx->runtime && ctx->strings && ctx->tokens, "Yant context corrupted, some pointers are null");
    blob_return(&ctx->tokens);
    blob_return(&ctx->strings);
    blob_return(&ctx->ast);
    blob_return(&ctx->runtime);
}
