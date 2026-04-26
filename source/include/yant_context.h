#pragma once
#include "./blobberman.h"

typedef struct {
    Blob* tokens;
    Blob* strings;
    Blob* ast;
    Blob* runtime;
} YantContext;

YantContext yant_context_init(Memory tokens_blob_size, Memory string_blob_size, Memory ast_blob_size, Memory runtime_blob_size);
void yant_context_free(YantContext* ctx);
