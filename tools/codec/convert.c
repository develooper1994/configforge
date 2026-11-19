/*
 * convert.c - `configforge codec convert` and `configforge codec format`
 * (D-94): a document read through one codec and written through the
 * encoder of another, as the codecs' events.
 *
 * The decoder's events are recorded in a tree, and the tree is walked
 * into the target's encoder in the order the target needs: a table's
 * values, then its arrays, then its tables. Events straight from the
 * decoder to the encoder would put a JSON document's later root key
 * inside an INI section, and a TOML array after a sub-table inside that
 * sub-table (the two faults src/bridge/flatten.c's comment above
 * all_digits records), and the key space that orders them is not in the
 * serialization library, which this program links alone.
 *
 * What the target cannot hold is found before anything is written, in
 * three passes over the tree, and named; nothing is dropped quietly. C89.
 */

#include "codec.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/* D-82's rule made the tool's (the library's CF_DECODE_* are in
   cf_flatten.h, which this program does not see): each node costs its
   name's length, its text's length and CV_NODE_COST bytes, and a
   document may cost CV_FACTOR times its size and CV_FLOOR bytes more. */
#define CV_FACTOR 64u
#define CV_FLOOR 1048576u
#define CV_NODE_COST 64u

/* The member table starts with this many slots, a power of two. */
#define CV_SLOTS 64u

typedef enum cv_kind { CV_TABLE, CV_ARRAY, CV_VALUE } cv_kind;

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
    cv_kind         kind;
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

/* The words a message is made of, and what it counts. */
typedef struct cv_check {
    const char*     cmd; /* "codec convert" or "codec format" */
    const cf_codec* target;
    size_t          findings;
} cv_check;

/* One walk into an encoder. With `check` set it is the third pass: a
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
static cv_node* cv_new(cv_tree* t, cv_node* parent, cv_kind kind, const char* name,
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
static cv_node* cv_member(cv_tree* t, cv_kind kind, const char* name, size_t n,
                          const cf_value* v) {
    cv_node*  parent = t->cur;
    cv_node*  node;
    cv_node** slot = NULL;
    size_t    text = (v != NULL && v->type == CF_TYPE_TEXT) ? v->u.t.n : 0;

    if (parent == NULL) {
        if (t->root != NULL || kind != CV_TABLE) {
            return NULL;
        }
        t->root = cv_new(t, NULL, kind, NULL, 0, 0);
        return t->root;
    }
    if (parent->kind != CV_TABLE) {
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
        if (*slot != NULL && (*slot)->kind == CV_TABLE && kind == CV_TABLE) {
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

static cf_bool cv_open(cv_tree* t, cv_kind kind, const char* name, size_t n) {
    cv_node* node = cv_member(t, kind, name, n, NULL);

    if (node == NULL) {
        return CF_FALSE;
    }
    t->cur = node;
    return CF_TRUE;
}

static cf_bool cv_rec_table(void* ctx, const char* name, size_t n) {
    return cv_open((cv_tree*)ctx, CV_TABLE, name, n);
}

static cf_bool cv_rec_array(void* ctx, const char* name, size_t n) {
    return cv_open((cv_tree*)ctx, CV_ARRAY, name, n);
}

static cf_bool cv_rec_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                             cf_bool textual, const cf_loc* at) {
    cv_tree* t = (cv_tree*)ctx;
    cv_node* node;

    (void)at;
    if (t->cur == NULL) {
        return CF_FALSE;
    }
    node = cv_member(t, CV_VALUE, name, n, v);
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

/* `n` bytes of text on stderr, a control byte as \xNN. */
static void cv_put(const char* p, size_t n) {
    static const char hex[] = "0123456789abcdef";
    size_t            i;

    for (i = 0; i < n; ++i) {
        unsigned char ch = (unsigned char)p[i];

        if (ch < 0x20u || ch == 0x7fu) {
            fputs("\\x", stderr);
            fputc(hex[ch >> 4], stderr);
            fputc(hex[ch & 15u], stderr);
        } else {
            fputc((int)ch, stderr);
        }
    }
}

/* The names from the document down to `n`, joined by '.', an array's
   members by their index. */
static void cv_put_path(const cv_node* n) {
    if (n->parent == NULL) {
        return;
    }
    cv_put_path(n->parent);
    if (n->parent->parent != NULL) {
        fputc('.', stderr);
    }
    if (n->name != NULL) {
        cv_put(n->name, n->nlen);
    } else {
        fprintf(stderr, "%lu", (unsigned long)n->pos);
    }
}

