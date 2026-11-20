/*
 * chain.h - what a conversion must give back, written from the table of
 * what each format can hold and from nothing else (D-109).
 *
 * pairs.c takes every document through every ordered pair of the writing
 * formats, and tests/fuzz/chain.c takes a random one through a random
 * chain; both read a document into a tree (ch_read), say what a format
 * with a given `holds` would write of it (ch_lower), and compare two
 * trees as sorted lines that do not depend on the order of keys
 * (ch_lines, ch_same). ch_hop is the check of one hop of the converter,
 * the same for both. Static functions, C89, public headers only.
 */

#ifndef CONFIGFORGE_TESTS_CODEC_CHAIN_H
#define CONFIGFORGE_TESTS_CODEC_CHAIN_H

#include "configforge/cf_serialize.h"
#include "convert_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { CH_TABLE = 1, CH_ARRAY = 2, CH_SCALAR = 3 };

typedef struct ch_node {
    struct ch_node* parent;
    struct ch_node* first;
    struct ch_node* last;
    struct ch_node* next;
    char*           name; /* NULL for the document and inside an array */
    size_t          nlen;
    cf_value        value; /* a scalar's, its text copied */
    int             kind;
} ch_node;

typedef ch_node ch_tree;

/* ------------------------------------------------------------------ *
 * The tree
 * ------------------------------------------------------------------ */

static ch_node* ch_new(cf_arena* ar, ch_node* parent, int kind, const char* name,
                       size_t n) {
    ch_node* node = (ch_node*)cf_arena_calloc(ar, sizeof *node);

    if (node == NULL) {
        return NULL;
    }
    node->kind   = kind;
    node->parent = parent;
    if (name != NULL) {
        node->name = (char*)cf_arena_alloc(ar, n + 1u);
        if (node->name == NULL) {
            return NULL;
        }
        memcpy(node->name, name, n);
        node->name[n] = '\0';
        node->nlen    = n;
    }
    if (parent != NULL) {
        if (parent->last != NULL) {
            parent->last->next = node;
        } else {
            parent->first = node;
        }
        parent->last = node;
    }
    return node;
}

/* The member of table `t` called `name`, or NULL. */
static ch_node* ch_find(const ch_node* t, const char* name, size_t n) {
    ch_node* c;

    for (c = t->first; c != NULL; c = c->next) {
        if (c->nlen == n && c->name != NULL && memcmp(c->name, name, n) == 0) {
            return c;
        }
    }
    return NULL;
}

/* `c` leaves its parent's members. */
static void ch_unlink(ch_node* c) {
    ch_node* p = c->parent;
    ch_node* at;

    if (p->first == c) {
        p->first = c->next;
        if (p->last == c) {
            p->last = NULL;
        }
        return;
    }
    for (at = p->first; at != NULL; at = at->next) {
        if (at->next == c) {
            at->next = c->next;
            if (p->last == c) {
                p->last = at;
            }
            return;
        }
    }
}

typedef struct ch_rec {
    cf_arena* ar;
    ch_node*  root;
    ch_node*  cur;
} ch_rec;

/* A member arrives, as the converter reads it (convert_core.c): a table
   that meets an earlier table of its name is that table reopened (INI's
   repeated sections); anything else replaces the earlier member, as the
   file means it. */
static ch_node* ch_add(ch_rec* r, int kind, const char* name, size_t n) {
    ch_node* parent = r->cur;
    ch_node* old;

    if (parent == NULL) {
        if (kind != CH_TABLE) {
            return NULL;
        }
        /* The document again, as the YAML reader says one at the end of
           every file: the same table reopened, as convert_core.c takes it. */
        if (r->root == NULL) {
            r->root = ch_new(r->ar, NULL, kind, NULL, 0);
        }
        return r->root;
    }
    if (parent->kind == CH_ARRAY) {
        return ch_new(r->ar, parent, kind, NULL, 0);
    }
    if (name == NULL) {
        return NULL;
    }
    old = ch_find(parent, name, n);
    if (old != NULL) {
        if (old->kind == CH_TABLE && kind == CH_TABLE) {
            return old;
        }
        ch_unlink(old);
    }
    return ch_new(r->ar, parent, kind, name, n);
}

