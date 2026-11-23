/* Compare two flat documents through their typed reader events. */
#include "configforge/cf_serialize.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIFF_FLOOR ((size_t)1048576)
#define DIFF_FACTOR 64u

typedef struct entry {
    struct entry* next;
    const char*   key;
    size_t        length;
    size_t        ordinal;
    cf_value      value;
} entry;

typedef struct side {
    cf_arena* arena;
    size_t    remaining;
    size_t    count;
    entry*    first;
    entry*    last;
    entry**   ordered;
    cf_bool   open;
    cf_bool   seen;
} side;

static void* reserve_work(void* ctx, size_t n) {
    side* s = (side*)ctx;
    if (n == 0) {
        n = 1;
    }
    if (n > s->remaining) {
        return NULL;
    }
    s->remaining -= n;
    return cf_arena_alloc(s->arena, n);
}

static char* reserve_text(void* ctx, size_t n) {
    return (char*)reserve_work(ctx, n);
}

static char* copy_span(side* s, const char* p, size_t n) {
    char* copy = reserve_text(s, n);
    if (copy != NULL && n != 0) {
        memcpy(copy, p, n);
    }
    return copy;
}

static cf_bool begin_table(void* ctx, const char* name, size_t n) {
    side* s = (side*)ctx;
    (void)n;
    if (s->open || name != NULL) {
        return CF_FALSE;
    }
    s->open = CF_TRUE;
    s->seen = CF_TRUE;
    return CF_TRUE;
}

static cf_bool begin_array(void* ctx, const char* name, size_t n) {
    (void)ctx;
    (void)name;
    (void)n;
    return CF_FALSE;
}

static cf_bool end_table(void* ctx) {
    side* s = (side*)ctx;
    if (!s->open) {
        return CF_FALSE;
    }
    s->open = CF_FALSE;
    return CF_TRUE;
}

static cf_bool scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                      cf_bool textual, const cf_loc* at) {
    side*  s = (side*)ctx;
    entry* e;
    (void)textual;
    (void)at;
    if (!s->open || name == NULL) {
        return CF_FALSE;
    }
    e = (entry*)reserve_work(s, sizeof *e);
    if (e == NULL) {
        return CF_FALSE;
    }
    e->key = copy_span(s, name, n);
    if (e->key == NULL) {
        return CF_FALSE;
    }
    e->length = n;
    e->value  = *v;
    if (v->type == CF_TYPE_TEXT) {
        e->value.u.t.p = copy_span(s, v->u.t.p, v->u.t.n);
        if (e->value.u.t.p == NULL) {
            return CF_FALSE;
        }
    }
    e->ordinal = s->count++;
    e->next    = NULL;
    if (s->last != NULL) {
        s->last->next = e;
    } else {
        s->first = e;
    }
    s->last = e;
    return CF_TRUE;
}

static int key_order(const entry* a, const entry* b) {
    size_t n   = a->length < b->length ? a->length : b->length;
    int    cmp = memcmp(a->key, b->key, n);
    if (cmp != 0) {
        return cmp;
    }
    return (a->length > b->length) - (a->length < b->length);
}

static int entry_order(const void* pa, const void* pb) {
    const entry* a   = *(entry* const*)pa;
    const entry* b   = *(entry* const*)pb;
    int          cmp = key_order(a, b);
    if (cmp != 0) {
        return cmp;
    }
    return (b->ordinal > a->ordinal) - (b->ordinal < a->ordinal);
}

static cf_bool read_side(side* s, const char* path, const char* format) {
    cf_blob         blob;
    cf_doc_sink     sink;
    cf_error        err = CF_ERROR_INIT;
    const cf_codec* codec;
    entry*          e;

    s->arena = cf_arena_new(NULL, 0);
    if (s->arena == NULL || !cf_blob_file(&blob, s->arena, path, &err)) {
        return CF_FALSE;
    }
    codec = format != NULL ? cf_codec_by_name(format)
                           : cf_codec_detect(NULL, 0, &blob, path, NULL, NULL);
    if (codec == NULL || !cf_codec_can_read(codec)) {
        return CF_FALSE;
    }
    s->remaining = blob.n > ((size_t)-1 - DIFF_FLOOR) / DIFF_FACTOR
                       ? (size_t)-1
                       : DIFF_FLOOR + DIFF_FACTOR * blob.n;
    memset(&sink, 0, sizeof sink);
    sink.size         = sizeof sink;
    sink.ctx          = s;
    sink.begin_table  = begin_table;
    sink.begin_array  = begin_array;
    sink.scalar       = scalar;
    sink.end          = end_table;
    sink.reserve_work = reserve_work;
    sink.reserve_text = reserve_text;
    if (!cf_codec_decode(codec, &blob, &sink, &err) || !s->seen || s->open) {
        return CF_FALSE;
    }
    if (s->count == 0) {
        return CF_TRUE;
    }
    if (s->count > s->remaining / sizeof *s->ordered) {
        return CF_FALSE;
    }
    s->ordered = (entry**)reserve_work(s, s->count * sizeof *s->ordered);
    if (s->ordered == NULL) {
        return CF_FALSE;
    }
    for (e = s->first; e != NULL; e = e->next) {
        s->ordered[e->ordinal] = e;
    }
    qsort((void*)s->ordered, s->count, sizeof *s->ordered, entry_order);
    return CF_TRUE;
}

