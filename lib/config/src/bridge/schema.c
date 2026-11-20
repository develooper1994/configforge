/*
 * schema.c - definitions read from a JSON Schema (draft 2020-12), the
 * reverse of cf_schema_write. C89.
 *
 * A program that has its contract as a schema - for an editor, for a
 * service that validates the same file - declares it once, there, and
 * reads it here; the definitions are what cf_schema_write would write
 * back. Plan item 7, D-64.
 *
 * What is read, keyword by keyword (anything else is refused, by name
 * and place, rather than dropped: a constraint the program believes in
 * and the library does not enforce is worse than an error):
 *
 *   object    type "object", properties, required, additionalProperties
 *             (true opens the prefix, as cf_defset_open does; false and
 *             absent leave the global unknown-key policy to decide)
 *   any       description, title (the description when there is none),
 *             deprecated, writeOnly (CF_SENSITIVE), default, $comment,
 *             examples, format, readOnly (annotations: kept or ignored,
 *             never enforced by a schema reader either)
 *   integer   minimum, maximum, exclusiveMinimum, exclusiveMaximum
 *   number    minimum, maximum
 *   string    minLength, maxLength, enum (of strings)
 *   boolean   -
 *   array     items, one schema, of the scalar types above: a list
 *   root      $schema (2020-12 or 2019-09), $id, title
 *
 * The JSON is read by this library's own JSON reader (held to RFC 8259,
 * docs/Conformance.md) into a small tree in an arena, then walked.
 */

#include "configforge/cf_define.h"
#include "configforge/cf_serialize.h"

#include <string.h>

#if CF_WITH_JSON

/* ------------------------------------------------------------------ *
 * The document as a tree
 * ------------------------------------------------------------------ */

enum { SN_OBJECT, SN_ARRAY, SN_SCALAR };

typedef struct snode {
    struct snode* parent;
    struct snode* first;
    struct snode* last;
    struct snode* next;
    const char*   name; /* NULL for an array element */
    size_t        nlen;
    int           kind;
    cf_value      value; /* SN_SCALAR; its text in the arena */
    cf_loc        at;    /* the first value at or inside the node */
    cf_bool       placed;
} snode;

typedef struct stree {
    cf_arena* ar;
    snode*    root;
    snode*    cur;
    cf_bool   failed;
    char*     text;
    size_t    text_cap;
} stree;

static snode* st_add(stree* t, const char* name, size_t n, int kind) {
    snode* s = (snode*)cf_arena_calloc(t->ar, sizeof *s);
    if (s != NULL && name != NULL) {
        s->name = cf_arena_dup(t->ar, name, n);
    }
    if (s == NULL || (name != NULL && s->name == NULL)) {
        t->failed = CF_TRUE;
        return NULL;
    }
    s->nlen   = (name != NULL) ? n : 0;
    s->kind   = kind;
    s->parent = t->cur;
    if (t->cur != NULL) {
        if (t->cur->last != NULL) {
            t->cur->last->next = s;
        } else {
            t->cur->first = s;
        }
        t->cur->last = s;
    }
    return s;
}

static cf_bool st_open(stree* t, const char* name, size_t n, int kind) {
    snode* s = st_add(t, name, n, kind);
    if (s == NULL) {
        return CF_FALSE;
    }
    if (t->root == NULL) {
        t->root = s;
    }
    t->cur = s;
    return CF_TRUE;
}