static cf_bool ch_on_open(ch_rec* r, int kind, const char* name, size_t n) {
    ch_node* node = ch_add(r, kind, name, n);

    if (node == NULL) {
        return CF_FALSE;
    }
    r->cur = node;
    return CF_TRUE;
}

static cf_bool ch_rec_table(void* ctx, const char* name, size_t n) {
    return ch_on_open((ch_rec*)ctx, CH_TABLE, name, n);
}

static cf_bool ch_rec_array(void* ctx, const char* name, size_t n) {
    return ch_on_open((ch_rec*)ctx, CH_ARRAY, name, n);
}

static cf_bool ch_rec_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                             cf_bool textual, const cf_loc* at) {
    ch_rec*  r = (ch_rec*)ctx;
    ch_node* node;

    (void)textual;
    (void)at;
    if (r->cur == NULL) {
        return CF_FALSE;
    }
    node = ch_add(r, CH_SCALAR, name, n);
    if (node == NULL) {
        return CF_FALSE;
    }
    node->value = *v;
    if (v->type == CF_TYPE_TEXT) {
        char* copy = (char*)cf_arena_alloc(r->ar, v->u.t.n + 1u);

        if (copy == NULL) {
            return CF_FALSE;
        }
        memcpy(copy, v->u.t.p, v->u.t.n);
        copy[v->u.t.n]    = '\0';
        node->value.u.t.p = copy;
    }
    return CF_TRUE;
}

static cf_bool ch_rec_end(void* ctx) {
    ch_rec* r = (ch_rec*)ctx;

    if (r->cur == NULL) {
        return CF_FALSE;
    }
    r->cur = r->cur->parent;
    return CF_TRUE;
}

/* The room a reader asks for is lent from the arena. */
static char* ch_rec_text(void* ctx, size_t n) {
    return (char*)cf_arena_alloc(((ch_rec*)ctx)->ar, n);
}

static void* ch_rec_work(void* ctx, size_t n) {
    return cf_arena_alloc(((ch_rec*)ctx)->ar, n);
}

/* The codec's events into a tree; NULL, with `err` set, when it refuses. */
static ch_tree* ch_read(cf_arena* ar, const cf_blob* in, const cf_codec* c,
                        cf_error* err) {
    ch_rec      r;
    cf_doc_sink sink;

    memset(&r, 0, sizeof r);
    memset(&sink, 0, sizeof sink);
    r.ar              = ar;
    sink.size         = sizeof sink;
    sink.begin_table  = ch_rec_table;
    sink.begin_array  = ch_rec_array;
    sink.scalar       = ch_rec_scalar;
    sink.end          = ch_rec_end;
    sink.ctx          = &r;
    sink.reserve_text = ch_rec_text;
    sink.reserve_work = ch_rec_work;
    if (!cf_codec_decode(c, in, &sink, err)) {
        return NULL;
    }
    return r.root;
}

/* ------------------------------------------------------------------ *
 * The lines
 * ------------------------------------------------------------------ */

typedef struct ch_list {
    cf_arena*    ar;
    const char** v;
    size_t       n;
    size_t       cap;
    cf_bool      bad;
} ch_list;

/* `p` escaped into `out` (NULL: only measured); `name` escapes the
   bytes a path is made of too. Returns the length. */
static size_t ch_esc(char* out, const char* p, size_t n, cf_bool name) {
    static const char hex[] = "0123456789abcdef";
    size_t            i;
    size_t            at = 0;

    for (i = 0; i < n; ++i) {
        unsigned char ch  = (unsigned char)p[i];
        const char*   two = NULL;
        cf_bool       x   = CF_FALSE;

        if (name) {
            x = (ch == '\\' || ch == '.' || ch == '[' || ch == ']' || ch < 0x20u ||
                 ch == 0x7fu)
                    ? CF_TRUE
                    : CF_FALSE;
        } else if (ch == '\n') {
            two = "\\n";
        } else if (ch == '\r') {
            two = "\\r";
        } else if (ch == '\t') {
            two = "\\t";
        } else if (ch == '\\') {
            two = "\\\\";
        } else {
            x = (ch < 0x20u || ch == 0x7fu) ? CF_TRUE : CF_FALSE;
        }
        if (two != NULL) {
            if (out != NULL) {
                out[at]      = two[0];
                out[at + 1u] = two[1];
            }
            at += 2u;
        } else if (x) {
            if (out != NULL) {
                out[at]      = '\\';
                out[at + 1u] = 'x';
                out[at + 2u] = hex[ch >> 4];
                out[at + 3u] = hex[ch & 15u];
            }
            at += 4u;
        } else {
            if (out != NULL) {
                out[at] = (char)ch;
            }
            ++at;
        }
    }
    return at;
}

