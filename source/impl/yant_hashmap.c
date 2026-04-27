#include "../include/yant_hashmap.h"
#include "../include/logc.h"
#include <stdlib.h>

static usize next_pow2(usize n) {
    usize p = 1;
    while (p < n) p <<= 1;
    return p;
}

static inline bool need_resize(Map* map) {
    return (map->count + 1) * 10 >= map->capacity * 7;
}

static usize find_spot(Map* map, StringSlice key) {
    usize mask = map->capacity - 1;
    usize i = ss_hash(key) & mask;
    while (map->pairs[i].occupied && !ss_eq(map->pairs[i].key, key)) {
        i = (i + 1) & mask;
    }
    return i;
}

static void resize(Map* map) {
    Pair* old_pairs = map->pairs;
    usize old_cap = map->capacity;
    usize new_cap = old_cap * 2;

    Pair* new_pairs = calloc(new_cap, sizeof(Pair));
    LOG_ASSERT(new_pairs, "failed to resize hashmap");

    map->pairs = new_pairs;
    map->capacity = new_cap;

    usize mask = new_cap - 1;
    for (usize i = 0; i < old_cap; i++) {
        if (!old_pairs[i].occupied) continue;
        usize j = ss_hash(old_pairs[i].key) & mask;
        while (new_pairs[j].occupied) {
            j = (j + 1) & mask;
        }
        new_pairs[j] = old_pairs[i];
    }

    free(old_pairs);
}

Map hmap_create(usize initial_capacity) {
    usize cap = next_pow2(initial_capacity < HMAP_INITIAL_CAP
                          ? HMAP_INITIAL_CAP : initial_capacity);
    Pair* pairs = calloc(cap, sizeof(Pair));
    LOG_ASSERT(pairs, "failed to allocate hashmap");

    return (Map) {
        .pairs    = pairs,
        .count    = 0,
        .capacity = cap,
    };
}

void hmap_free(Map* map) {
    LOG_ASSERT(map, "null map");
    if (map->pairs) {
        free(map->pairs);
        map->pairs = NULL;
    }
    map->count = 0;
    map->capacity = 0;
}

void hmap_set(Map* map, StringSlice key, void* element) {
    if (need_resize(map)) resize(map);

    usize idx = find_spot(map, key);
    bool was_empty = !map->pairs[idx].occupied;

    map->pairs[idx].key = key;
    map->pairs[idx].element = element;
    map->pairs[idx].occupied = true;

    if (was_empty) map->count++;
}

bool hmap_insert(Map* map, StringSlice key, void* element) {
    if (hmap_has(map, key)) return false;
    hmap_set(map, key, element);
    return true;
}

bool hmap_update(Map* map, StringSlice key, void* element) {
    if (!hmap_has(map, key)) return false;

    usize idx = find_spot(map, key);
    map->pairs[idx].element = element;
    return true;
}

void* hmap_get(Map* map, StringSlice key) {
    usize idx = find_spot(map, key);
    if (!map->pairs[idx].occupied) return NULL;
    return map->pairs[idx].element;
}

bool hmap_has(Map* map, StringSlice key) {
    usize idx = find_spot(map, key);
    return map->pairs[idx].occupied;
}

void hmap_print(Map* map, void (*value_printer)(void* value)) {
    for (usize i = 0; i < map->capacity; i++) {
        if (!map->pairs[i].occupied) continue;

        printf("  " SS_FMT " -> ", SS_ARG(map->pairs[i].key));
        value_printer(map->pairs[i].element);
        printf("\n");
    }
}
