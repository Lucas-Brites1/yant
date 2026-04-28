#pragma once
#include "./yant_types.h"

static inline bool is_letter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static inline bool is_digit(char c) {
    return (c >= '0' && c <= '9');
}

static inline bool is_alnum(char c) {
    return is_letter(c) || is_digit(c);
}

static inline bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static inline bool is_identifier_start(char c) {
    return is_letter(c) || c == '_';
}

static inline bool is_ident_cont(char c) {
    return is_alnum(c) || c == '_';
}