static void ch_push(ch_list* l, const char* line) {
    if (l->bad) {
        return;
    }
    if (l->n == l->cap) {
        size_t       cap = (l->cap == 0) ? 64u : l->cap * 2u;
        const char** v   = (const char**)cf_arena_alloc(l->ar, cap * sizeof *v);

        if (v == NULL) {
            l->bad = CF_TRUE;
            return;
        }
        if (l->n > 0) {
            memcpy(v, l->v, l->n * sizeof *v);
        }
        l->v   = v;
        l->cap = cap;
    }
    l->v[l->n++] = line;
}

/* `<path>\t<rest>` as one string in the arena. */
static const char* ch_join(ch_list* l, const char* path, size_t plen, const char* a,
                           const char* b, const char* c, size_t clen) {
    size_t alen = strlen(a);
    size_t blen = strlen(b);
    char*  s    = (char*)cf_arena_alloc(l->ar, plen + alen + blen + clen + 1u);

    if (s == NULL) {
        l->bad = CF_TRUE;
        return NULL;
    }
    memcpy(s, path, plen);
    memcpy(s + plen, a, alen);
    memcpy(s + plen + alen, b, blen);
    memcpy(s + plen + alen + blen, c, clen);
    s[plen + alen + blen + clen] = '\0';
    return s;
}

static void ch_walk(ch_list* l, const ch_node* n, const char* path, size_t plen) {
    const ch_node* c;
    size_t         index = 0;

    if (n->kind == CH_SCALAR) {
        char        num[CF_VALUE_TEXT_MAX + 1];
        const char* text;
        size_t      tlen;
        size_t      elen;
        char*       esc;
        const char* line;

        if (n->value.type == CF_TYPE_TEXT) {
            text = n->value.u.t.p;
            tlen = n->value.u.t.n;
        } else {
            tlen = cf_value_to_text(&n->value, num, sizeof num);
            text = num;
        }
        elen = ch_esc(NULL, text, tlen, CF_FALSE);
        esc  = (char*)cf_arena_alloc(l->ar, elen + 1u);
        if (esc == NULL) {
            l->bad = CF_TRUE;
            return;
        }
        (void)ch_esc(esc, text, tlen, CF_FALSE);
        esc[elen] = '\0';
        line      = ch_join(l, path, plen, "\t", cf_type_name(n->value.type), "\t", 1u);
        if (line != NULL) {
            /* the text goes after the second tab */
            const char* full = ch_join(l, line, strlen(line), esc, "", "", 0u);

            if (full != NULL) {
                ch_push(l, full);
            }
        }
        return;
    }
    if (n->first == NULL) {
        const char* line = ch_join(l, path, plen, "\t", (n->kind == CH_TABLE) ? "table" : "array",
                                   "", 0u);

        if (line != NULL) {
            ch_push(l, line);
        }
        return;
    }
    for (c = n->first; c != NULL; c = c->next) {
        char*  sub;
        size_t add;
        size_t len;

        if (n->kind == CH_ARRAY) {
            char num[32];

            sprintf(num, "%lu", (unsigned long)index++);
            add = strlen(num) + 2u;
            sub = (char*)cf_arena_alloc(l->ar, plen + add + 1u);
            if (sub == NULL) {
                l->bad = CF_TRUE;
                return;
            }
            memcpy(sub, path, plen);
            sub[plen] = '[';
            memcpy(sub + plen + 1u, num, add - 2u);
            sub[plen + add - 1u] = ']';
            len                  = plen + add;
        } else {
            add = ch_esc(NULL, c->name, c->nlen, CF_TRUE) + 1u;
            sub = (char*)cf_arena_alloc(l->ar, plen + add + 1u);
            if (sub == NULL) {
                l->bad = CF_TRUE;
                return;
            }
            memcpy(sub, path, plen);
            sub[plen] = '.';
            (void)ch_esc(sub + plen + 1u, c->name, c->nlen, CF_TRUE);
            len = plen + add;
        }
        sub[len] = '\0';
        ch_walk(l, c, sub, len);
    }
}

