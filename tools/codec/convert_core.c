/*
 * convert_core.c - the document into the target's codec, with what the
 * target cannot hold named: the tree, the passes and the replay of
 * `configforge codec convert` and `configforge codec format` (D-94),
 * behind one function, cv_convert, so a test runs the converter without
 * a process (D-109).
 *
 * The decoder's events are recorded in a tree, and the tree is walked
 * into the target's encoder in the order the target needs: a table's
 * values, then its arrays, then its tables. Events straight from the
 * decoder to the encoder would put a JSON document's later root key
 * inside an INI section, and a TOML array after a sub-table inside that
 * sub-table (the two faults lib/config/src/bridge/flatten.c's comment above
 * all_digits records), and the key space that orders them is not in the
 * serialization library, which this program links alone.
 *
 * What the target cannot hold is found before anything is written, in
 * passes over the tree, and returned as findings; nothing is dropped
 * quietly. C89.
 */

#include "convert_core.h"

#include <stdio.h>
#include <string.h>

/* D-82's rule made the tool's (the library's CF_DECODE_* are in
   cf_flatten.h, which this program does not see): each node costs its
   name's length, its text's length and CV_NODE_COST bytes, and a
   document may cost CV_FACTOR times its size and CV_FLOOR bytes more. */
#define CV_FACTOR 64u
#define CV_FLOOR 1048576u
#define CV_NODE_COST 64u

/* The member table starts with this many slots, a power of two. */
#define CV_SLOTS 64u

typedef enum cv_nkind { CVN_TABLE, CVN_ARRAY, CVN_VALUE } cv_nkind;

typedef struct cv_node {
    struct cv_node* parent;
    struct cv_node* first;
    struct cv_node* last;
    struct cv_node* next;
    char*           name; /* NULL for the document and inside an array */
    size_t          nlen;
    cf_value        value;
    unsigned long   id;    /* creation order: what the member table is keyed by */
    unsigned long   hash;  /* of the parent's id and the name */
    size_t          pos;   /* inside an array: its index */
    size_t          count; /* members so far */
    cv_nkind        kind;
    cf_bool         textual;
    cf_bool         left_out;
} cv_node;

/* The recording sink's state, all in one arena. */
typedef struct cv_tree {
    cf_arena* ar;
    cv_node*  root;
    cv_node*  cur;
    cv_node** slots; /* members of tables, open addressing */
    size_t    cap;
    size_t    used;
    size_t    serial;
    size_t    spent;
    size_t    budget;
    cf_bool   over;
    cf_bool   nomem;
} cv_tree;

/* What the passes found, in the arena. */
typedef struct cv_check {
    cf_arena*       ar;
    const cf_codec* target;
    cv_finding*     list;
    size_t          n;
    size_t          cap;
    unsigned        holds;     /* of the target, when `known` */
    size_t          max_path;  /* and its reader's limits */
    size_t          max_depth;
    cf_bool         known; /* the table of what a format holds has the target */
    cf_bool         nomem;
} cv_check;

/* One walk into an encoder. With `check` set it is the last pass: a
   node the encoder refuses is a finding and is left out; without, a
   refusal is a bug. */
typedef struct cv_out {
    cf_doc_sink* sink;
    cv_check*    check;
    size_t       non_text;
    cf_bool      failed;
    cf_loc       none;
} cv_out;

/* ------------------------------------------------------------------ *
 * The tree
 * ------------------------------------------------------------------ */

static unsigned long cv_fnv(unsigned long h, const unsigned char* p, size_t n) {
    size_t i;

    for (i = 0; i < n; ++i) {
        h ^= p[i];
        h = (h * 16777619UL) & 0xFFFFFFFFUL;
    }
    return h;
}

static unsigned long cv_hash(unsigned long parent, const char* name, size_t n) {
    unsigned char id[4];
    unsigned      k;

    for (k = 0; k < 4u; ++k) {
        id[k] = (unsigned char)((parent >> (8u * k)) & 0xFFu);
    }
    return cv_fnv(cv_fnv(2166136261UL, id, sizeof id), (const unsigned char*)name, n);
}

/* The slot a member of `parent` called `name` is in, or the empty one it
   goes into. The table has room: it is grown before it is asked. */
