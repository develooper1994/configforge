/* Compare nested mappings of scalars and scalar arrays by their typed
   reader events. */
#include "configforge/cf_serialize.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIFF_FLOOR ((size_t)1048576)
#define DIFF_FACTOR 64u
#define DIFF_SEGMENTS 64u

typedef struct seg {
    const char* p;
    size_t      n;
} seg;

typedef struct array_item {
    struct array_item* next;
    cf_value           value;
} array_item;

typedef struct entry {
    struct entry* next;
    const seg*    path;
    size_t        depth;
    size_t        ordinal;
    cf_bool       is_array;
    union {
        cf_value    scalar;
        array_item* items;
    } value;
} entry;

typedef struct side {
    cf_arena*    arena;
    size_t       remaining;
    size_t       count;
    entry*       first;
    entry*       last;
    entry**      ordered;
    size_t       tables;
    cf_bool      seen;
    seg*         stack;
    array_item** array_next;
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
    if (s->array_next != NULL) {
        return CF_FALSE;
    }
    if (name == NULL) {
        if (s->tables != 0) {
            return CF_FALSE;
        }
        s->tables = 1;
        s->seen   = CF_TRUE;
        return CF_TRUE;
    }
    if (s->tables == 0 || s->tables >= DIFF_SEGMENTS) {
        return CF_FALSE;
    }
    s->stack[s->tables - 1].p = copy_span(s, name, n);
    if (s->stack[s->tables - 1].p == NULL) {
        return CF_FALSE;
    }
    s->stack[s->tables - 1].n = n;
    ++s->tables;
    return CF_TRUE;
}

static entry* record_entry(side* s, const char* name, size_t n) {
    entry* e;
    seg*   path;
    if (s->tables == 0 || name == NULL) {
        return NULL;
    }
    e = (entry*)reserve_work(s, sizeof *e);
    if (e == NULL) {
        return NULL;
    }
    path = (seg*)reserve_work(s, s->tables * sizeof *path);
    if (path == NULL) {
        return NULL;
    }
    memcpy(path, s->stack, (s->tables - 1) * sizeof *path);
    path[s->tables - 1].p = copy_span(s, name, n);
    if (path[s->tables - 1].p == NULL) {
        return NULL;
    }
    path[s->tables - 1].n = n;
    e->path               = path;
    e->depth              = s->tables;
    e->ordinal            = s->count++;
    e->next               = NULL;
    if (s->last != NULL) {
        s->last->next = e;
    } else {
        s->first = e;
    }
    s->last = e;
    return e;
}

static cf_bool begin_array(void* ctx, const char* name, size_t n) {
    side*  s = (side*)ctx;
    entry* e;
    if (s->array_next != NULL) {
        return CF_FALSE;
    }
    e = record_entry(s, name, n);
    if (e == NULL) {
        return CF_FALSE;
    }
    e->is_array    = CF_TRUE;
    e->value.items = NULL;
    s->array_next  = &e->value.items;
    return CF_TRUE;
}

static cf_bool end_container(void* ctx) {
    side* s = (side*)ctx;
    if (s->array_next != NULL) {
        s->array_next = NULL;
        return CF_TRUE;
    }
    if (s->tables == 0) {
        return CF_FALSE;
    }
    --s->tables;
    return CF_TRUE;
}

static cf_bool copy_value(side* s, cf_value* dest, const cf_value* src) {
    *dest = *src;
    if (src->type == CF_TYPE_TEXT) {
        dest->u.t.p = copy_span(s, src->u.t.p, src->u.t.n);
        return dest->u.t.p != NULL;
    }
    return CF_TRUE;
}

static cf_bool scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                      cf_bool textual, const cf_loc* at) {
    side*  s = (side*)ctx;
    entry* e;
    (void)textual;
    (void)at;
    if (s->array_next != NULL) {
        array_item* item;
        if (name != NULL) {
            return CF_FALSE;
        }
        item = (array_item*)reserve_work(s, sizeof *item);
        if (item == NULL || !copy_value(s, &item->value, v)) {
            return CF_FALSE;
        }
        item->next     = NULL;
        *s->array_next = item;
        s->array_next  = &item->next;
        return CF_TRUE;
    }
    e = record_entry(s, name, n);
    if (e == NULL) {
        return CF_FALSE;
    }
    e->is_array = CF_FALSE;
    return copy_value(s, &e->value.scalar, v);
}

static int key_order(const entry* a, const entry* b) {
    size_t depth = a->depth < b->depth ? a->depth : b->depth;
    size_t i;
    for (i = 0; i < depth; ++i) {
        const seg* x   = &a->path[i];
        const seg* y   = &b->path[i];
        size_t     n   = x->n < y->n ? x->n : y->n;
        int        cmp = memcmp(x->p, y->p, n);
        if (cmp != 0) {
            return cmp;
        }
        if (x->n != y->n) {
            return x->n > y->n ? 1 : -1;
        }
    }
    return (a->depth > b->depth) - (a->depth < b->depth);
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
    s->stack = s->arena == NULL
                   ? NULL
                   : (seg*)cf_arena_alloc(s->arena, DIFF_SEGMENTS * sizeof *s->stack);
    if (s->stack == NULL || !cf_blob_file(&blob, s->arena, path, &err)) {
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
    sink.end          = end_container;
    sink.reserve_work = reserve_work;
    sink.reserve_text = reserve_text;
    if (!cf_codec_decode(codec, &blob, &sink, &err) || !s->seen || s->tables != 0 ||
        s->array_next != NULL) {
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

static cf_bool write_segment(const seg* g) {
    static const char hex[] = "0123456789abcdef";
    size_t            i;
    if (fputs("[\"", stdout) == EOF) {
        return CF_FALSE;
    }
    for (i = 0; i < g->n; ++i) {
        unsigned char c = (unsigned char)g->p[i];
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
    return fputs("\"]", stdout) == EOF ? CF_FALSE : CF_TRUE;
}

static cf_bool write_change(const char* kind, const entry* e) {
    size_t i;
    if (e == NULL || fputs(kind, stdout) == EOF || fputs("\t$", stdout) == EOF) {
        return CF_FALSE;
    }
    for (i = 0; i < e->depth; ++i) {
        if (!write_segment(&e->path[i])) {
            return CF_FALSE;
        }
    }
    return putchar('\n') == EOF ? CF_FALSE : CF_TRUE;
}

static cf_bool entry_equal(const entry* a, const entry* b) {
    const array_item* x;
    const array_item* y;
    if (a->is_array != b->is_array) {
        return CF_FALSE;
    }
    if (!a->is_array) {
        return cf_value_equal(&a->value.scalar, &b->value.scalar);
    }
    x = a->value.items;
    y = b->value.items;
    while (x != NULL && y != NULL) {
        if (!cf_value_equal(&x->value, &y->value)) {
            return CF_FALSE;
        }
        x = x->next;
        y = y->next;
    }
    return x == NULL && y == NULL;
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
        } else if (!entry_equal(a, b)) {
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
