#ifndef BLOBBERMAN_H
#define BLOBBERMAN_H

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef unsigned char byte;
typedef size_t usize;

#define BLOBBERMAN_MEMORY_SPECIFIER(MEM) \
  MEM(BYTE, Byte_, 1ULL, "Byte") \
  MEM(KIB,  Kib_, 1024ULL, "Kibibyte") \
  MEM(MIB,  Mib_, 1024ULL * 1024ULL, "Mebibyte") \
  MEM(GIB,  Gib_, 1024ULL * 1024ULL * 1024ULL, "Gibibyte")

#define EXTENDS_MEM_ENUM(ENUM, CONSTRUCTOR, VALUE, STR) ENUM = VALUE,
typedef enum blobberman_memory_specifier {
  BLOBBERMAN_MEMORY_SPECIFIER(EXTENDS_MEM_ENUM)
} MemoryType;
#undef EXTENDS_MEM_ENUM

typedef struct blobberman_memory_t {
  usize       amount;
  MemoryType  type;
} Memory;

typedef struct blobberman_chunk_t {
  struct blobberman_chunk_t* next;
  usize capacity;
  usize used;
  byte  blobinhos[];
} BlobChunk;

typedef struct blobberman_blob_t {
  BlobChunk* head;
  BlobChunk* tail;
  usize alignment;
  usize default_chunk_size;  /* tamanho usado quando precisa criar chunk novo */
} Blob;

#define CAST(AS_TYPE, VALUE) ((AS_TYPE)VALUE)
#define DEREF(POINTER)       (*POINTER)
#define REGSIZE              sizeof(void*)
#define ALIGNMENT_32         CAST(usize, 32)
#define ALIGNMENT_64         CAST(usize, 64)

static inline usize blob_alignment_by(usize alignment, usize size) {
  return (size + (alignment - 1)) & ~(alignment - 1);
}

static inline bool chunk_has_space(BlobChunk* chunk, usize data_size) {
  return chunk && (chunk->capacity - chunk->used) >= data_size;
}

static inline usize chunk_remaining_space(BlobChunk* chunk) {
  return chunk ? (chunk->capacity - chunk->used) : 0;
}

static inline usize blob_remaining_space(Blob* blob) {
  return blob ? chunk_remaining_space(blob->tail) : 0;
}

static inline usize blob_chunk_count(Blob* blob) {
  usize count = 0;
  if (!blob) return 0;
  for (BlobChunk* c = blob->head; c; c = c->next) count++;
  return count;
}

static inline usize blob_total_capacity(Blob* blob) {
  usize total = 0;
  if (!blob) return 0;
  for (BlobChunk* c = blob->head; c; c = c->next) total += c->capacity;
  return total;
}

/* Soma o uso total de todos os chunks. */
static inline usize blob_total_used(Blob* blob) {
  usize total = 0;
  if (!blob) return 0;
  for (BlobChunk* c = blob->head; c; c = c->next) total += c->used;
  return total;
}

#define EXTENDS_MEM_STRING(ENUM, CONSTRUCTOR, VALUE, STR) case ENUM: return STR;
static inline const char* memory_type_str(MemoryType t) {
  switch (t) {
    BLOBBERMAN_MEMORY_SPECIFIER(EXTENDS_MEM_STRING)
    default: return "Unknown";
  }
}
#undef EXTENDS_MEM_STRING

#define EXTENDS_CONSTRUCTOR(ENUM, CONSTRUCTOR, VALUE, STR) \
  static inline Memory CONSTRUCTOR(usize quantity) { \
    return (Memory){ .amount = VALUE * quantity, .type = ENUM }; \
  }
BLOBBERMAN_MEMORY_SPECIFIER(EXTENDS_CONSTRUCTOR)
#undef EXTENDS_CONSTRUCTOR

Blob* create_new_blob(Memory blob_size, usize data_alignment);
void* blob_reserve(Blob* blob, usize data_size);
void  blob_return(Blob** blob);

void  blob_print_stats(Blob* blob);

#ifdef BLOBBERMAN_IMPLEMENTATION

static BlobChunk* create_chunk(usize capacity_aligned) {
  usize chunk_header = sizeof(BlobChunk);
  usize total = chunk_header + capacity_aligned;
  BlobChunk* chunk = CAST(BlobChunk*, calloc(1, total));
  if (!chunk) {
    fprintf(stderr, "blobberman: out of memory creating chunk\n");
    exit(1);
  }
  chunk->next = NULL;
  chunk->capacity = capacity_aligned;
  chunk->used = 0;
  return chunk;
}

Blob* create_new_blob(Memory blob_size, usize data_alignment) {
  Blob* blob = CAST(Blob*, calloc(1, sizeof(Blob)));
  if (!blob) {
    fprintf(stderr, "blobberman: out of memory creating blob\n");
    exit(1);
  }

  usize cap_aligned = blob_alignment_by(data_alignment, blob_size.amount);

  blob->alignment = data_alignment;
  blob->default_chunk_size = cap_aligned;
  blob->head = create_chunk(cap_aligned);
  blob->tail = blob->head;

  return blob;
}

void* blob_reserve(Blob* blob, usize data_size) {
  assert(blob && "blob_reserve called with NULL blob");
  assert(data_size > 0 && "blob_reserve called with zero size");

  usize data_size_aligned = blob_alignment_by(blob->alignment, data_size);

  if (!chunk_has_space(blob->tail, data_size_aligned)) {
    usize new_chunk_size = blob->default_chunk_size;
    if (data_size_aligned > new_chunk_size) {
      new_chunk_size = blob_alignment_by(blob->alignment, data_size_aligned);
    }

    BlobChunk* new_chunk = create_chunk(new_chunk_size);
    blob->tail->next = new_chunk;
    blob->tail = new_chunk;
  }

  void* data = blob->tail->blobinhos + blob->tail->used;
  blob->tail->used += data_size_aligned;
  return data;
}

void blob_return(Blob** blob_ref) {
  assert(blob_ref && *blob_ref);
  Blob* blob = *blob_ref;

  BlobChunk* current = blob->head;
  while (current) {
    BlobChunk* next = current->next;
    free(current);
    current = next;
  }

  free(blob);
  *blob_ref = NULL;
}

void blob_print_stats(Blob* blob) {
  if (!blob) {
    printf("blob: NULL\n");
    return;
  }
  usize chunks = blob_chunk_count(blob);
  usize used = blob_total_used(blob);
  usize cap = blob_total_capacity(blob);
  printf("blob stats: %zu chunk(s), %zu/%zu bytes used (%.1f%%)\n",
         chunks, used, cap,
         cap > 0 ? (100.0 * used / cap) : 0.0);
}

#endif /* BLOBBERMAN_IMPLEMENTATION */
#endif /* BLOBBERMAN_H */