static cv_node** cv_slot(cv_tree* t, const cv_node* parent, const char* name, size_t n,
                         unsigned long hash) {
    size_t i = (size_t)hash & (t->cap - 1u);

    while (t->slots[i] != NULL) {
        const cv_node* m = t->slots[i];

        if (m->parent == parent && m->nlen == n && memcmp(m->name, name, n) == 0) {
            break;
        }
        i = (i + 1u) & (t->cap - 1u);
    }
    return &t->slots[i];
}

/* Doubled at three quarters full. False when there is no room. */
static cf_bool cv_room(cv_tree* t) {
    cv_node** old;
    size_t    old_cap;
    size_t    i;

    if (t->cap != 0 && (t->used + 1u) * 4u <= t->cap * 3u) {
        return CF_TRUE;
    }
    old      = t->slots;
    old_cap  = t->cap;
    t->cap   = (old_cap == 0) ? CV_SLOTS : old_cap * 2u;
    t->slots = (cv_node**)cf_arena_calloc(t->ar, t->cap * sizeof *t->slots);
    if (t->slots == NULL) {
        return CF_FALSE;
    }
    for (i = 0; i < old_cap; ++i) {
        if (old[i] != NULL) {
            size_t at = (size_t)old[i]->hash & (t->cap - 1u);

            while (t->slots[at] != NULL) {
                at = (at + 1u) & (t->cap - 1u);
            }
            t->slots[at] = old[i];
        }
    }
    return CF_TRUE;
}

/* A node under `parent`, its name and text copied, paid for out of the
   document's budget. NULL when the budget or the memory is gone, and the
   tree says which. */
static cv_node* cv_new(cv_tree* t, cv_node* parent, cv_nkind kind, const char* name,
                       size_t n, size_t text) {
    size_t   cost = n + text + CV_NODE_COST;
    cv_node* node;

    if (t->over || cost < n || cost > t->budget - t->spent) {
        t->over = CF_TRUE;
        return NULL;
    }
    node = (cv_node*)cf_arena_calloc(t->ar, sizeof *node);
    if (node == NULL) {
        t->nomem = CF_TRUE;
        return NULL;
    }
    t->spent += cost;
    node->kind   = kind;
    node->parent = parent;
    node->id     = (unsigned long)++t->serial;
    if (name != NULL) {
        node->name = (char*)cf_arena_alloc(t->ar, n + 1u);
        if (node->name == NULL) {
            t->nomem = CF_TRUE;
            return NULL;
        }
        memcpy(node->name, name, n);
        node->name[n] = '\0';
        node->nlen    = n;
    }
    if (parent != NULL) {
        node->pos = parent->count++;
        if (parent->last != NULL) {
            parent->last->next = node;
        } else {
            parent->first = node;
        }
        parent->last = node;
    }
    return node;
}

/* A table or array opens, or a value arrives: `name` is NULL inside an
   array and for the document. A table that meets an earlier table of its
   name is that table reopened (INI's repeated sections); anything else
   replaces the earlier member, which is left out - the later one wins, as
   the file means it and as a configuration reads it. */
