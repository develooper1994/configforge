/*
 * error.c - the error model. C89.
 */

#include "configforge/cf_error.h"
#include "configforge/cf_build_options.h"
#include "configforge/cf_port.h"

#include <stdarg.h>
#include <stddef.h>
#include <string.h>

#define ERROR_SIZE_1_7 offsetof(cf_error, where)
#define ERROR_SIZE_1_8 (offsetof(cf_error, reason) + sizeof(int))

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

void cf_error_place(cf_error* e, int reason, unsigned int line, unsigned int col) {
    if (e != NULL && e->size >= ERROR_SIZE_1_8) {
        e->where.file = NULL;
        e->where.line = line;
        e->where.col  = col;
        e->reason     = reason;
    }
}

void cf_error_clear(cf_error* e) {
    if (e == NULL) {
        return;
    }
    e->code       = CF_OK;
    e->key[0]     = '\0';
    e->source[0]  = '\0';
    e->message[0] = '\0';
    cf_error_place(e, CF_REASON_NONE, 0, 0);
}

void cf_error_copy(cf_error* dst, const cf_error* src) {
    if (dst == NULL || src == NULL || dst == src) {
        return;
    }
    /* 1.0's fields. A field added in 1.x is copied here only when
       dst->size reaches past it (offsetof + sizeof). */
    dst->code = src->code;
    memcpy(dst->key, src->key, sizeof dst->key);
    memcpy(dst->source, src->source, sizeof dst->source);
    memcpy(dst->message, src->message, sizeof dst->message);
    if (dst->size >= ERROR_SIZE_1_8) {
        if (src->size >= ERROR_SIZE_1_8) {
            cf_error_place(dst, src->reason, src->where.line, src->where.col);
        } else {
            cf_error_place(dst, CF_REASON_NONE, 0, 0);
        }
    }
}

static void copy_bounded(char* dst, size_t cap, const char* src) {
    size_t n;

    if (cap == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    n = strlen(src);
    if (n >= cap) {
        n = cap - 1;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
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
    e->code = (int)code;
    copy_bounded(e->key, sizeof e->key, key);
    copy_bounded(e->source, sizeof e->source, source);
    cf_error_place(e, CF_REASON_NONE, 0, 0);

    if (fmt == NULL) {
        e->message[0] = '\0';
        return CF_FALSE;
    }
    va_start(ap, fmt);
    cf_vfmt(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    copy_bounded(e->message, sizeof e->message, tmp);
    return CF_FALSE;
}