static cf_bool st_table(void* ctx, const char* name, size_t n) {
    return st_open((stree*)ctx, name, n, SN_OBJECT);
}
static cf_bool st_array(void* ctx, const char* name, size_t n) {
    return st_open((stree*)ctx, name, n, SN_ARRAY);
}
static cf_bool st_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                         cf_bool textual, const cf_loc* at) {
    stree* t = (stree*)ctx;
    snode* s = st_add(t, name, n, SN_SCALAR);
    snode* up;
    (void)textual;
    if (s == NULL) {
        return CF_FALSE;
    }
    s->value = *v;
    if (v->type == CF_TYPE_TEXT) {
        char* p = cf_arena_dup(t->ar, v->u.t.p, v->u.t.n);
        if (p == NULL) {
            t->failed = CF_TRUE;
            return CF_FALSE;
        }
        s->value.u.t.p = p;
    }
    if (at != NULL) {
        s->at     = *at;
        s->placed = CF_TRUE;
        /* A container has no place of its own in the events: it takes
           that of the first value inside it. */
        for (up = t->cur; up != NULL && !up->placed; up = up->parent) {
            up->at     = *at;
            up->placed = CF_TRUE;
        }
    }
    return CF_TRUE;
}
static cf_bool st_end(void* ctx) {
    stree* t = (stree*)ctx;
    if (t->cur == NULL) {
        return CF_FALSE;
    }
    t->cur = t->cur->parent;
    return CF_TRUE;
}
static char* st_text(void* ctx, size_t n) {
    stree* t = (stree*)ctx;
    if (n > t->text_cap) {
        size_t c = (t->text_cap == 0) ? 256u : t->text_cap;
        while (c < n) {
            c *= 2u;
        }
        t->text = (char*)cf_arena_alloc(t->ar, c);
        if (t->text == NULL) {
            return NULL;
        }
        t->text_cap = c;
    }
    return t->text;
}

/* ------------------------------------------------------------------ *
 * The walk
 * ------------------------------------------------------------------ */

typedef struct swalk {
    cf_defset*  ds;
    cf_arena*   ar;
    cf_error*   err;
    const char* file;
} swalk;

/* A refusal at the place of `at`, the name quoted first when there is
   one. cf_fmt has no precision for %s, so the name is copied, cut short
   where the message would not hold it. */
static cf_bool sw_fail(const swalk* w, const snode* at, const char* what,
                       const char* name, size_t n) {
    char text[CF_MESSAGE_MAX];
    if (name != NULL) {
        size_t room = sizeof text - 4;
        size_t wn   = strlen(what);
        if (wn > room) {
            wn = room;
        }
        if (n > room - wn) {
            n = room - wn;
        }
        text[0] = '\'';
        memcpy(text + 1, name, n);
        text[1 + n] = '\'';
        text[2 + n] = ' ';
        memcpy(text + 3 + n, what, wn);
        text[3 + n + wn] = '\0';
        what             = text;
    }
    if (at != NULL && at->placed) {
        return cf_error_set(w->err, CF_E_DEFINITION, NULL, w->file, "%s:%u:%u: %s",
                            w->file, at->at.line, at->at.col, what);
    }
    return cf_error_set(w->err, CF_E_DEFINITION, NULL, w->file, "%s: %s", w->file,
                        what);
}

static cf_bool sn_is(const snode* s, const char* name) {
    return (s->name != NULL && s->nlen == strlen(name) &&
            memcmp(s->name, name, s->nlen) == 0)
               ? CF_TRUE
               : CF_FALSE;
}

static const snode* sn_child(const snode* s, const char* name) {
    const snode* c;
    for (c = s->first; c != NULL; c = c->next) {
        if (sn_is(c, name)) {
            return c;
        }
    }
    return NULL;
}

static cf_bool sn_text(const snode* s, const char* text) {
    return (s != NULL && s->kind == SN_SCALAR && s->value.type == CF_TYPE_TEXT &&
            s->value.u.t.n == strlen(text) &&
            memcmp(s->value.u.t.p, text, s->value.u.t.n) == 0)
               ? CF_TRUE
               : CF_FALSE;
}

/* A keyword that is there and true: "deprecated": true. */
static cf_bool sn_true(const snode* s, const char* name) {
    const snode* c = sn_child(s, name);
    return (c != NULL && c->kind == SN_SCALAR && c->value.type == CF_TYPE_BOOL &&
            c->value.u.b)
               ? CF_TRUE
               : CF_FALSE;
}

