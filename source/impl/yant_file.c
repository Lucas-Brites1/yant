#include "../include/yant_file.h"
#include "../include/yant_types.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

Source source_load(const char* filename) {
    FILE* file = fopen(filename, "r");
    assert(file);

    fseek(file, 0, SEEK_END);
    usize size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = malloc(size + 1);
    usize read   = fread(buffer, 1, size, file);
    buffer[read] = '\0';

    fclose(file);
    return (Source) {
        .filename = filename,
        .text = buffer,
        .size = size,
        .line = 1,
        .column = 1,
        .cursor = 0,
    };
}

void source_free(Source* source) {
    assert(source && source->text);
    free((void*)source->text);
    source->text=nil;
    source->size=0;
    source->cursor=0;
}
