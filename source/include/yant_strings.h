#pragma once

#include "../include/yant_types.h"
#include "blobberman.h"

typedef struct {
    const char* data;
    usize length;
} StringSlice;

#define SS(literal) ((StringSlice){ (literal), sizeof(literal) - 1})
#define SS_FMT      "%.*s"
#define SS_ARG(ss)  (i32)(ss).length, (ss).data

bool ss_eq(StringSlice ssA, StringSlice ssB);
bool ss_eq_cstr(StringSlice ss, const char* cstr);
i64  ss_cmp(StringSlice ssA, StringSlice ssB);

StringSlice ss_view(const char* cstr);
StringSlice ss_clone(Blob* arena, const char* cstr);
StringSlice ss_clone_ss(Blob*arena, StringSlice ss);
