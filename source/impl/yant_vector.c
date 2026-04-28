#include "../include/yant_types.h"
#include "../include/yant_vector.h"
#include <assert.h>
#include <linux/limits.h>
#include <stdlib.h>
#include <string.h>

static inline void vec_realloc(Vector* vec) {
    assert(vec);
    usize new_cap  = vec->cap == 0 ? VEC_INITIAL_CAP : vec->cap*2;
    byte* new_data = realloc(vec->data, new_cap * vec->element_size);
    assert(new_data);
    vec->data = new_data;
    vec->cap = new_cap;
}

Vector  vec_new(usize element_size) {
    return (Vector){
        .element_size = element_size,
        .data = nil,
        .cap = 0,
        .len = 0,
    };
}

void vec_free(Vector* vec) {
    assert(vec);
    free(vec->data);
    vec->data= nil;
    vec->len = 0;
    vec->cap = 0;
    vec->element_size = 0;
}

void vec_push(Vector* vec, void* element) {
    if (vec->len == vec->cap) {
        vec_realloc(vec);
    }

    memcpy(
        vec->data + vec->len * vec->element_size,
        element,
        vec->element_size
        );
    vec->len++;
}

void* vec_get(Vector* vec, usize index) {
    assert(vec && vec->len > index);
    return vec->data + index * vec->element_size;
}

void vec_pop(Vector* vec, void** out) {
    vec->len--;
    vec->len--;
    memcpy(out, (u8*)vec->data + vec->len * vec->element_size, vec->element_size);
}