static int ch_cmp(const void* a, const void* b) {
    return strcmp(*(const char* const*)a, *(const char* const*)b);
}

/* One line for each scalar, `<path>\t<type>\t<text>`, and one
   `<path>\ttable` or `<path>\tarray` for each container with nothing in
   it (the path empty for the document), sorted. NULL when out of
   memory. */
static const char* const* ch_lines(cf_arena* ar, const ch_tree* t, size_t* count) {
    ch_list l;

    memset(&l, 0, sizeof l);
    l.ar = ar;
    ch_walk(&l, t, "", 0u);
    if (l.bad) {
        return NULL;
    }
    if (l.n > 1) {
        qsort((void*)l.v, l.n, sizeof *l.v, ch_cmp);
    }
    *count = l.n;
    return l.v;
}

/* The first line two sorted lists differ at: its index, or the length
   of the shorter when one is the other's start; `n` when they are the
   same. */
static size_t ch_first_diff(const char* const* a, size_t an, const char* const* b,
                            size_t bn) {
    size_t i;

    for (i = 0; i < an && i < bn; ++i) {
        if (strcmp(a[i], b[i]) != 0) {
            return i;
        }
    }
    return (an == bn) ? an : (an < bn ? an : bn);
}

/* Two trees say the same: equal lines. */
static cf_bool ch_same(cf_arena* ar, const ch_tree* a, const ch_tree* b) {
    size_t              an = 0;
    size_t              bn = 0;
    const char* const*  al = ch_lines(ar, a, &an);
    const char* const*  bl = ch_lines(ar, b, &bn);

    if (al == NULL || bl == NULL) {
        return CF_FALSE;
    }
    return (an == bn && ch_first_diff(al, an, bl, bn) == an) ? CF_TRUE : CF_FALSE;
}

/* ------------------------------------------------------------------ *
 * What the table says a format writes
 * ------------------------------------------------------------------ */

static cf_bool ch_plain_name(const char* p, size_t n) {
    size_t i;

    for (i = 0; i < n; ++i) {
        char ch = p[i];

        if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
              (ch >= '0' && ch <= '9') || ch == '_' || ch == '-')) {
            return CF_FALSE;
        }
    }
    return n > 0;
}

/* Rule 3: a scalar, copied under `dst` as the table says, or dropped. False
   when out of memory. */
static cf_bool ch_lower_scalar(cf_arena* ar, const ch_node* s, ch_node* dst,
                               unsigned holds, size_t* lowered) {
    cf_value v = s->value;
    char     num[CF_VALUE_TEXT_MAX + 1];
    ch_node* n;
    cf_bool  as_text = CF_FALSE;

    switch (v.type) {
    case CF_TYPE_INT:
        as_text = ((holds & CF_HOLDS_INT) == 0u) ? CF_TRUE : CF_FALSE;
        break;
    case CF_TYPE_REAL:
        as_text = ((holds & CF_HOLDS_REAL) == 0u) ? CF_TRUE : CF_FALSE;
        break;
    case CF_TYPE_BOOL:
        as_text = ((holds & CF_HOLDS_BOOL) == 0u) ? CF_TRUE : CF_FALSE;
        break;
    case CF_TYPE_NULL:
        if ((holds & CF_HOLDS_NULL) == 0u) {
            if ((holds & CF_REFUSES_NULL) != 0u) {
                return CF_TRUE; /* dropped */
            }
            as_text = CF_TRUE;
        }
        break;
    default:
        if (memchr(v.u.t.p, '\n', v.u.t.n) != NULL && (holds & CF_HOLDS_LINE_BREAK) == 0u) {
            return CF_TRUE;
        }
        if (memchr(v.u.t.p, '\0', v.u.t.n) != NULL && (holds & CF_HOLDS_NUL) == 0u) {
            return CF_TRUE;
        }
        break;
    }
    n = ch_new(ar, dst, CH_SCALAR, s->name, s->nlen);
    if (n == NULL) {
        return CF_FALSE;
    }
    if (as_text) {
        size_t len  = cf_value_to_text(&v, num, sizeof num);
        char*  copy = (char*)cf_arena_alloc(ar, len + 1u);

        if (copy == NULL) {
            return CF_FALSE;
        }
        memcpy(copy, num, len);
        copy[len] = '\0';
        cf_value_text(&n->value, copy, len);
        ++*lowered;
    } else {
        n->value = v;
    }
    return CF_TRUE;
}