static cv_node* cv_member(cv_tree* t, cv_nkind kind, const char* name, size_t n,
                          const cf_value* v) {
    cv_node*  parent = t->cur;
    cv_node*  node;
    cv_node** slot = NULL;
    size_t    text = (v != NULL && v->type == CF_TYPE_TEXT) ? v->u.t.n : 0;

    if (parent == NULL) {
        if (kind != CVN_TABLE) {
            return NULL;
        }
        /* The document again, with nothing in it: the YAML reader says
           one at the end of every file, and a configuration's sinks
           take it as the same table reopened. */
        if (t->root == NULL) {
            t->root = cv_new(t, NULL, kind, NULL, 0, 0);
        }
        return t->root;
    }
    if (parent->kind != CVN_TABLE) {
        name = NULL;
        n    = 0;
    } else if (name == NULL) {
        return NULL;
    }
    if (name != NULL) {
        unsigned long hash = cv_hash(parent->id, name, n);

        if (!cv_room(t)) {
            t->nomem = CF_TRUE;
            return NULL;
        }
        slot = cv_slot(t, parent, name, n, hash);
        if (*slot != NULL && (*slot)->kind == CVN_TABLE && kind == CVN_TABLE) {
            return *slot; /* reopened */
        }
        node = cv_new(t, parent, kind, name, n, text);
        if (node == NULL) {
            return NULL;
        }
        node->hash = hash;
        if (*slot != NULL) {
            (*slot)->left_out = CF_TRUE;
        } else {
            ++t->used;
        }
        *slot = node;
    } else {
        node = cv_new(t, parent, kind, NULL, 0, text);
        if (node == NULL) {
            return NULL;
        }
    }
    if (v != NULL) {
        node->value = *v;
        if (v->type == CF_TYPE_TEXT) {
            char* copy = (char*)cf_arena_alloc(t->ar, text + 1u);

            if (copy == NULL) {
                t->nomem = CF_TRUE;
                return NULL;
            }
            memcpy(copy, v->u.t.p, text);
            copy[text]        = '\0';
            node->value.u.t.p = copy;
        }
    }
    return node;
}

static cf_bool cv_open(cv_tree* t, cv_nkind kind, const char* name, size_t n) {
    cv_node* node = cv_member(t, kind, name, n, NULL);

    if (node == NULL) {
        return CF_FALSE;
    }
    t->cur = node;
    return CF_TRUE;
}

static cf_bool cv_rec_table(void* ctx, const char* name, size_t n) {
    return cv_open((cv_tree*)ctx, CVN_TABLE, name, n);
}

static cf_bool cv_rec_array(void* ctx, const char* name, size_t n) {
    return cv_open((cv_tree*)ctx, CVN_ARRAY, name, n);
}

static cf_bool cv_rec_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                             cf_bool textual, const cf_loc* at) {
    cv_tree* t = (cv_tree*)ctx;
    cv_node* node;

    (void)at;
    if (t->cur == NULL) {
        return CF_FALSE;
    }
    node = cv_member(t, CVN_VALUE, name, n, v);
    if (node == NULL) {
        return CF_FALSE;
    }
    node->textual = textual;
    return CF_TRUE;
}

static cf_bool cv_rec_end(void* ctx) {
    cv_tree* t = (cv_tree*)ctx;

    if (t->cur == NULL) {
        return CF_FALSE;
    }
    t->cur = t->cur->parent;
    return CF_TRUE;
}

/* The room a reader asks for is lent from the arena. */
static char* cv_rec_text(void* ctx, size_t n) {
    return (char*)cf_arena_alloc(((cv_tree*)ctx)->ar, n);
}

static void* cv_rec_work(void* ctx, size_t n) {
    return cf_arena_alloc(((cv_tree*)ctx)->ar, n);
}


/* ------------------------------------------------------------------ *
 * Findings
 * ------------------------------------------------------------------ */

/* `n` bytes of text into `out` at `at` (NULL out: only measured), a
   control byte as \xNN. Returns the new position. */
