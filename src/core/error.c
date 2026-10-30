/*
 * error.c - the error model. C89.
 */

#include "configforge/cf_error.h"
#include "configforge/cf_log.h"

#include <stdarg.h>
#include <string.h>

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

void cf_error_clear(cf_error* e) {
    if (e == NULL) {
        return;
    }
    e->code       = CF_OK;
    e->key[0]     = '\0';
    e->source[0]  = '\0';
    e->message[0] = '\0';
}

const char* cf_log_level_name(cf_log_level level) {
    switch (level) {
    case CF_LOG_DEBUG:
        return "debug";
    case CF_LOG_INFO:
        return "info";
    case CF_LOG_WARN:
        return "warn";
    case CF_LOG_ERROR:
        return "error";
    }
    return "?";
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