/* Whether an array the table says is written: rule 4. */
static cf_bool ch_array_kept(const ch_node* a, unsigned holds) {
    const ch_node* m;

    if ((holds & CF_HOLDS_ARRAYS) == 0u) {
        return CF_FALSE;
    }
    if (a->first == NULL) {
        return ((holds & CF_HOLDS_EMPTY_ARRAY) != 0u) ? CF_TRUE : CF_FALSE;
    }
    for (m = a->first; m != NULL; m = m->next) {
        if (m->kind == CH_TABLE && (holds & CF_HOLDS_TABLE_ARRAYS) == 0u) {
            return CF_FALSE;
        }
        if (m->kind == CH_ARRAY && (holds & CF_HOLDS_NESTED_ARRAYS) == 0u) {
            return CF_FALSE;
        }
    }
    return CF_TRUE;
}

/* The members of `src` that the table says `holds` writes, under `dst`.
   `plen` is the length of the joined path of `src` when it is a table
   written with a header (0 for the document); `inline_` is set inside
   an array, where no path is written. False when out of memory. */
static cf_bool ch_lower_into(cf_arena* ar, const ch_node* src, ch_node* dst,
                             unsigned holds, size_t plen, cf_bool inline_,
                             size_t* lowered) {
    const ch_node* c;
    cf_bool        in_table = (src->kind == CH_TABLE) ? CF_TRUE : CF_FALSE;

    for (c = src->first; c != NULL; c = c->next) {
        ch_node* made;

        if (in_table) {
            if (c->nlen == 0) {
                if ((holds & CF_HOLDS_EMPTY_NAME) == 0u) {
                    continue;
                }
            } else if (!ch_plain_name(c->name, c->nlen) &&
                       (holds & CF_HOLDS_ANY_NAME) == 0u) {
                continue;
            }
        }
        if (c->kind == CH_SCALAR) {
            if (!ch_lower_scalar(ar, c, dst, holds, lowered)) {
                return CF_FALSE;
            }
        } else if (c->kind == CH_ARRAY) {
            if (!ch_array_kept(c, holds)) {
                continue;
            }
            made = ch_new(ar, dst, CH_ARRAY, c->name, c->nlen);
            if (made == NULL ||
                !ch_lower_into(ar, c, made, holds, 0u, CF_TRUE, lowered)) {
                return CF_FALSE;
            }
        } else {
            size_t sub = plen;

            if (in_table && !inline_) {
                if (plen + c->nlen > (size_t)CF_KEY_MAX - 2u &&
                    (holds & CF_HOLDS_LONG_PATHS) == 0u) {
                    continue;
                }
                sub = (plen == 0) ? c->nlen : plen + 1u + c->nlen;
            }
            made = ch_new(ar, dst, CH_TABLE, c->name, c->nlen);
            if (made == NULL ||
                !ch_lower_into(ar, c, made, holds, sub, inline_, lowered)) {
                return CF_FALSE;
            }
            if (in_table && made->first == NULL && (holds & CF_HOLDS_EMPTY_TABLE) == 0u) {
                ch_unlink(made);
            }
        }
    }
    return CF_TRUE;
}

/* What the table says a format with `holds` writes of a tree, as a new
   tree, written from the bits and from nothing else (D-109). *lowered
   is how many values became their text. */
static ch_tree* ch_lower(cf_arena* ar, const ch_tree* t, unsigned holds,
                         size_t* lowered) {
    ch_node* root = ch_new(ar, NULL, CH_TABLE, NULL, 0);

    *lowered = 0;
    if (root == NULL || !ch_lower_into(ar, t, root, holds, 0u, CF_FALSE, lowered)) {
        return NULL;
    }
    return root;
}

/* ------------------------------------------------------------------ *
 * One hop
 * ------------------------------------------------------------------ */

enum { CH_OK = 0, CH_FAILED = 1, CH_BOUND = 2 };