/* `<cmd>: <path>: <what>, which <format> cannot hold; left out`, and the
   node, with all that is under it, is left out. `named` adds the name
   to what is said. */
static void cv_finding(cv_check* k, cv_node* n, const char* what, cf_bool named) {
    fprintf(stderr, "%s: ", k->cmd);
    cv_put_path(n);
    fprintf(stderr, ": %s", what);
    if (named) {
        fputs(" ('", stderr);
        cv_put(n->name, n->nlen);
        fputs("')", stderr);
    }
    fprintf(stderr, ", which %s cannot hold; left out\n", k->target->name);
    n->left_out = CF_TRUE;
    ++k->findings;
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
            cv_finding(k, c, "a name it would write unquoted", CF_TRUE);
        } else if (c->kind != CV_VALUE) {
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
        if (c->kind == CV_ARRAY && !scalar_only) {
            cv_finding(k, c, "an array", CF_FALSE);
        } else if (c->kind == CV_ARRAY) {
            const cv_node* m;
            const char*    what = (c->first == NULL) ? "an empty array" : NULL;

            for (m = c->first; m != NULL && what == NULL; m = m->next) {
                if (m->kind == CV_TABLE) {
                    what = "an array of tables";
                }
            }
            for (m = c->first; m != NULL && what == NULL; m = m->next) {
                if (m->kind == CV_ARRAY) {
                    what = "an array of arrays";
                }
            }
            if (what != NULL) {
                cv_finding(k, c, what, CF_FALSE);
            }
        } else if (c->kind == CV_TABLE) {
            cv_pass_arrays(k, c);
        }
    }
}

/* ------------------------------------------------------------------ *
 * The replay
 * ------------------------------------------------------------------ */

static void cv_emit(cv_out* o, cv_node* n);

/* The encoder refused `n`: a finding in the third pass, a bug after it. */
static void cv_refused(cv_out* o, cv_node* n) {
    const char* what = "a value of this kind";

    if (o->check == NULL) {
        o->failed = CF_TRUE;
        return;
    }
    if (n->kind == CV_VALUE && n->value.type == CF_TYPE_NULL) {
        what = "a null";
    } else if (n->kind == CV_VALUE && n->value.type == CF_TYPE_TEXT &&
               memchr(n->value.u.t.p, '\n', n->value.u.t.n) != NULL) {
        what = "text with a line break";
    }
    cv_finding(o->check, n, what, CF_FALSE);
}

/* The members of a table in the order its target needs - its values,
   then its arrays, then its tables - and of an array in the order they
   came. A node left out is not walked. */