/* One of the names, or false. */
static cf_bool sn_among(const snode* s, const char* const* names) {
    size_t i;
    for (i = 0; names[i] != NULL; ++i) {
        if (sn_is(s, names[i])) {
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}

/* Every keyword of a schema is one this reader knows, or the schema is
   refused: by name, at its place. */
static cf_bool sw_known(const swalk* w, const snode* s, const char* const* known) {
    const snode*             c;
    static const char* const ann[] = {"description", "title",    "$comment", "examples",
                                      "format",      "readOnly", NULL};
    for (c = s->first; c != NULL; c = c->next) {
        if (!sn_among(c, known) && !sn_among(c, ann)) {
            return sw_fail(
                w, c,
                "is a JSON Schema keyword this library does not read; a "
                "constraint it cannot enforce is refused rather than dropped",
                c->name, c->nlen);
        }
    }
    return CF_TRUE;
}

static char* sw_dup(const swalk* w, const snode* s) {
    return (s != NULL && s->kind == SN_SCALAR && s->value.type == CF_TYPE_TEXT)
               ? cf_arena_dup(w->ar, s->value.u.t.p, s->value.u.t.n)
               : NULL;
}

static cf_bool sw_int(const snode* s, cf_i64* out) {
    if (s == NULL || s->kind != SN_SCALAR || s->value.type != CF_TYPE_INT) {
        return CF_FALSE;
    }
    *out = s->value.u.i;
    return CF_TRUE;
}

#if CF_WITH_REAL
static cf_bool sw_real(const snode* s, double* out) {
    if (s == NULL || s->kind != SN_SCALAR) {
        return CF_FALSE;
    }
    if (s->value.type == CF_TYPE_REAL) {
        *out = s->value.u.r;
        return CF_TRUE;
    }
    if (s->value.type == CF_TYPE_INT) {
        *out = (double)s->value.u.i;
        return CF_TRUE;
    }
    return CF_FALSE;
}
#endif

/* The scalar part of a leaf - a property's, or its array's items' - its
   type and the constraints of that type, into d. */
static cf_bool sw_scalar(const swalk* w, const snode* s, cf_def* d) {
    static const char* const k_int[] = {"type",
                                        "minimum",
                                        "maximum",
                                        "exclusiveMinimum",
                                        "exclusiveMaximum",
                                        "default",
                                        "deprecated",
                                        "writeOnly",
                                        NULL};
    static const char* const k_num[] = {"type",       "minimum",   "maximum", "default",
                                        "deprecated", "writeOnly", NULL};
    static const char* const k_str[] = {"type",    "minLength",  "maxLength", "enum",
                                        "default", "deprecated", "writeOnly", NULL};
    static const char* const k_bool[] = {"type", "default", "deprecated", "writeOnly",
                                         NULL};
    const snode*             type     = sn_child(s, "type");
    const snode*             c;

    if (sn_text(type, "integer")) {
        cf_i64  lo = CF_I64_MIN, hi = CF_I64_MAX, v;
        cf_bool any = CF_FALSE;
        d->type     = CF_TYPE_INT;
        if (!sw_known(w, s, k_int)) {
            return CF_FALSE;
        }
        c = sn_child(s, "minimum");
        if (c != NULL) {
            if (!sw_int(c, &v)) {
                return sw_fail(w, c, "minimum of an integer is an integer", NULL, 0);
            }
            lo  = v;
            any = CF_TRUE;
        }
        c = sn_child(s, "exclusiveMinimum");
        if (c != NULL) {
            if (!sw_int(c, &v) || v == CF_I64_MAX) {
                return sw_fail(w, c, "exclusiveMinimum of an integer is an integer",
                               NULL, 0);
            }
            lo  = (v + 1 > lo) ? v + 1 : lo;
            any = CF_TRUE;
        }
        c = sn_child(s, "maximum");
        if (c != NULL) {
            if (!sw_int(c, &v)) {
                return sw_fail(w, c, "maximum of an integer is an integer", NULL, 0);
            }
            hi  = v;
            any = CF_TRUE;
        }
        c = sn_child(s, "exclusiveMaximum");
        if (c != NULL) {
            if (!sw_int(c, &v) || v == CF_I64_MIN) {
                return sw_fail(w, c, "exclusiveMaximum of an integer is an integer",
                               NULL, 0);
            }
            hi  = (v - 1 < hi) ? v - 1 : hi;
            any = CF_TRUE;
        }
        if (any) {
            d->has_range = CF_TRUE;
            d->imin      = lo;
            d->imax      = hi;
        }
        return CF_TRUE;
    }
    if (sn_text(type, "number")) {
#if CF_WITH_REAL
        double lo = -1.7976931348623157e308, hi = 1.7976931348623157e308;
        d->type = CF_TYPE_REAL;
        if (!sw_known(w, s, k_num)) {
            return CF_FALSE;
        }
        c = sn_child(s, "minimum");
        if (c != NULL && !sw_real(c, &lo)) {
            return sw_fail(w, c, "minimum of a number is a number", NULL, 0);
        }
        c = sn_child(s, "maximum");
        if (c != NULL && !sw_real(c, &hi)) {
            return sw_fail(w, c, "maximum of a number is a number", NULL, 0);
        }
        if (sn_child(s, "minimum") != NULL || sn_child(s, "maximum") != NULL) {
            d->has_range = CF_TRUE;
            d->rmin      = lo;
            d->rmax      = hi;
        }
        return CF_TRUE;
#else
        (void)k_num;
        return sw_fail(w, type, "a number needs a build with CF_WITH_REAL", NULL, 0);
#endif
    }
    if (sn_text(type, "string")) {
        cf_i64 v;
        d->type = CF_TYPE_TEXT;
        if (!sw_known(w, s, k_str)) {
            return CF_FALSE;
        }
        c = sn_child(s, "minLength");
        if (c != NULL) {
            if (!sw_int(c, &v) || v < 0) {
                return sw_fail(w, c, "minLength is a count", NULL, 0);
            }
            d->min_len = (size_t)v;
        }
        c = sn_child(s, "maxLength");
        if (c != NULL) {
            if (!sw_int(c, &v) || v < 0) {
                return sw_fail(w, c, "maxLength is a count", NULL, 0);
            }
            d->max_len = (size_t)v;
        }
        c = sn_child(s, "enum");
        if (c != NULL) {
            const snode* e;
            size_t       n = 0, i = 0;
            const char** list;
            if (c->kind != SN_ARRAY) {
                return sw_fail(w, c, "enum is an array", NULL, 0);
            }
            for (e = c->first; e != NULL; e = e->next) {
                if (e->kind != SN_SCALAR || e->value.type != CF_TYPE_TEXT) {
                    return sw_fail(w, e, "enum of a string holds strings", NULL, 0);
                }
                ++n;
            }
            list = (const char**)cf_arena_alloc(w->ar, (n + 1) * sizeof *list);
            if (list == NULL) {
                return cf_error_set(w->err, CF_E_MEMORY, NULL, w->file,
                                    "no room for an enum");
            }
            for (e = c->first; e != NULL; e = e->next) {
                list[i++] = sw_dup(w, e);
            }
            list[n]    = NULL;
            d->allowed = list;
        }
        return CF_TRUE;
    }
    if (sn_text(type, "boolean")) {
        d->type = CF_TYPE_BOOL;
        return sw_known(w, s, k_bool);
    }
    if (type == NULL) {
        return sw_fail(w, s, "has no type; a definition here has one", s->name,
                       s->nlen);
    }
    return sw_fail(
        w, type,
        "is not a type this library reads: string, integer, number, boolean, "
        "array or object",
        NULL, 0);
}

/* A default for the scalar type in d, from a schema's value. */
static cf_bool sw_value(const swalk* w, const snode* s, cf_type type, cf_value* out) {
    if (s->kind != SN_SCALAR) {
        return sw_fail(w, s, "the default is not a value of its type", NULL, 0);
    }
    switch (type) {
    case CF_TYPE_INT:
        if (s->value.type == CF_TYPE_INT) {
            *out = s->value;
            return CF_TRUE;
        }
        break;
    case CF_TYPE_REAL:
#if CF_WITH_REAL
        if (s->value.type == CF_TYPE_REAL || s->value.type == CF_TYPE_INT) {
            cf_value_real(out, s->value.type == CF_TYPE_INT ? (double)s->value.u.i
                                                            : s->value.u.r);
            return CF_TRUE;
        }
#endif
        break;
    case CF_TYPE_TEXT:
    case CF_TYPE_BOOL:
        if (s->value.type == type) {
            *out = s->value;
            return CF_TRUE;
        }
        break;
    case CF_TYPE_NULL:
    default:
        break;
    }
    return sw_fail(w, s, "the default is not a value of its type", NULL, 0);
}

/* A property that is a value or a list of values. */
static cf_bool sw_leaf(const swalk* w, const snode* s, const char* key,
                       cf_bool required) {
    static const char* const k_arr[] = {"type",       "items",     "default",
                                        "deprecated", "writeOnly", NULL};
    /* Every keyword a leaf of any type may have: one outside these is
       refused by its own name before the type is looked at, so "$ref"
       is called "$ref" and not "no type". */
    static const char* const k_any[] = {"type",
                                        "items",
                                        "default",
                                        "deprecated",
                                        "writeOnly",
                                        "minimum",
                                        "maximum",
                                        "exclusiveMinimum",
                                        "exclusiveMaximum",
                                        "minLength",
                                        "maxLength",
                                        "enum",
                                        NULL};
    cf_def                   d;
    const snode*             c;
    const snode*             desc = sn_child(s, "description");
    const snode*             body = s;

    if (!sw_known(w, s, k_any)) {
        return CF_FALSE;
    }
    cf_def_init(&d, CF_TYPE_TEXT, key);
    if (sn_text(sn_child(s, "type"), "array")) {
        if (!sw_known(w, s, k_arr)) {
            return CF_FALSE;
        }
        body = sn_child(s, "items");
        if (body == NULL || body->kind != SN_OBJECT) {
            return sw_fail(w, s, "is an array without one schema for its items",
                           s->name, s->nlen);
        }
        if (sn_text(sn_child(body, "type"), "array") ||
            sn_text(sn_child(body, "type"), "object")) {
            return sw_fail(w, body,
                           "is a list of lists or of tables, which a definition cannot "
                           "describe; each element is its own key",
                           s->name, s->nlen);
        }
        d.list_mode = CF_LIST_REPLACE;
    }
    if (!sw_scalar(w, body, &d)) {
        return CF_FALSE;
    }
    if (body != s &&
        (sn_child(body, "default") != NULL || sn_child(body, "deprecated") != NULL ||
         sn_child(body, "writeOnly") != NULL)) {
        return sw_fail(w, body, "an item's default and flags belong to its array", NULL,
                       0);
    }
    if (desc == NULL) {
        desc = sn_child(s, "title");
    }
    d.description = sw_dup(w, desc);
    if (sn_true(s, "deprecated")) {
        d.flags |= CF_DEPRECATED;
    }
    if (sn_true(s, "writeOnly")) {
        d.flags |= CF_SENSITIVE;
    }
    if (required) {
        d.flags |= CF_REQUIRED;
    }
    c = sn_child(s, "default");
    if (c != NULL) {
        if (d.list_mode == CF_LIST_NONE) {
            if (!sw_value(w, c, d.type, &d.default_value)) {
                return CF_FALSE;
            }
        } else {
            const snode* e;
            size_t       n = 0, i = 0;
            cf_value*    vals;
            if (c->kind != SN_ARRAY) {
                return sw_fail(w, c, "the default of an array is an array", NULL, 0);
            }
            for (e = c->first; e != NULL; e = e->next) {
                ++n;
            }
            vals = (cf_value*)cf_arena_calloc(w->ar, (n + 1) * sizeof *vals);
            if (vals == NULL) {
                return cf_error_set(w->err, CF_E_MEMORY, NULL, w->file,
                                    "no room for a default");
            }
            for (e = c->first; e != NULL; e = e->next) {
                if (!sw_value(w, e, d.type, &vals[i++])) {
                    return CF_FALSE;
                }
            }
            d.list_defaults      = vals;
            d.list_default_count = n;
        }
        d.has_default = CF_TRUE;
    }
    if (!cf_define(w->ds, &d, w->err)) {
        /* The definition's own refusal, with the schema's place first. */
        char why[CF_MESSAGE_MAX];
        memcpy(why, w->err->message, sizeof why);
        why[sizeof why - 1] = '\0';
        return sw_fail(w, s, why, NULL, 0);
    }
    return CF_TRUE;
}

/* "required": the names of the properties that must be given. */
static cf_bool sw_required(const snode* obj, const char* name, size_t n) {
    const snode* r = sn_child(obj, "required");
    const snode* e;
    if (r == NULL || r->kind != SN_ARRAY) {
        return CF_FALSE;
    }
    for (e = r->first; e != NULL; e = e->next) {
        if (e->kind == SN_SCALAR && e->value.type == CF_TYPE_TEXT &&
            e->value.u.t.n == n && memcmp(e->value.u.t.p, name, n) == 0) {
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}

static cf_bool sw_object(const swalk* w, const snode* s, char* key, size_t klen,
                         int depth) {
    static const char* const k_root[] = {
        "$schema", "$id", "type", "properties", "required", "additionalProperties",
        NULL};
    static const char* const k_obj[] = {
        "type", "properties", "required", "additionalProperties", "deprecated", NULL};
    const snode* props = sn_child(s, "properties");
    const snode* extra = sn_child(s, "additionalProperties");
    const snode* req   = sn_child(s, "required");
    const snode* type  = sn_child(s, "type");
    const snode* p;

    if (depth >= CF_MAX_DEPTH) {
        return sw_fail(w, s, "the schema nests deeper than this build's CF_MAX_DEPTH",
                       NULL, 0);
    }
    if (!sw_known(w, s, (depth == 0) ? k_root : k_obj)) {
        return CF_FALSE;
    }
    if (type != NULL && !sn_text(type, "object")) {
        return sw_fail(w, type, "a schema with properties is an object", NULL, 0);
    }
    if (req != NULL) {
        const snode* e;
        if (req->kind != SN_ARRAY) {
            return sw_fail(w, req, "required is an array of names", NULL, 0);
        }
        for (e = req->first; e != NULL; e = e->next) {
            if (e->kind != SN_SCALAR || e->value.type != CF_TYPE_TEXT) {
                return sw_fail(w, e, "required holds names", NULL, 0);
            }
        }
    }
    if (extra != NULL) {
        if (extra->kind == SN_SCALAR && extra->value.type == CF_TYPE_BOOL) {
            if (extra->value.u.b && klen > 0 && !cf_defset_open(w->ds, key, w->err)) {
                return CF_FALSE;
            }
        } else {
            return sw_fail(w, extra,
                           "additionalProperties is true or false here; a schema for "
                           "the keys a definition does not name is not read",
                           NULL, 0);
        }
    }
    if (props == NULL) {
        return CF_TRUE;
    }
    if (props->kind != SN_OBJECT) {
        return sw_fail(w, props, "properties is an object", NULL, 0);
    }
    for (p = props->first; p != NULL; p = p->next) {
        size_t n = klen + (klen > 0 ? 1u : 0u) + p->nlen;
        if (p->kind != SN_OBJECT) {
            return sw_fail(w, p, "is a property whose schema is not an object", p->name,
                           p->nlen);
        }
        if (n >= CF_KEY_MAX) {
            return sw_fail(w, p, "makes a key longer than CF_KEY_MAX", p->name,
                           p->nlen);
        }
        if (klen > 0) {
            key[klen] = '.';
        }
        memcpy(key + klen + (klen > 0 ? 1u : 0u), p->name, p->nlen);
        key[n] = '\0';
        if (sn_text(sn_child(p, "type"), "object") ||
            sn_child(p, "properties") != NULL) {
            if (!sw_object(w, p, key, n, depth + 1)) {
                return CF_FALSE;
            }
        } else if (!sw_leaf(w, p, key, sw_required(s, p->name, p->nlen))) {
            return CF_FALSE;
        }
        key[klen] = '\0';
    }
    /* A name in "required" that is no property here is a mistake in the
       schema, said rather than ignored. */
    if (req != NULL) {
        const snode* e;
        for (e = req->first; e != NULL; e = e->next) {
            for (p = props->first; p != NULL; p = p->next) {
                if (p->nlen == e->value.u.t.n &&
                    memcmp(p->name, e->value.u.t.p, p->nlen) == 0) {
                    break;
                }
            }
            if (p == NULL) {
                return sw_fail(w, e, "is required and is not a property",
                               e->value.u.t.p, e->value.u.t.n);
            }
        }
    }
    return CF_TRUE;
}

cf_bool cf_defset_schema(cf_defset* ds, const char* text, size_t n, const char* name,
                         cf_error* err) {
    char            key[CF_KEY_MAX]; /* the key being built, a level at a time */
    const cf_codec* json = cf_codec_by_name("json");
    cf_doc_sink     sink;
    stree           t;
    swalk           w;
    cf_blob         blob;
    const snode*    ver;

    if (ds == NULL || text == NULL || json == NULL) {
        return cf_error_set(err, CF_E_DEFINITION, NULL, name, "no schema to read");
    }
    memset(&t, 0, sizeof t);
    t.ar = ds->arena;
    memset(&sink, 0, sizeof sink);
    sink.size         = sizeof sink;
    sink.begin_table  = st_table;
    sink.begin_array  = st_array;
    sink.scalar       = st_scalar;
    sink.end          = st_end;
    sink.ctx          = &t;
    sink.reserve_text = st_text;
    cf_blob_mem(&blob, text, n, (name != NULL) ? name : "<schema>");
    if (!json->decode(&blob, &sink, err)) {
        return CF_FALSE;
    }
    if (t.failed || t.root == NULL) {
        return cf_error_set(err, CF_E_MEMORY, NULL, name, "no room to read the schema");
    }
    w.ds   = ds;
    w.ar   = ds->arena;
    w.err  = err;
    w.file = blob.name;
    ver    = sn_child(t.root, "$schema");
    if (ver != NULL && !sn_text(ver, "https://json-schema.org/draft/2020-12/schema") &&
        !sn_text(ver, "https://json-schema.org/draft/2019-09/schema")) {
        return sw_fail(&w, ver,
                       "is a JSON Schema draft this library does not read; it reads "
                       "2020-12 and 2019-09",
                       NULL, 0);
    }
    key[0] = '\0';
    return sw_object(&w, t.root, key, 0, 0);
}

/* The file's bytes in an arena of their own when the definitions' arena
   has an allocator to take one from, and freed after; in a fixed arena,
   in that arena, which bounds them with everything else. */
cf_bool cf_defset_schema_file(cf_defset* ds, const char* path, cf_error* err) {
    const cf_allocator* a;
    cf_arena*           ar;
    cf_blob             blob;
    cf_bool             ok;

    if (ds == NULL || path == NULL) {
        return cf_error_set(err, CF_E_DEFINITION, NULL, path, "no schema to read");
    }
    a  = cf_arena_allocator(ds->arena);
    ar = (a != NULL) ? cf_arena_new(a, 0) : ds->arena;
    if (ar == NULL) {
        return cf_error_set(err, CF_E_MEMORY, NULL, path, "no room to read the schema");
    }
    ok = cf_blob_file(&blob, ar, path, err) &&
         cf_defset_schema(ds, blob.p, blob.n, path, err);
    if (ar != ds->arena) {
        cf_arena_free(ar);
    }
    return ok;
}

#else /* !CF_WITH_JSON */

cf_bool cf_defset_schema(cf_defset* ds, const char* text, size_t n, const char* name,
                         cf_error* err) {
    (void)ds;
    (void)text;
    (void)n;
    return cf_error_set(err, CF_E_CAPABILITY, NULL, name,
                        "a JSON Schema is read by the JSON reader, and this build has "
                        "none (CF_WITH_JSON=OFF)");
}

cf_bool cf_defset_schema_file(cf_defset* ds, const char* path, cf_error* err) {
    return cf_defset_schema(ds, NULL, 0, path, err);
}

#endif /* CF_WITH_JSON */
