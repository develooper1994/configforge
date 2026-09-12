/*
 * cf_alloc.h - allocation interface and arena.
 *
 * Every allocation in the library passes through a cf_allocator, so a
 * caller can supply their own, and so the fixed-capacity profile can
 * refuse to allocate at all rather than growing silently.
 *
 * Snapshots are immutable and are freed whole, which is exactly what
 * an arena is for: allocation is a pointer bump and release is one
 * call, with no per-object bookkeeping anywhere.
 */

#ifndef CONFIGFORGE_CF_ALLOC_H
#define CONFIGFORGE_CF_ALLOC_H

#include "configforge/cf_port.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cf_allocator {
    void* (*alloc)(void* ctx, size_t n);
    void (*release)(void* ctx, void* p);
    void* ctx;
} cf_allocator;

/* malloc and free. NULL on targets built with CF_NO_STDLIB. */
const cf_allocator* cf_default_allocator(void);

/* ------------------------------------------------------------------ *
 * Arena
 *
 * Two flavours. A growing arena takes blocks from an allocator; a
 * fixed arena works inside a buffer the caller supplies and fails
 * cleanly when it is exhausted, which is the freestanding profile.
 * ------------------------------------------------------------------ */

typedef struct cf_arena cf_arena;

cf_arena* cf_arena_new(const cf_allocator* a, size_t block_size);
cf_arena* cf_arena_fixed(void* buffer, size_t size);
void      cf_arena_free(cf_arena* ar);

void* cf_arena_alloc(cf_arena* ar, size_t n);
void* cf_arena_calloc(cf_arena* ar, size_t n);

/* Copy len bytes and terminate. Stored text is NUL-terminated in
   addition to carrying a length, so the convenience accessors are
   free and an embedded NUL is detectable by the caller. */
char* cf_arena_dup(cf_arena* ar, const char* s, size_t len);

size_t  cf_arena_used(const cf_arena* ar);
size_t  cf_arena_capacity(const cf_arena* ar);
cf_bool cf_arena_is_fixed(const cf_arena* ar);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_ALLOC_H */