typedef struct ch_hop {
    cf_blob out;      /* what the target wrote, in the arena */
    size_t  nfindings;
    size_t  as_text;
    char    why[400]; /* when CH_FAILED */
} ch_hop;

/* `text` shortened to fit `cap` and made of printable bytes. */
static void ch_show(char* out, size_t cap, const char* text) {
    size_t i = 0;

    for (; text[i] != '\0' && i + 4u < cap; ++i) {
        unsigned char ch = (unsigned char)text[i];

        out[i] = (ch < 0x20u) ? '?' : (char)ch;
    }
    if (text[i] != '\0') {
        out[i++] = '.';
        out[i++] = '.';
        out[i++] = '.';
    }
    out[i] = '\0';
}

/* The first line two trees differ at, in words, into `why`. */
static void ch_why_diff(cf_arena* ar, char* why, size_t cap, const char* what,
                        const ch_tree* a, const ch_tree* b) {
    size_t             an = 0;
    size_t             bn = 0;
    const char* const* al = ch_lines(ar, a, &an);
    const char* const* bl = ch_lines(ar, b, &bn);
    char               x[110];
    char               y[110];
    size_t             at;

    if (al == NULL || bl == NULL) {
        sprintf(why, "%s: out of memory", what);
        return;
    }
    at = ch_first_diff(al, an, bl, bn);
    ch_show(x, sizeof x, (at < an) ? al[at] : "(nothing)");
    ch_show(y, sizeof y, (at < bn) ? bl[at] : "(nothing)");
    (void)cap;
    sprintf(why, "%s: first '%s' against '%s'", what, x, y);
}

/* `in`, which `src` reads, written through `to`, and the result held to
   the table (`holds` of `to`): what `to`'s reader gives back is what
   the table says `to` writes of what `src` gave, and nothing is lost
   quietly. CH_OK, or CH_FAILED with `why`, or CH_BOUND when the
   converter refuses the document by D-82's bound (or has no memory). */
static int ch_hop_run(cf_arena* ar, const cf_blob* in, const cf_codec* src,
                      const cf_codec* to, unsigned holds, ch_hop* h) {
    cv_result res;
    cf_error  err = CF_ERROR_INIT;
    ch_tree*  m;
    ch_tree*  r;
    ch_tree*  want;
    size_t    lowered = 0;
    size_t    i;
    int       rc;

    memset(h, 0, sizeof *h);
    rc = cv_convert(ar, in, src, to, &res, &err);
    if (rc == 2) {
        return CH_BOUND;
    }
    if (rc != 0) {
        sprintf(h->why, "the converter refused: %.200s", err.message);
        return CH_FAILED;
    }
    h->out       = res.out;
    h->nfindings = res.nfindings;
    h->as_text   = res.as_text;
    for (i = 0; i < res.nfindings; ++i) {
        if (res.findings[i].kind == CV_OTHER) {
            sprintf(h->why, "a refusal the table did not predict, at '%.200s'",
                    res.findings[i].path);
            return CH_FAILED;
        }
    }
    m = ch_read(ar, in, src, &err);
    if (m == NULL) {
        sprintf(h->why, "the source is not read again: %.200s", err.message);
        return CH_FAILED;
    }
    r = ch_read(ar, &res.out, to, &err);
    if (r == NULL) {
        sprintf(h->why, "%s's reader refuses what its writer wrote: %.200s", to->name,
                err.message);
        return CH_FAILED;
    }
    want = ch_lower(ar, m, holds, &lowered);
    if (want == NULL) {
        sprintf(h->why, "out of memory");
        return CH_FAILED;
    }
    if (res.as_text != lowered) {
        sprintf(h->why, "the converter counted %lu value(s) written as text, the table says %lu",
                (unsigned long)res.as_text, (unsigned long)lowered);
        return CH_FAILED;
    }
    if (!ch_same(ar, r, want)) {
        ch_why_diff(ar, h->why, sizeof h->why, "read back differs from the table", r,
                    want);
        return CH_FAILED;
    }
    if (res.nfindings + res.as_text == 0 && !ch_same(ar, m, r)) {
        ch_why_diff(ar, h->why, sizeof h->why, "a quiet loss", m, r);
        return CH_FAILED;
    }
    return CH_OK;
}

#endif /* CONFIGFORGE_TESTS_CODEC_CHAIN_H */