static size_t cv_esc(char* out, size_t at, const char* p, size_t n) {
    static const char hex[] = "0123456789abcdef";
    size_t            i;

    for (i = 0; i < n; ++i) {
        unsigned char ch = (unsigned char)p[i];

        if (ch < 0x20u || ch == 0x7fu) {
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

/* The names from the document down to `n`, joined by '.', an array's
   members by their index, into `out` at `at` (NULL: measured). */
static size_t cv_path(const cv_node* n, char* out, size_t at) {
    if (n->parent == NULL) {
        return at;
    }
    at = cv_path(n->parent, out, at);
    if (n->parent->parent != NULL) {
        if (out != NULL) {
            out[at] = '.';
        }
        ++at;
    }
    if (n->name != NULL) {
        return cv_esc(out, at, n->name, n->nlen);
    }
    {
        char   num[32];
        size_t len;

        sprintf(num, "%lu", (unsigned long)n->pos);
        len = strlen(num);
        if (out != NULL) {
            memcpy(out + at, num, len);
        }
        return at + len;
    }
}

/* A finding of `kind` for `n`, which, with all that is under it, is left
   out. `named` keeps the node's own name for the message. */
static void cv_note(cv_check* k, cv_node* n, cv_kind kind, cf_bool named) {
    cv_finding* f;
    char*       path;
    size_t      len;

    n->left_out = CF_TRUE;
    if (k->nomem) {
        return;
    }
    if (k->n == k->cap) {
        size_t      cap  = (k->cap == 0) ? 16u : k->cap * 2u;
        cv_finding* list = (cv_finding*)cf_arena_alloc(k->ar, cap * sizeof *list);

        if (list == NULL) {
            k->nomem = CF_TRUE;
            return;
        }
        if (k->n > 0) {
            memcpy(list, k->list, k->n * sizeof *list);
        }
        k->list = list;
        k->cap  = cap;
    }
    len  = cv_path(n, NULL, 0);
    path = (char*)cf_arena_alloc(k->ar, len + 1u);
    if (path == NULL) {
        k->nomem = CF_TRUE;
        return;
    }
    (void)cv_path(n, path, 0);
    path[len] = '\0';
    f         = &k->list[k->n++];
    f->kind   = kind;
    f->path   = path;
    f->name   = NULL;
    if (named) {
        size_t nl   = cv_esc(NULL, 0, n->name, n->nlen);
        char*  copy = (char*)cf_arena_alloc(k->ar, nl + 1u);

        if (copy == NULL) {
            k->nomem = CF_TRUE;
            return;
        }
        (void)cv_esc(copy, 0, n->name, n->nlen);
        copy[nl] = '\0';
        f->name  = copy;
    }
}

/* One or more of A-Z, a-z, 0-9, '_' and '-': what an encoder that writes
   a name as it is writes unquoted. */
static cf_bool cv_plain_name(const char* p, size_t n) {
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

/* Pass 1: the names. */
static void cv_pass_names(cv_check* k, cv_node* n) {
    cv_node* c;

    for (c = n->first; c != NULL; c = c->next) {
        if (c->left_out) {
            continue;
        }
        if (c->name != NULL && !cv_plain_name(c->name, c->nlen)) {
            cv_note(k, c, CV_NAME, CF_TRUE);
        } else if (c->kind != CVN_VALUE) {
            cv_pass_names(k, c);
        }
    }
}

/* Pass 2: the arrays, by what the target's caps say it writes. An array
   it can write holds nothing a table could be inside. */
static void cv_pass_arrays(cv_check* k, cv_node* n) {
    cv_node* c;
    cf_bool  scalar_only = ((k->target->caps & CF_CODEC_SCALAR_ARRAYS) != 0u);

    for (c = n->first; c != NULL; c = c->next) {
        if (c->left_out) {
            continue;
        }
        if (c->kind == CVN_ARRAY && !scalar_only) {
            cv_note(k, c, CV_ARRAY, CF_FALSE);
        } else if (c->kind == CVN_ARRAY) {
            const cv_node* m;
            cv_kind        what = (c->first == NULL) ? CV_EMPTY_ARRAY : CV_OTHER;

            for (m = c->first; m != NULL && what == CV_OTHER; m = m->next) {
                if (m->kind == CVN_TABLE) {
                    what = CV_TABLE_ARRAY;
                }
            }
            for (m = c->first; m != NULL && what == CV_OTHER; m = m->next) {
                if (m->kind == CVN_ARRAY) {
                    what = CV_NESTED_ARRAY;
                }
            }
            if (what != CV_OTHER) {
                cv_note(k, c, what, CF_FALSE);
            }
        } else if (c->kind == CVN_TABLE) {
            cv_pass_arrays(k, c);
        }
    }
}

/* What kind of finding an array is, by the bits the target holds, judged
   on its children as they were read (whatever a pass has left out
   since): 0 for none. */
static cv_kind cv_array_kind(const cv_node* a, unsigned holds) {
    const cv_node* m;

    if ((holds & CF_HOLDS_ARRAYS) == 0u) {
        return CV_ARRAY;
    }
    if (a->first == NULL) {
        return ((holds & CF_HOLDS_EMPTY_ARRAY) == 0u) ? CV_EMPTY_ARRAY : (cv_kind)0;
    }
    for (m = a->first; m != NULL; m = m->next) {
        if (m->kind == CVN_TABLE && (holds & CF_HOLDS_TABLE_ARRAYS) == 0u) {
            return CV_TABLE_ARRAY;
        }
    }
    for (m = a->first; m != NULL; m = m->next) {
        if (m->kind == CVN_ARRAY && (holds & CF_HOLDS_NESTED_ARRAYS) == 0u) {
            return CV_NESTED_ARRAY;
        }
    }
    return (cv_kind)0;
}

/* Passes 1 and 2 for a target the table of what a format holds knows
   (D-109): each member of `n` is judged by the bits and on the document
   as it was read, so that no finding depends on the order of the
   checks. A name is judged first, an empty one by EMPTY_NAME and any
   other by ANY_NAME; then, by its kind, an array, a table's path, an
   empty table. `plen` is the length of the joined path of `n` when it is
   a table written with a header (0 for the document); inside an array
   (`inline_`) a table is written with no path. */
static void cv_pass_bits(cv_check* k, cv_node* n, size_t plen, cf_bool inline_,
                         size_t depth) {
    cv_node* c;
    cf_bool  in_table = (n->kind == CVN_TABLE) ? CF_TRUE : CF_FALSE;

    for (c = n->first; c != NULL; c = c->next) {
        if (c->left_out) {
            continue;
        }
        if (in_table && c->nlen == 0) {
            if ((k->holds & CF_HOLDS_EMPTY_NAME) == 0u) {
                cv_note(k, c, CV_EMPTY_NAME, CF_FALSE);
                continue;
            }
        } else if (in_table && !cv_plain_name(c->name, c->nlen) &&
                   (k->holds & CF_HOLDS_ANY_NAME) == 0u) {
            cv_note(k, c, CV_NAME, CF_TRUE);
            continue;
        }
        if (c->kind != CVN_VALUE && depth + 1u > k->max_depth) {
            cv_note(k, c, CV_DEEP, CF_FALSE);
            continue;
        }
        if (in_table && !inline_ && c->kind != CVN_TABLE &&
            ((plen == 0) ? c->nlen : plen + 1u + c->nlen) > k->max_path) {
            cv_note(k, c, CV_LONG_PATH, CF_FALSE);
            continue;
        }
        if (c->kind == CVN_ARRAY) {
            cv_kind what = cv_array_kind(c, k->holds);

            if (what != (cv_kind)0) {
                cv_note(k, c, what, CF_FALSE);
            } else {
                cv_pass_bits(k, c, 0u, CF_TRUE, depth + 1u);
            }
        } else if (c->kind == CVN_TABLE) {
            size_t sub = plen;

            if (in_table && !inline_) {
                sub = (plen == 0) ? c->nlen : plen + 1u + c->nlen;
                if (sub > k->max_path) {
                    cv_note(k, c, CV_LONG_PATH, CF_FALSE);
                    continue;
                }
            }
            if (in_table && c->first == NULL && (k->holds & CF_HOLDS_EMPTY_TABLE) == 0u) {
                cv_note(k, c, CV_EMPTY_TABLE, CF_FALSE);
            } else {
                cv_pass_bits(k, c, sub, inline_, depth + 1u);
            }
        }
    }
}

/* ------------------------------------------------------------------ *
 * The replay
 * ------------------------------------------------------------------ */

static void cv_emit(cv_out* o, cv_node* n);

/* Whether the target writes `v` as its text: a value that is not text,
   of a type its format lacks (D-109); a target the table does not know
   is held to its CF_CODEC_TYPED. Only counted in the passes. */
static cf_bool cv_as_text(const cv_check* k, const cf_value* v) {
    if (k == NULL || v->type == CF_TYPE_TEXT) {
        return CF_FALSE;
    }
    if (!k->known) {
        return ((k->target->caps & CF_CODEC_TYPED) == 0u) ? CF_TRUE : CF_FALSE;
    }
    switch (v->type) {
    case CF_TYPE_INT:
        return ((k->holds & CF_HOLDS_INT) == 0u) ? CF_TRUE : CF_FALSE;
    case CF_TYPE_REAL:
        return ((k->holds & CF_HOLDS_REAL) == 0u) ? CF_TRUE : CF_FALSE;
    case CF_TYPE_BOOL:
        return ((k->holds & CF_HOLDS_BOOL) == 0u) ? CF_TRUE : CF_FALSE;
    case CF_TYPE_NULL:
        return ((k->holds & (CF_HOLDS_NULL | CF_REFUSES_NULL)) == 0u) ? CF_TRUE
                                                                      : CF_FALSE;
    default:
        break;
    }
    return CF_FALSE;
}

/* The encoder refused `n`: a finding in the last pass, a bug after it. */
static void cv_refused(cv_out* o, cv_node* n) {
    cv_kind what = CV_OTHER;

    if (o->check == NULL) {
        o->failed = CF_TRUE;
        return;
    }
    if (n->kind == CVN_VALUE && n->value.type == CF_TYPE_NULL) {
        what = CV_NULL;
    } else if (n->kind == CVN_VALUE && n->value.type == CF_TYPE_TEXT &&
               memchr(n->value.u.t.p, '\n', n->value.u.t.n) != NULL) {
        what = CV_LINE_BREAK;
    }
    cv_note(o->check, n, what, CF_FALSE);
}

/* The members of a table in the order its target needs - its values,
   then its arrays, then its tables - and of an array in the order they
   came. A node left out is not walked. */
static void cv_emit_members(cv_out* o, cv_node* n) {
    static const cv_nkind order[3] = {CVN_VALUE, CVN_ARRAY, CVN_TABLE};
    cv_node*              c;
    unsigned              pass;

    if (n->kind == CVN_ARRAY) {
        for (c = n->first; c != NULL && !o->failed; c = c->next) {
            cv_emit(o, c);
        }
        return;
    }
    for (pass = 0; pass < 3u; ++pass) {
        for (c = n->first; c != NULL && !o->failed; c = c->next) {
            if (c->kind == order[pass]) {
                cv_emit(o, c);
            }
        }
    }
}

static void cv_emit(cv_out* o, cv_node* n) {
    cf_doc_sink* s = o->sink;
    cf_bool      ok;

    if (n->left_out) {
        return;
    }
    if (n->kind == CVN_VALUE) {
        ok = s->scalar(s->ctx, n->name, n->nlen, &n->value, n->textual, &o->none);
        if (!ok) {
            cv_refused(o, n);
        } else if (cv_as_text(o->check, &n->value)) {
            ++o->non_text;
        }
        return;
    }
    ok = (n->kind == CVN_TABLE) ? s->begin_table(s->ctx, n->name, n->nlen)
                                : s->begin_array(s->ctx, n->name, n->nlen);
    if (!ok) {
        cv_refused(o, n); /* its events are not passed on */
        return;
    }
    cv_emit_members(o, n);
    if (!o->failed && !s->end(s->ctx)) {
        o->failed = CF_TRUE;
    }
}

/* The tree into the target's encoder, which writes into `out`. False
   when the encoder or the walk failed. */
static cf_bool cv_run(const cf_codec* c, cv_node* root, cv_check* check, cf_sink* out,
                      size_t* non_text) {
    cf_doc_sink sink;
    cv_out      o;
    /* The encoder's state, as cf_emit holds it, aligned through the
       union because a char array is not aligned for a struct. */
    union {
        cf_i64 align;
        char   b[CF_ENCODER_STATE_MAX];
    } state;

    memset(&sink, 0, sizeof sink);
    memset(&o, 0, sizeof o);
    if (!c->encoder(out, state.b, &sink)) {
        return CF_FALSE;
    }
    o.sink  = &sink;
    o.check = check;
    cv_emit(&o, root);
    if (non_text != NULL) {
        *non_text = o.non_text;
    }
    return !o.failed;
}

/* ------------------------------------------------------------------ *
 * The conversion
 * ------------------------------------------------------------------ */

/* The document read and recorded: 0, or the exit code with `err` set. */
static int cv_record(const cf_codec* from, const cf_blob* blob, cv_tree* t,
                     cf_error* err) {
    cf_doc_sink sink;

    memset(&sink, 0, sizeof sink);
    sink.size         = sizeof sink;
    sink.begin_table  = cv_rec_table;
    sink.begin_array  = cv_rec_array;
    sink.scalar       = cv_rec_scalar;
    sink.end          = cv_rec_end;
    sink.ctx          = t;
    sink.reserve_text = cv_rec_text;
    sink.reserve_work = cv_rec_work;
    if (cf_codec_decode(from, blob, &sink, err)) {
        return 0;
    }
    if (t->over) {
        (void)cf_error_set(err, CF_E_SOURCE, NULL, NULL,
                           "makes more than %u times its size; refused (D-82's bound)",
                           CV_FACTOR);
        return 2;
    }
    if (t->nomem) {
        (void)cf_error_set(err, CF_E_MEMORY, NULL, NULL, "out of memory");
        return 2;
    }
    return 1;
}

/* The three passes. */
static void cv_find(cv_check* k, cv_node* root, size_t* non_text) {
    const cf_codec* c = k->target;
    cf_sink         nothing;

    k->known = (c->name != NULL && cf_format_holds(c->name, &k->holds) &&
                cf_format_limits(c->name, &k->max_path, &k->max_depth))
                   ? CF_TRUE
                   : CF_FALSE;
    if (k->known) {
        cv_pass_bits(k, root, 0u, CF_FALSE, 0u);
    } else {
        /* A codec the table does not know: W5-V1's passes, by its caps. */
        cv_pass_names(k, root);
        if ((c->caps & CF_CODEC_ARRAYS) == 0u) {
            cv_pass_arrays(k, root);
        }
    }
    cf_sink_init(&nothing, NULL, 0);
    if (!cv_run(c, root, k, &nothing, non_text)) {
        *non_text = (size_t)-1;
    }
}

/* The tree as bytes in the target's format, from the arena. */
static cf_bool cv_bytes(const cf_codec* target, cv_node* root, cf_arena* ar,
                        cf_blob* out, cf_error* err) {
    cf_sink s;
    char*   buf = NULL;
    cf_bool ok;

    cf_sink_init(&s, NULL, 0);
    ok = cv_run(target, root, NULL, &s, NULL);
    if (ok) {
        size_t len = s.len;

        buf = (char*)cf_arena_alloc(ar, len + 1u);
        if (buf == NULL) {
            (void)cf_error_set(err, CF_E_MEMORY, NULL, NULL, "out of memory");
            return CF_FALSE;
        }
        cf_sink_init(&s, buf, len + 1u);
        ok = cv_run(target, root, NULL, &s, NULL) && s.len == len;
        out->p = buf;
        out->n = len;
    }
    if (!ok) {
        (void)cf_error_set(err, CF_E_SOURCE, NULL, NULL,
                           "the '%s' encoder refused the document (a bug in "
                           "configforge codec: the check let it through)",
                           target->name);
    }
    return ok;
}

int cv_convert(cf_arena* ar, const cf_blob* in, const cf_codec* from,
               const cf_codec* to, cv_result* res, cf_error* err) {
    cv_tree  t;
    cv_check k;
    size_t   non_text = 0;
    int      rc;

    memset(res, 0, sizeof *res);
    memset(&t, 0, sizeof t);
    memset(&k, 0, sizeof k);
    t.ar     = ar;
    t.budget = (in->n > ((size_t)-1 - CV_FLOOR) / CV_FACTOR)
                   ? (size_t)-1
                   : in->n * CV_FACTOR + CV_FLOOR;
    rc       = cv_record(from, in, &t, err);
    if (rc != 0) {
        return rc;
    }
    k.ar     = ar;
    k.target = to;
    cv_find(&k, t.root, &non_text);
    res->findings  = k.list;
    res->nfindings = k.n;
    if (k.nomem) {
        (void)cf_error_set(err, CF_E_MEMORY, NULL, NULL, "out of memory");
        return 2;
    }
    if (non_text == (size_t)-1) {
        (void)cf_error_set(err, CF_E_SOURCE, NULL, NULL,
                           "the '%s' encoder refused the document", to->name);
        return 3;
    }
    res->as_text = non_text;
    if (!cv_bytes(to, t.root, ar, &res->out, err)) {
        return (err->code == CF_E_MEMORY) ? 2 : 3;
    }
    return 0;
}
