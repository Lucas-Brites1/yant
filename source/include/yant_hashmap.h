#pragma once
#include "yant_strings.h"
#include "yant_types.h"
#include <stdbool.h>

#define HMAP_INITIAL_CAP 32

#define hmap_get_as(T, map, key) ((T*)hmap_get((map), (key)))

typedef struct {
    StringSlice key;
    void*       element;
    bool        occupied;
} Pair;

typedef struct {
    Pair*  pairs;
    usize  count;
    usize  capacity;
} Map;

#define FNV1A_OFFSET_BASIS 0xcbf29ce484222325ULL
#define FNV1A_PRIME        0x100000001b3ULL

static inline usize fnv1a_hash(const char* key, usize len) {
    usize hash = FNV1A_OFFSET_BASIS;
    for (usize i = 0; i < len; i++) {
        hash ^= (u8)key[i];
        hash *= FNV1A_PRIME;
    }
    return hash;
}

static inline usize ss_hash(StringSlice ss) {
    return fnv1a_hash(ss.data, ss.length);
}

Map   hmap_create(usize initial_capacity);
void  hmap_free(Map* map);
void  hmap_set(Map* map, StringSlice key, void* element);
bool  hmap_insert(Map* map, StringSlice key, void* element);
bool  hmap_update(Map* map, StringSlice key, void* element);
void* hmap_get(Map* map, StringSlice key);
bool  hmap_has(Map* map, StringSlice key);
