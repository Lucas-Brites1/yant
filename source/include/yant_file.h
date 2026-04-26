#pragma once
#include "./yant_types.h"
#include <assert.h>

typedef struct {
    usize cursor;
    usize size;

    usize line;
    usize column;

    const char* text;
    const char* filename;
} Source;

Source source_load(const char* filename);
void source_free(Source* source);

static inline char advance(Source* s) {
    assert(s->text);
    if (s->cursor >= s->size) return '\0';
    char c = s->text[s->cursor++];
    if (c == '\0') {
        s->line++;
        s->column = 1;
    } else {
        s->column++;
    }
    return c;
}

static inline char peek(Source* s) {
    assert(s->text);
    if (s->cursor >= s->size) return '\0';
    return s->text[s->cursor];
}

static inline char peek_at(Source* s, usize offset) {
    assert(s->text);
    if (s->cursor + offset >= s->size) return '\0';
    return s->text[s->cursor + offset];
}

static inline bool at_end(Source* s) {
    return s->cursor >= s->size;
}

static inline bool match(Source* s, char expected) {
    if (peek(s) != expected) return false;
    advance(s);
    return true;
}
