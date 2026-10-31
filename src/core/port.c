/*
 * port.c - ConfigForge portability layer implementation.
 *
 * C89. No <ctype.h>, no <locale.h>, no snprintf, no strtod.
 */

#include "configforge/cf_port.h"
#include "configforge/cf_numeric.h"

#include <stdarg.h>
#include <string.h>

/* ------------------------------------------------------------------ *
 * Atomic reference counting
 * ------------------------------------------------------------------ */

#if defined(CF_ATOMIC_WIN32)
#include <windows.h>
#endif
#if defined(CF_ATOMIC_C11)
#include <stdatomic.h>
#endif

void cf_ref_init(cf_refcount* rc, cf_i32 n) {
    rc->v = n;
}

cf_i32 cf_ref_inc(cf_refcount* rc) {
#if defined(CF_ATOMIC_GCC_ATOMIC)
    return __atomic_add_fetch(&rc->v, 1, __ATOMIC_ACQ_REL);
#elif defined(CF_ATOMIC_GCC_SYNC)
    return __sync_add_and_fetch(&rc->v, 1);
#elif defined(CF_ATOMIC_WIN32)
    return (cf_i32)InterlockedIncrement((LONG volatile*)&rc->v);
#elif defined(CF_ATOMIC_C11)
    return atomic_fetch_add_explicit((_Atomic cf_i32*)&rc->v, 1, memory_order_acq_rel) +
           1;
#else
    rc->v += 1;
    return rc->v;
#endif
}

cf_i32 cf_ref_dec(cf_refcount* rc) {
#if defined(CF_ATOMIC_GCC_ATOMIC)
    return __atomic_sub_fetch(&rc->v, 1, __ATOMIC_ACQ_REL);
#elif defined(CF_ATOMIC_GCC_SYNC)
    return __sync_sub_and_fetch(&rc->v, 1);
#elif defined(CF_ATOMIC_WIN32)
    return (cf_i32)InterlockedDecrement((LONG volatile*)&rc->v);
#elif defined(CF_ATOMIC_C11)
    return atomic_fetch_sub_explicit((_Atomic cf_i32*)&rc->v, 1, memory_order_acq_rel) -
           1;
#else
    rc->v -= 1;
    return rc->v;
#endif
}

/* A read the compiler cannot keep in a register or move: a loop that
   waits on it (cf_live's lock) must see another thread's write. */
cf_i32 cf_ref_get(const cf_refcount* rc) {
#if defined(CF_ATOMIC_GCC_ATOMIC)
    return __atomic_load_n(&rc->v, __ATOMIC_ACQUIRE);
#elif defined(CF_ATOMIC_GCC_SYNC)
    /* __sync has no load; a volatile read between two full barriers is
       one, without casting the const away. */
    cf_i32 v;
    __sync_synchronize();
    v = *(const volatile cf_i32*)&rc->v;
    __sync_synchronize();
    return v;
#elif defined(CF_ATOMIC_WIN32)
    return (cf_i32)InterlockedCompareExchange((LONG volatile*)&rc->v, 0, 0);
#elif defined(CF_ATOMIC_C11)
    return atomic_load_explicit((const _Atomic cf_i32*)&rc->v, memory_order_acquire);
#else
    return rc->v;
#endif
}

/* ------------------------------------------------------------------ *
 * Byte-wise comparison
 * ------------------------------------------------------------------ */

int cf_cmp_bytes(const char* a, size_t alen, const char* b, size_t blen) {
    size_t n = (alen < blen) ? alen : blen;
    size_t i;

    for (i = 0; i < n; ++i) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        if (ca != cb) {
            return (ca < cb) ? -1 : 1;
        }
    }
    if (alen == blen) {
        return 0;
    }
    return (alen < blen) ? -1 : 1;
}

int cf_cmp_ascii_ci(const char* a, size_t alen, const char* b, size_t blen) {
    size_t n = (alen < blen) ? alen : blen;
    size_t i;

    for (i = 0; i < n; ++i) {
        int ca = cf_to_lower((unsigned char)a[i]);
        int cb = cf_to_lower((unsigned char)b[i]);
        if (ca != cb) {
            return (ca < cb) ? -1 : 1;
        }
    }
    if (alen == blen) {
        return 0;
    }
    return (alen < blen) ? -1 : 1;
}

/* ------------------------------------------------------------------ *
 * Bounded formatting
 *
 * Used unconditionally in both portability modes so that output is
 * identical. Deliberately tiny: no width, no precision, no flags.
 * ------------------------------------------------------------------ */

typedef struct cf_fmt_out {
    char*  buf;
    size_t cap;
    size_t len; /* characters that would have been written */
} cf_fmt_out;

static void cf_put(cf_fmt_out* o, char c) {
    if (o->buf != NULL && o->cap > 0 && o->len + 1 < o->cap) {
        o->buf[o->len] = c;
    }
    o->len += 1;
}

static void cf_puts_n(cf_fmt_out* o, const char* s, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i) {
        cf_put(o, s[i]);
    }
}

