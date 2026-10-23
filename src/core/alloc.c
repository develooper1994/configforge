/*
 * alloc.c - allocation interface and arena. C89.
 */

#include "configforge/cf_alloc.h"

#include <string.h>

#ifndef CF_NO_STDLIB
#include <stdlib.h>
#endif

/* ------------------------------------------------------------------ *
 * Default allocator
 * ------------------------------------------------------------------ */

#ifndef CF_NO_STDLIB

static void* cf_std_alloc(void* ctx, size_t n) {
    (void)ctx;
    return malloc(n);
}

static void cf_std_release(void* ctx, void* p) {
    (void)ctx;
    free(p);
}

static const cf_allocator cf_std_allocator = {cf_std_alloc, cf_std_release, NULL};

const cf_allocator* cf_default_allocator(void) {
    return &cf_std_allocator;
}

#else

const cf_allocator* cf_default_allocator(void) {
    return NULL; /* freestanding: the caller must supply one */
}

#endif

/* ------------------------------------------------------------------ *
 * Arena
 *
 * Alignment is to the strictest scalar the library stores. A union is
 * the portable way to ask the compiler what that is; C89 has no
 * alignof.
 * ------------------------------------------------------------------ */

typedef union cf_align {
    void*  p;
    cf_i64 i;
    long   l;
#if CF_WITH_REAL
    double d;
#endif
} cf_align;

#define CF_ALIGN_TO sizeof(cf_align)

typedef struct cf_block {
    struct cf_block* next;
    size_t           size;
    size_t           used;
    /* Payload follows. char[1] rather than a flexible array member,
       which is C99. */
    char data[1];
} cf_block;

struct cf_arena {
    const cf_allocator* alloc;
    cf_block*           head;
    size_t              block_size;
    size_t              total;    /* bytes handed out          */
    size_t              capacity; /* bytes owned               */
    cf_bool             fixed;
    cf_bool             owns_self;
};

static size_t cf_round_up(size_t n) {
    size_t r = n % CF_ALIGN_TO;
    return (r == 0) ? n : n + (CF_ALIGN_TO - r);
}

/* Header size excluding the one-byte payload placeholder. */
static size_t cf_block_header(void) {
    return cf_round_up(sizeof(cf_block) - 1);
}

cf_arena* cf_arena_new(const cf_allocator* a, size_t block_size) {
    cf_arena* ar;

    if (a == NULL) {
        a = cf_default_allocator();
    }
    if (a == NULL) {
        return NULL;
    }
    /* A floor, not a suggestion. Below about a kilobyte an arena stops
       being an arena: each block carries a header, so tiny ones spend
       most of themselves on bookkeeping and turn the arena into a
       linked list of singletons with worse locality than malloc. A
       caller who wants a small budget wants cf_arena_fixed. */
    if (block_size < 1024) {
        block_size = 4096;
    }
    ar = (cf_arena*)a->alloc(a->ctx, sizeof(cf_arena));
    if (ar == NULL) {
        return NULL;
    }
    ar->alloc      = a;
    ar->head       = NULL;
    ar->block_size = block_size;
    ar->total      = 0;
    ar->capacity   = 0;
    ar->fixed      = CF_FALSE;
    ar->owns_self  = CF_TRUE;
    return ar;
}

cf_arena* cf_arena_fixed(void* buffer, size_t size) {
    cf_arena* ar;
    cf_block* b;
    char*     base = (char*)buffer;
    size_t    skip;
    size_t    need;

    if (buffer == NULL) {
        return NULL;
    }
    /* The caller's buffer need not be aligned. A static char array
       usually is not, and placing a struct at an unaligned address is
       undefined behaviour on every target that cares. Align first and
       charge the padding to the buffer. */
    {
        size_t addr = (size_t)base;
        size_t rem  = addr % CF_ALIGN_TO;
        skip        = (rem == 0) ? 0 : (CF_ALIGN_TO - rem);
    }
    if (size <= skip) {
        return NULL;
    }
    base += skip;
    size -= skip;

    need = cf_round_up(sizeof(cf_arena)) + cf_block_header();
    if (size <= need) {
        return NULL;
    }
    /* From void*, because the alignment was established above: a cast
       from char* makes Clang's -Wcast-align object - it cannot see the
       arithmetic - and a cast through (void*) in one expression is what
       clang-tidy's bugprone-casting-through-void objects to. */
    {
        void* at = base;
        ar       = (cf_arena*)at;
        at       = base + cf_round_up(sizeof(cf_arena));
        b        = (cf_block*)at;
    }

    b->next = NULL;
    b->size = size - need;
    b->used = 0;

    ar->alloc      = NULL;
    ar->head       = b;
    ar->block_size = b->size;
    ar->total      = 0;
    ar->capacity   = b->size;
    ar->fixed      = CF_TRUE;
    ar->owns_self  = CF_FALSE;
    return ar;
}

const cf_allocator* cf_arena_allocator(const cf_arena* ar) {
    return (ar != NULL && !ar->fixed) ? ar->alloc : NULL;
}

void cf_arena_free(cf_arena* ar) {
    cf_block*           b;
    const cf_allocator* a;

    if (ar == NULL || ar->fixed) {
        return;
    }
    a = ar->alloc;
    b = ar->head;
    while (b != NULL) {
        cf_block* next = b->next;
        a->release(a->ctx, b);
        b = next;
    }
    if (ar->owns_self) {
        a->release(a->ctx, ar);
    }
}

void* cf_arena_alloc(cf_arena* ar, size_t n) {
    cf_block* b;
    size_t    need;
    char*     p;

    if (ar == NULL) {
        return NULL;
    }
    need = cf_round_up(n == 0 ? 1 : n);

    b = ar->head;
    if (b != NULL && b->size - b->used >= need) {
        p = b->data + b->used;
        b->used += need;
        ar->total += need;
        return p;
    }
    if (ar->fixed) {
        return NULL; /* exhausted; resolution fails cleanly */
    }
    {
        size_t    bs         = (need > ar->block_size) ? need : ar->block_size;
        size_t    alloc_size = cf_block_header() + bs;
        cf_block* nb         = (cf_block*)ar->alloc->alloc(ar->alloc->ctx, alloc_size);
        if (nb == NULL) {
            return NULL;
        }
        nb->next = ar->head;
        nb->size = bs;
        nb->used = need;
        ar->head = nb;
        ar->capacity += bs;
        ar->total += need;
        return nb->data;
    }
}

void* cf_arena_calloc(cf_arena* ar, size_t n) {
    void* p = cf_arena_alloc(ar, n);
    if (p != NULL) {
        memset(p, 0, n);
    }
    return p;
}

char* cf_arena_dup(cf_arena* ar, const char* s, size_t len) {
    char* p;

    if (s == NULL) {
        return NULL;
    }
    p = (char*)cf_arena_alloc(ar, len + 1);
    if (p == NULL) {
        return NULL;
    }
    if (len > 0) {
        memcpy(p, s, len);
    }
    p[len] = '\0';
    return p;
}

size_t cf_arena_used(const cf_arena* ar) {
    return (ar != NULL) ? ar->total : 0;
}

size_t cf_arena_capacity(const cf_arena* ar) {
    return (ar != NULL) ? ar->capacity : 0;
}

cf_bool cf_arena_is_fixed(const cf_arena* ar) {
    return (ar != NULL) ? ar->fixed : CF_FALSE;
}
