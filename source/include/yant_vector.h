#pragma once

#include "yant_types.h"
#include "blobberman.h"

typedef struct Vector Vector;

#define VEC_INITIAL_CAP 32
struct Vector {
    usize len;
    usize cap;
    usize element_size;
    byte* data;
};

#define vec_of(T) vec_new(sizeof(T))
Vector  vec_new(usize element_size);
void    vec_free(Vector* vec);

void vec_push(Vector* vec, void* element);
#define vec_at(T, vec, index) *((T*)vec_get(vec, index))
#define vec_ref(T, vec, index) ((T*)vec_get(vec, index))
void* vec_get(Vector* vec, usize index);

#define vec_foreach(T, var, vec) \
    for (usize _i=0; _i<(vec)->len; _i++) \
        for (T* var = vec_ref(T, vec, _i); var; var = nil)