static size_t next_key(const side* s, size_t i) {
    const entry* first = s->ordered[i++];
    while (i < s->count && key_order(first, s->ordered[i]) == 0) {
        ++i;
    }
    return i;
}

static cf_bool write_change(const char* kind, const entry* e) {
    static const char hex[] = "0123456789abcdef";
    size_t            i;
    if (e == NULL) {
        return CF_FALSE;
    }
    if (fputs(kind, stdout) == EOF || fputs("\t$[\"", stdout) == EOF) {
        return CF_FALSE;
    }
    for (i = 0; i < e->length; ++i) {
        unsigned char c = (unsigned char)e->key[i];
        if (c < 32 || c > 126) {
            if (fputs("\\x", stdout) == EOF || putchar(hex[c >> 4]) == EOF ||
                putchar(hex[c & 15]) == EOF) {
                return CF_FALSE;
            }
        } else {
            if ((c == '\\' || c == '"') && putchar('\\') == EOF) {
                return CF_FALSE;
            }
            if (putchar(c) == EOF) {
                return CF_FALSE;
            }
        }
    }
    return fputs("\"]\n", stdout) == EOF ? CF_FALSE : CF_TRUE;
}

static int compare(const side* left, const side* right) {
    size_t i = 0, j = 0;
    int    rc = 0;
    while (i < left->count || j < right->count) {
        const entry* a     = i < left->count ? left->ordered[i] : NULL;
        const entry* b     = j < right->count ? right->ordered[j] : NULL;
        int          order = a == NULL ? 1 : (b == NULL ? -1 : key_order(a, b));
        const char*  kind  = NULL;
        if (order < 0) {
            kind = "removed";
        } else if (order > 0) {
            kind = "added";
        } else if (!cf_value_equal(&a->value, &b->value)) {
            kind = "changed";
        }
        if (kind != NULL) {
            if (!write_change(kind, order > 0 ? b : a)) {
                return 2;
            }
            rc = 1;
        }
        if (order <= 0) {
            i = next_key(left, i);
        }
        if (order >= 0) {
            j = next_key(right, j);
        }
    }
    return fflush(stdout) == EOF ? 2 : rc;
}

static void usage(FILE* out) {
    (void)fputs("usage: configforge diff [--as-left FORMAT] [--as-right FORMAT] "
                "[--] LEFT RIGHT\n",
                out);
}

int main(int argc, char** argv) {
    const char* formats[2] = {NULL, NULL};
    side        left, right;
    int         i = 1, ended = 0, rc = 2;
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        usage(stdout);
        if (fflush(stdout) == EOF || ferror(stdout)) {
            (void)fputs("diff: cannot write output\n", stderr);
            return 2;
        }
        return 0;
    }
    while (i < argc && argv[i][0] == '-') {
        int index;
        if (strcmp(argv[i], "--") == 0) {
            ++i;
            ended = 1;
            break;
        }
        if (strcmp(argv[i], "--as-left") == 0) {
            index = 0;
        } else if (strcmp(argv[i], "--as-right") == 0) {
            index = 1;
        } else {
            usage(stderr);
            return 2;
        }
        ++i;
        if (formats[index] != NULL || i == argc || argv[i][0] == '-') {
            usage(stderr);
            return 2;
        }
        formats[index] = argv[i++];
    }
    if (argc - i != 2 || (!ended && argv[i + 1][0] == '-')) {
        usage(stderr);
        return 2;
    }
    memset(&left, 0, sizeof left);
    memset(&right, 0, sizeof right);
    if (!read_side(&left, argv[i], formats[0])) {
        (void)fputs("diff: left input refused\n", stderr);
    } else if (!read_side(&right, argv[i + 1], formats[1])) {
        (void)fputs("diff: right input refused\n", stderr);
    } else {
        rc = compare(&left, &right);
        if (rc == 2) {
            (void)fputs("diff: cannot write output\n", stderr);
        }
    }
    cf_arena_free(right.arena);
    cf_arena_free(left.arena);
    return rc;
}