static void cv_emit_members(cv_out* o, cv_node* n) {
    static const cv_kind order[3] = {CV_VALUE, CV_ARRAY, CV_TABLE};
    cv_node*             c;
    unsigned             pass;

    if (n->kind == CV_ARRAY) {
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
    if (n->kind == CV_VALUE) {
        ok = s->scalar(s->ctx, n->name, n->nlen, &n->value, n->textual, &o->none);
        if (!ok) {
            cv_refused(o, n);
        } else if (n->value.type != CF_TYPE_TEXT) {
            ++o->non_text;
        }
        return;
    }
    ok = (n->kind == CV_TABLE) ? s->begin_table(s->ctx, n->name, n->nlen)
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
 * The command
 * ------------------------------------------------------------------ */

/* The formats whose files can hold comments, which no reader keeps. */
static cf_bool cv_has_comments(const char* name) {
    static const char* const formats[] = {"ini",   "toml",       "yaml",
                                          "json5", "properties", "xml"};
    size_t                   i;

    for (i = 0; i < sizeof formats / sizeof formats[0]; ++i) {
        if (strcmp(formats[i], name) == 0) {
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}

typedef struct cv_args {
    const char* in;
    const char* to;
    const char* as;
    const char* out;
    cf_bool     strict;
} cv_args;

/* The input once, `--to` once and only for `convert`, `--as` and `--out`
   once, `--strict` as a flag; anything else is the usage. */
static cf_bool cv_parse(int argc, char** argv, cf_bool same, cv_args* a) {
    int i;

    memset(a, 0, sizeof *a);
    for (i = 0; i < argc; ++i) {
        const char** slot = NULL;

        if (strcmp(argv[i], "--to") == 0 && !same) {
            slot = &a->to;
        } else if (strcmp(argv[i], "--as") == 0) {
            slot = &a->as;
        } else if (strcmp(argv[i], "--out") == 0) {
            slot = &a->out;
        } else if (strcmp(argv[i], "--strict") == 0) {
            a->strict = CF_TRUE;
        } else if (strncmp(argv[i], "--", 2) == 0 || a->in != NULL) {
            return CF_FALSE;
        } else {
            a->in = argv[i];
        }
        if (slot != NULL) {
            if (*slot != NULL || i + 1 >= argc) {
                return CF_FALSE;
            }
            *slot = argv[++i];
        }
    }
    return a->in != NULL && (same || a->to != NULL);
}

/* The notes, said once each: about the formats, the same for every file,
   and so not findings. */
static void cv_notes(const cv_check* k, const cf_codec* from, const char* in) {
    if (cv_has_comments(from->name)) {
        fprintf(stderr,
                "%s: note: %s can hold comments, and no reader keeps them: any in %s "
                "are not in the output\n",
                k->cmd, from->name, in);
    }
    if ((from->caps & CF_CODEC_TYPED) == 0u &&
        (k->target->caps & CF_CODEC_TYPED) != 0u) {
        fprintf(stderr,
                "%s: note: %s carries no types, so its values are written to %s as "
                "text\n",
                k->cmd, from->name, k->target->name);
    }
}

/* The three passes. Findings go to stderr as they are found. */
static void cv_find(cv_check* k, cv_node* root, size_t* non_text) {
    const cf_codec* c = k->target;
    cf_sink         nothing;

    if ((c->name == NULL) ||
        (strcmp(c->name, "json") != 0 && strcmp(c->name, "json5") != 0 &&
         strcmp(c->name, "cbor") != 0 && strcmp(c->name, "yaml") != 0)) {
        cv_pass_names(k, root);
    }
    if ((c->caps & CF_CODEC_ARRAYS) == 0u) {
        cv_pass_arrays(k, root);
    }
    cf_sink_init(&nothing, NULL, 0);
    if (!cv_run(c, root, k, &nothing, non_text)) {
        ++k->findings; /* the encoder failed outside a node: said by the caller */
        *non_text = (size_t)-1;
    }
}

/* `data` to `path`, or to standard output when `path` is NULL. */
static int cv_write(const char* path, const char* data, size_t n) {
    FILE*   f = stdout;
    cf_bool ok;

    if (path != NULL) {
        f = fopen(path, "wb");
        if (f == NULL) {
            fprintf(stderr, "codec: cannot write %s\n", path);
            return 2;
        }
    } else {
#ifdef _WIN32
        (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
    }
    ok = (fwrite(data, 1, n, f) == n) ? CF_TRUE : CF_FALSE;
    if (path != NULL) {
        if (fclose(f) != 0) {
            ok = CF_FALSE;
        }
    } else if (fflush(f) != 0) {
        ok = CF_FALSE;
    }
    if (!ok) {
        if (path != NULL) {
            remove(path);
        }
        fprintf(stderr, "codec: cannot write %s\n",
                (path != NULL) ? path : "standard output");
        return 2;
    }
    return 0;
}

/* The target must write, and its state must fit. */
static int cv_target(const cf_codec* c) {
    if ((c->caps & CF_CODEC_WRITE) == 0u || c->encoder == NULL) {
        fprintf(stderr, "codec: format '%s' can be read but not written\n", c->name);
        return 2;
    }
    if (c->encoder_state_size > CF_ENCODER_STATE_MAX) {
        fprintf(stderr, "codec: no room for the '%s' encoder\n", c->name);
        return 2;
    }
    return 0;
}

/* The document read and recorded: 0, or the exit code. */
static int cv_record(const cf_codec* from, const cf_blob* blob, const char* in,
                     cv_tree* t) {
    cf_doc_sink sink;
    cf_error    err = CF_ERROR_INIT;

    memset(&sink, 0, sizeof sink);
    sink.size         = sizeof sink;
    sink.begin_table  = cv_rec_table;
    sink.begin_array  = cv_rec_array;
    sink.scalar       = cv_rec_scalar;
    sink.end          = cv_rec_end;
    sink.ctx          = t;
    sink.reserve_text = cv_rec_text;
    sink.reserve_work = cv_rec_work;
    if (cf_codec_decode(from, blob, &sink, &err)) {
        return 0;
    }
    if (t->over) {
        fprintf(stderr,
                "codec: %s makes more than %u times its size; refused (D-82's bound)\n",
                in, CV_FACTOR);
        return 2;
    }
    if (t->nomem) {
        fprintf(stderr, "codec: out of memory\n");
        return 2;
    }
    fprintf(stderr, "codec: %s\n", err.message);
    return 1;
}

/* The tree as bytes in the target's format, from the arena. */
static int cv_bytes(const cf_codec* target, cv_node* root, cf_arena* ar, char** buf,
                    size_t* len, const char* cmd) {
    cf_sink s;
    cf_bool ok;

    cf_sink_init(&s, NULL, 0);
    ok = cv_run(target, root, NULL, &s, NULL);
    if (ok) {
        *len = s.len;
        *buf = (char*)cf_arena_alloc(ar, s.len + 1u);
        if (*buf == NULL) {
            fprintf(stderr, "codec: out of memory\n");
            return 2;
        }
        cf_sink_init(&s, *buf, s.len + 1u);
        ok = cv_run(target, root, NULL, &s, NULL) && s.len == *len;
    }
    if (!ok) {
        fprintf(
            stderr,
            "%s: the '%s' encoder refused the document (a bug in configforge codec: "
            "the check let it through)\n",
            cmd, target->name);
        return 3;
    }
    return 0;
}

int codec_convert(int argc, char** argv, cf_bool same_format) {
    cv_args         a;
    cv_check        k;
    cv_tree         t;
    cf_arena*       ar;
    cf_blob         blob;
    const cf_codec* from = NULL;
    cf_detect       how;
    size_t          non_text = 0;
    char*           buf      = NULL;
    size_t          len      = 0;
    int             rc;

    k.cmd = same_format ? "codec format" : "codec convert";
    if (!cv_parse(argc, argv, same_format, &a)) {
        if (same_format) {
            fprintf(stderr,
                    "codec: format <file> [--as <format>] [--out <file>] [--strict]\n");
        } else {
            fprintf(
                stderr,
                "codec: convert <file> --to <format> [--as <format>] [--out <file>] "
                "[--strict]\n");
        }
        return 2;
    }
    k.target   = NULL;
    k.findings = 0;
    memset(&t, 0, sizeof t);
    if (!same_format) {
        rc = codec_named(a.to, &k.target);
        if (rc == 0) {
            rc = cv_target(k.target);
        }
        if (rc != 0) {
            return rc;
        }
    }
    ar = cf_arena_new(NULL, 0);
    if (ar == NULL) {
        fprintf(stderr, "codec: out of memory\n");
        return 2;
    }
    rc = codec_read(a.in, a.as, ar, &blob, &from, &how);
    if (rc == 0 && same_format) {
        k.target = from;
        rc       = cv_target(from);
    }
    if (rc == 0) {
        t.ar     = ar;
        t.budget = (blob.n > ((size_t)-1 - CV_FLOOR) / CV_FACTOR)
                       ? (size_t)-1
                       : blob.n * CV_FACTOR + CV_FLOOR;
        rc       = cv_record(from, &blob, a.in, &t);
    }
    if (rc == 0) {
        cv_notes(&k, from, a.in);
        cv_find(&k, t.root, &non_text);
        if (non_text == (size_t)-1) {
            fprintf(stderr, "%s: the '%s' encoder refused the document\n", k.cmd,
                    k.target->name);
            rc = 3;
        } else if ((k.target->caps & CF_CODEC_TYPED) == 0u && non_text > 0) {
            fprintf(
                stderr,
                "%s: %s carries no types: %lu value(s) that were numbers, booleans or "
                "nulls are written as their text\n",
                k.cmd, k.target->name, (unsigned long)non_text);
            ++k.findings;
        }
    }
    if (rc == 0 && a.strict && k.findings > 0) {
        fprintf(stderr, "%s: %lu finding(s); nothing written (--strict)\n", k.cmd,
                (unsigned long)k.findings);
        rc = 1;
    }
    if (rc == 0) {
        rc = cv_bytes(k.target, t.root, ar, &buf, &len, k.cmd);
    }
    if (rc == 0) {
        rc = cv_write(a.out, buf, len);
    }
    cf_arena_free(ar);
    return rc;
}
