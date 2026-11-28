/*
 * error.c - the error model. C89.
 */

#include "configforge/cf_error.h"
#include "configforge/cf_alloc.h"
#include "configforge/cf_build_options.h"
#include "configforge/cf_port.h"

#include <stdarg.h>
#include <stddef.h>
#include <string.h>

#define ERROR_SIZE_1_7 offsetof(cf_error, where)
#define ERROR_SIZE_1_8 (offsetof(cf_error, reason) + sizeof(int))
#define ERROR_SIZE_2_0 (offsetof(cf_error, offset) + sizeof(size_t))
#define ERROR_SIZE_HEAP (offsetof(cf_error, cfi_heap) + 3u * sizeof(char*))

const char* cf_code_name(cf_code c) {
    switch (c) {
#define CF_CODE_X(sym, name, desc)                                                     \
    case sym:                                                                          \
        return name;
#include "configforge/cf_code.def"
#undef CF_CODE_X
    }
    return "?";
}

const char* cf_code_description(cf_code c) {
    switch (c) {
#define CF_CODE_X(sym, name, desc)                                                     \
    case sym:                                                                          \
        return desc;
#include "configforge/cf_code.def"
#undef CF_CODE_X
    }
    return "unknown";
}

const char* cf_reason_name(int reason) {
    switch (reason) {
#define CF_REASON_X(sym, name, desc)                                                   \
    case sym:                                                                          \
        return name;
#include "configforge/cf_reason.def"
#undef CF_REASON_X
    default:
        break;
    }
    return "?";
}

/* One entry a reason, indexed by its value, in the order of cf_reason.def. */
static const char* const severity_of[] = {
    "none",    "error",   "error",   "error",   "error",   "error",  "error",
    "error",   "warning", "warning", "warning", "error",   "error",  "error",
    "warning", "error",   "error",   "error",   "warning", "warning"};

static const char* const weakness_of[] = {
    "",        "", "CWE-176", "", "", "",        "", "", "CWE-674", "CWE-770",
    "CWE-400", "", "",        "", "", "CWE-694", "", "", "",        ""};

/* A reason added to cf_reason.def without a row here stops the build. */
typedef char cf_reason_severity_size[(sizeof severity_of / sizeof severity_of[0] ==
                                      (size_t)CF_REASON_STORAGE + 1)
                                         ? 1
                                         : -1];
typedef char cf_reason_weakness_size[(sizeof weakness_of / sizeof weakness_of[0] ==
                                      (size_t)CF_REASON_STORAGE + 1)
                                         ? 1
                                         : -1];

const char* cf_reason_severity(int reason) {
    if (reason < 0 || reason > CF_REASON_STORAGE) {
        return "?";
    }
    return severity_of[reason];
}

const char* cf_reason_weakness(int reason) {
    if (reason < 0 || reason > CF_REASON_STORAGE) {
        return "";
    }
    return weakness_of[reason];
}

const char* cf_reason_description(int reason) {
    switch (reason) {
#define CF_REASON_X(sym, name, desc)                                                   \
    case sym:                                                                          \
        return desc;
#include "configforge/cf_reason.def"
#undef CF_REASON_X
    default:
        break;
    }
    return "unknown";
}

void cf_error_place_at(cf_error* e, int reason, unsigned int line, unsigned int col,
                       size_t offset) {
    if (e != NULL && e->size >= ERROR_SIZE_1_8) {
        e->where.file = NULL;
        e->where.line = line;
        e->where.col  = col;
        e->reason     = reason;
        if (e->size >= ERROR_SIZE_2_0) {
            e->offset = offset;
        }
    }
}

void cf_error_place(cf_error* e, int reason, unsigned int line, unsigned int col) {
    cf_error_place_at(e, reason, line, col, (size_t)-1);
}

/* ------------------------------------------------------------------ *
 * The texts (D-176)
 *
 * Each of key, source and message lives in the error's inline store when
 * it fits and in one block from cf_default_allocator() when it does not.
 * An empty text points at a literal, so a cleared error holds nothing
 * of its own and may be copied by value.
 * ------------------------------------------------------------------ */

enum { TEXT_KEY = 0, TEXT_SOURCE = 1, TEXT_MESSAGE = 2 };

/* A block of `n` bytes for a long text, or NULL: a build with fixed limits
   never allocates, a target without an allocator has none. */
static char* text_block(size_t n) {
#if CF_FIXED_LIMITS
    (void)n;
    return NULL;
#else
    const cf_allocator* a = cf_default_allocator();

    if (a == NULL || a->alloc == NULL || a->release == NULL) {
        return NULL;
    }
    return (char*)a->alloc(a->ctx, n);
#endif
}

static void text_release(char* block) {
    const cf_allocator* a;

    if (block != NULL) {
        a = cf_default_allocator();
        if (a != NULL && a->release != NULL) {
            a->release(a->ctx, block);
        }
    }
}

/* `text` of `n` bytes becomes the text of `slot`. `block`, when it is not
   NULL, holds the text already and is adopted. The old block is released
   last, so that a text that points into it can be copied. An error whose
   `size` does not reach the blocks keeps its text inline, cut. */