static void cf_put_hex(cf_fmt_out* o, cf_u64 v) {
    static const char digits[] = "0123456789abcdef";
    char              tmp[17];
    size_t            n = 0;

    do {
        tmp[n++] = digits[(size_t)(v & (cf_u64)0xF)];
        v >>= 4;
    } while (v != 0);
    while (n > 0) {
        cf_put(o, tmp[--n]);
    }
}

static size_t cf_vformat(cf_fmt_out* o, const char* fmt, va_list ap) {
    const char* p = fmt;
    char        num[CF_I64_DIGITS];
    size_t      n;

    while (*p != '\0') {
        if (*p != '%') {
            cf_put(o, *p++);
            continue;
        }
        ++p;
        switch (*p) {
        case 's': {
            const char* s = va_arg(ap, const char*);
            if (s == NULL) {
                s = "(null)";
            }
            cf_puts_n(o, s, strlen(s));
            break;
        }
        case 'S': {
            /* A pointer and a length. printf spells this "%.*s", but
               precision is deliberately absent from this formatter, and
               a bounded string is common enough in a library that slices
               rather than copies to deserve its own conversion. */
            const char* s2 = va_arg(ap, const char*);
            size_t      n2 = va_arg(ap, size_t);
            if (s2 != NULL) {
                cf_puts_n(o, s2, n2);
            }
            break;
        }
        case 'c':
            cf_put(o, (char)va_arg(ap, int));
            break;
        case 'd':
        case 'i':
            n = cf_i64_to_text((cf_i64)va_arg(ap, int), num, sizeof num);
            cf_puts_n(o, num, n);
            break;
        case 'u':
            n = cf_u64_to_text((cf_u64)va_arg(ap, unsigned int), num, sizeof num);
            cf_puts_n(o, num, n);
            break;
        case 'z':
            /* %zu only */
            if (*(p + 1) == 'u') {
                ++p;
                n = cf_u64_to_text((cf_u64)va_arg(ap, size_t), num, sizeof num);
                cf_puts_n(o, num, n);
            } else {
                cf_put(o, '%');
                cf_put(o, 'z');
            }
            break;
        case 'I':
            n = cf_i64_to_text(va_arg(ap, cf_i64), num, sizeof num);
            cf_puts_n(o, num, n);
            break;
        case 'U':
            n = cf_u64_to_text(va_arg(ap, cf_u64), num, sizeof num);
            cf_puts_n(o, num, n);
            break;
        case 'x':
            cf_put_hex(o, (cf_u64)va_arg(ap, unsigned int));
            break;
        case '%':
            cf_put(o, '%');
            break;
        case '\0':
            cf_put(o, '%');
            continue;
        default:
            /* Unknown conversion is emitted literally rather than
               silently swallowed: a format bug should be visible. */
            cf_put(o, '%');
            cf_put(o, *p);
            break;
        }
        ++p;
    }

    if (o->buf != NULL && o->cap > 0) {
        size_t at  = (o->len < o->cap) ? o->len : o->cap - 1;
        o->buf[at] = '\0';
    }
    return o->len;
}

size_t cf_vfmt(char* buf, size_t cap, const char* fmt, va_list ap) {
    cf_fmt_out o;

    o.buf = buf;
    o.cap = cap;
    o.len = 0;
    return cf_vformat(&o, fmt, ap);
}

size_t cf_fmt(char* buf, size_t cap, const char* fmt, ...) {
    cf_fmt_out o;
    va_list    ap;
    size_t     r;

    o.buf = buf;
    o.cap = cap;
    o.len = 0;

    va_start(ap, fmt);
    r = cf_vformat(&o, fmt, ap);
    va_end(ap);
    return r;
}

size_t cf_fmt_append(char* buf, size_t cap, size_t at, const char* fmt, ...) {
    cf_fmt_out o;
    va_list    ap;
    size_t     r;

    /* Past the end, or asked to measure rather than write: keep
       counting. A caller sizes a buffer by running the whole render
       with cap 0, so bailing out early here would report a length that
       is short by everything after the first overflow. */
    if (buf == NULL || at >= cap) {
        o.buf = NULL;
        o.cap = 0;
    } else {
        o.buf = buf + at;
        o.cap = cap - at;
    }
    o.len = 0;

    va_start(ap, fmt);
    r = cf_vformat(&o, fmt, ap);
    va_end(ap);
    return at + r;
}

/* ------------------------------------------------------------------ *
 * Build description
 * ------------------------------------------------------------------ */

const char* cf_port_description(void) {
#if defined(CF_FORCE_PORTABLE)
    return "C89 baseline, forced portable, atomics: " CF_ATOMIC_KIND;
#elif CF_HAVE_C11
    return "C89 baseline, C11 detected, atomics: " CF_ATOMIC_KIND;
#elif CF_HAVE_C99
    return "C89 baseline, C99 detected, atomics: " CF_ATOMIC_KIND;
#else
    return "C89 baseline, atomics: " CF_ATOMIC_KIND;
#endif
}