static void text_put(cf_error* e, int slot, const char* text, size_t n, char* block) {
    char*        store;
    size_t       room;
    const char** at;
    char*        old   = NULL;
    cf_bool      owner = (e->size >= ERROR_SIZE_HEAP) ? CF_TRUE : CF_FALSE;

    switch (slot) {
    case TEXT_KEY:
        store = e->cfi_key;
        room  = sizeof e->cfi_key;
        at    = &e->key;
        break;
    case TEXT_SOURCE:
        store = e->cfi_source;
        room  = sizeof e->cfi_source;
        at    = &e->source;
        break;
    default:
        store = e->cfi_message;
        room  = sizeof e->cfi_message;
        at    = &e->message;
        break;
    }
    if (owner) {
        old = e->cfi_heap[slot];
    } else if (block != NULL) {
        /* A block made for an error that cannot own it: the inline store
           takes what fits and the block goes. */
        if (n >= room) {
            n = room - 1u;
        }
        memcpy(store, block, n);
        store[n] = '\0';
        *at      = store;
        text_release(block);
        return;
    }
    if (block != NULL) {
        *at = block;
    } else if (n == 0) {
        *at = "";
    } else {
        if (n >= room && owner) {
            block = text_block(n + 1u);
        }
        if (block != NULL) {
            memcpy(block, text, n);
            block[n] = '\0';
            *at      = block;
        } else {
            if (n >= room) {
                n = room - 1u;
            }
            memmove(store, text, n);
            store[n] = '\0';
            *at      = store;
        }
    }
    if (owner) {
        e->cfi_heap[slot] = block;
        if (old != block) {
            text_release(old);
        }
    }
}

/* The length of `s` ("" for NULL), held to `cap` when `cap` is not 0. */
static size_t text_len(const char* s, size_t cap) {
    size_t n = (s != NULL) ? strlen(s) : 0u;

    return (cap != 0u && n > cap) ? cap : n;
}

void cf_error_clear(cf_error* e) {
    if (e == NULL) {
        return;
    }
    e->code = CF_OK;
    text_put(e, TEXT_KEY, "", 0, NULL);
    text_put(e, TEXT_SOURCE, "", 0, NULL);
    text_put(e, TEXT_MESSAGE, "", 0, NULL);
    cf_error_place(e, CF_REASON_NONE, 0, 0);
}

void cf_error_copy(cf_error* dst, const cf_error* src) {
    if (dst == NULL || src == NULL || dst == src) {
        return;
    }
    /* 1.0's fields. A field added in 1.x is copied here only when
       dst->size reaches past it (offsetof + sizeof). The texts are
       copied into dst's own store, never pointed at in src. */
    dst->code = src->code;
    text_put(dst, TEXT_KEY, src->key, text_len(src->key, 0u), NULL);
    text_put(dst, TEXT_SOURCE, src->source, text_len(src->source, 0u), NULL);
    text_put(dst, TEXT_MESSAGE, src->message, text_len(src->message, 0u), NULL);
    if (dst->size >= ERROR_SIZE_1_8) {
        if (src->size >= ERROR_SIZE_1_8) {
            cf_error_place_at(dst, src->reason, src->where.line, src->where.col,
                              (src->size >= ERROR_SIZE_2_0) ? src->offset : (size_t)-1);
        } else {
            cf_error_place(dst, CF_REASON_NONE, 0, 0);
        }
    }
}

cf_bool cf_error_set(cf_error* e, cf_code code, const char* key, const char* source,
                     const char* fmt, ...) {
    va_list ap;
    char    tmp[CF_MESSAGE_MAX];

    /* A NULL error is not a mistake: it means the caller does not want
       the detail. Reporting unconditionally keeps every call site free
       of a guard. */
    if (e == NULL) {
        return CF_FALSE;
    }
    tmp[0] = '\0';
    if (fmt != NULL) {
        va_start(ap, fmt);
        cf_vfmt(tmp, sizeof tmp, fmt, ap);
        va_end(ap);
    }
    e->code = (int)code;
    text_put(e, TEXT_KEY, key, text_len(key, (size_t)CF_KEY_MAX - 1u), NULL);
    text_put(e, TEXT_SOURCE, source, text_len(source, (size_t)CF_SOURCE_MAX - 1u),
             NULL);
    cf_error_place(e, CF_REASON_NONE, 0, 0);
    text_put(e, TEXT_MESSAGE, tmp, text_len(tmp, 0u), NULL);
    return CF_FALSE;
}

cf_bool cf_error_set_long(cf_error* e, cf_code code, const char* key,
                          const char* source, size_t cap, const char* fmt, ...) {
    va_list ap;
    char    tmp[CF_MESSAGE_MAX];
    char*   big = NULL;
    size_t  want;

    if (e == NULL) {
        return CF_FALSE;
    }
    tmp[0] = '\0';
    want   = 0;
    if (fmt != NULL) {
        va_start(ap, fmt);
        want = cf_vfmt(tmp, sizeof tmp, fmt, ap);
        va_end(ap);
        if (cap != 0u && want > cap) {
            want = cap;
        }
        if (want >= sizeof tmp && e->size >= ERROR_SIZE_HEAP) {
            /* The text does not fit the inline store: format it again
               into a block of its own, which the error then adopts. */
            big = text_block(want + 1u);
            if (big != NULL) {
                va_start(ap, fmt);
                (void)cf_vfmt(big, want + 1u, fmt, ap);
                va_end(ap);
            }
        }
    }
    e->code = (int)code;
    text_put(e, TEXT_KEY, key, text_len(key, cap), NULL);
    text_put(e, TEXT_SOURCE, source, text_len(source, cap), NULL);
    cf_error_place(e, CF_REASON_NONE, 0, 0);
    if (big != NULL) {
        text_put(e, TEXT_MESSAGE, big, want, big);
    } else {
        text_put(e, TEXT_MESSAGE, tmp, text_len(tmp, cap), NULL);
    }
    return CF_FALSE;
}
