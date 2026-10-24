/*
 * diff.c - what changed between two configurations. C89.
 *
 * The question after a reload, and the one a log line should answer
 * without anyone reading two dumps side by side: which keys changed,
 * from what to what, and where the new value came from. Both key spaces
 * are sorted, so it is one pass over each.
 *
 * A value that stayed the same but now comes from somewhere else is a
 * change too. The configuration means the same thing and its provenance
 * does not, and provenance is what this library is for: a port that was
 * the default and is now written in a file is one edit away from being
 * different, and that file is the one to look at.
 */

#include "configforge/configforge.h"

#include <string.h>

static cf_bool diff_same_origin(const cf_candidate* x, const cf_candidate* y) {
    if (x->source != y->source) {
        return CF_FALSE;
    }
    if (x->at == NULL || y->at == NULL) {
        return (x->at == NULL && y->at == NULL) ? CF_TRUE : CF_FALSE;
    }
    return (strcmp(x->at, y->at) == 0) ? CF_TRUE : CF_FALSE;
}

size_t cf_diff(const cf_config* before, const cf_config* after, cf_diff_fn fn,
               void* ctx) {
    const cf_config*   a = before;
    const cf_config*   b = after;
    const cf_keyspace* ka;
    const cf_keyspace* kb;
    size_t             i = 0, j = 0, na, nb, changes = 0;

    if (a == NULL || b == NULL) {
        return 0;
    }
    ka = cf_keys(a);
    kb = cf_keys(b);
    na = cf_ks_count(ka);
    nb = cf_ks_count(kb);

    while (i < na || j < nb) {
        const cf_entry*     ea = (i < na) ? cf_ks_at(ka, i) : NULL;
        const cf_entry*     eb = (j < nb) ? cf_ks_at(kb, j) : NULL;
        const cf_entry*     e;
        const cf_candidate* x = NULL;
        const cf_candidate* y = NULL;
        cf_change           kind;
        int                 cmp = 0;

        /* Each branch tests the pointer it uses, so that what the loop
           condition guarantees is visible to -Wnull-dereference too. */
        if (ea != NULL && eb != NULL) {
            cmp = cf_cmp_bytes(ea->key, ea->keylen, eb->key, eb->keylen);
        }
        if (ea != NULL && (eb == NULL || cmp < 0)) {
            e = ea;
            x = ea->selected;
            ++i;
        } else if (eb != NULL && (ea == NULL || cmp > 0)) {
            e = eb;
            y = eb->selected;
            ++j;
        } else if (ea != NULL && eb != NULL) {
            e = ea;
            x = ea->selected;
            y = eb->selected;
            ++i;
            ++j;
        } else {
            break;
        }

        if (x == NULL) {
            if (y == NULL) {
                continue; /* a key with no value on either side */
            }
            kind = CF_CHANGE_ADDED;
        } else if (y == NULL) {
            kind = CF_CHANGE_REMOVED;
        } else if (!cf_value_equal(&x->value, &y->value)) {
            kind = CF_CHANGE_VALUE;
        } else if (!diff_same_origin(x, y)) {
            kind = CF_CHANGE_ORIGIN;
        } else {
            continue;
        }
        ++changes;
        if (fn != NULL && !fn(ctx, e->key, e->keylen, kind, x, y)) {
            break;
        }
    }
    return changes;
}

/* ------------------------------------------------------------------ *
 * As text
 * ------------------------------------------------------------------ */

typedef struct diff_text {
    char*            buf;
    size_t           cap;
    size_t           at;
    const cf_config* a;
    const cf_config* b;
} diff_text;

static const char* diff_source_name(const cf_config* c, int rank) {
    const cf_source_view* sv = cf_sources(c);
    size_t                i;

    for (i = 0; sv != NULL && i < sv->count; ++i) {
        if (sv->items[i] != NULL && sv->items[i]->rank == rank) {
            return sv->items[i]->name;
        }
    }
    return "?";
}

/* As explain() shows a value, and redacted as explain() redacts it: a
   reload log is a diagnostic like any other. */
static void diff_put_value(diff_text* t, const cf_value* v, cf_bool secret) {
    char   tmp[CF_KEY_MAX];
    size_t n;

    if (secret) {
        t->at = (v->type == CF_TYPE_TEXT)
                    ? cf_fmt_append(t->buf, t->cap, t->at, "<sensitive, %zu bytes>",
                                    v->u.t.n)
                    : cf_fmt_append(t->buf, t->cap, t->at, "<sensitive>");
        return;
    }
    n = cf_value_to_text(v, tmp, sizeof tmp);
    if (n == 0 && v->type != CF_TYPE_TEXT) {
        t->at = cf_fmt_append(t->buf, t->cap, t->at, "<unprintable>");
    } else if (v->type == CF_TYPE_TEXT) {
        t->at = cf_fmt_append(t->buf, t->cap, t->at, "\"%s\"", tmp);
    } else {
        t->at = cf_fmt_append(t->buf, t->cap, t->at, "%s", tmp);
    }
}

static void diff_put_origin(diff_text* t, const cf_config* c, const cf_candidate* k) {
    t->at = cf_fmt_append(t->buf, t->cap, t->at, "%s", diff_source_name(c, k->source));
    if (k->at != NULL) {
        t->at = cf_fmt_append(t->buf, t->cap, t->at, " %s", k->at);
    }
}

static cf_bool diff_line(void* ctx, const char* key, size_t klen, cf_change kind,
                         const cf_candidate* before, const cf_candidate* after) {
    diff_text*    t = (diff_text*)ctx;
    const cf_def* d =
        cf_defset_find(cf_defs(kind == CF_CHANGE_REMOVED ? t->a : t->b), key, klen);
    cf_bool secret = (d != NULL && (d->flags & CF_SENSITIVE) != 0) ? CF_TRUE : CF_FALSE;

    t->at = cf_fmt_append(t->buf, t->cap, t->at, "%S: ", key, klen);
    switch (kind) {
    case CF_CHANGE_ADDED:
        t->at = cf_fmt_append(t->buf, t->cap, t->at, "added, ");
        diff_put_value(t, &after->value, secret);
        t->at = cf_fmt_append(t->buf, t->cap, t->at, " (");
        diff_put_origin(t, t->b, after);
        t->at = cf_fmt_append(t->buf, t->cap, t->at, ")");
        break;
    case CF_CHANGE_REMOVED:
        t->at = cf_fmt_append(t->buf, t->cap, t->at, "removed, was ");
        diff_put_value(t, &before->value, secret);
        t->at = cf_fmt_append(t->buf, t->cap, t->at, " (");
        diff_put_origin(t, t->a, before);
        t->at = cf_fmt_append(t->buf, t->cap, t->at, ")");
        break;
    case CF_CHANGE_VALUE:
        if (secret) {
            /* Not even two lengths: a changed length is a fact about
               the secret, and "changed" is all a log needs. */
            t->at = cf_fmt_append(t->buf, t->cap, t->at, "changed, <sensitive>");
        } else {
            diff_put_value(t, &before->value, CF_FALSE);
            t->at = cf_fmt_append(t->buf, t->cap, t->at, " -> ");
            diff_put_value(t, &after->value, CF_FALSE);
        }
        t->at = cf_fmt_append(t->buf, t->cap, t->at, " (");
        diff_put_origin(t, t->b, after);
        t->at = cf_fmt_append(t->buf, t->cap, t->at, ")");
        break;
    case CF_CHANGE_ORIGIN:
        diff_put_value(t, &after->value, secret);
        t->at = cf_fmt_append(t->buf, t->cap, t->at, ", now from ");
        diff_put_origin(t, t->b, after);
        t->at = cf_fmt_append(t->buf, t->cap, t->at, ", was from ");
        diff_put_origin(t, t->a, before);
        break;
    }
    t->at = cf_fmt_append(t->buf, t->cap, t->at, "\n");
    return CF_TRUE;
}

size_t cf_diff_text(const cf_config* before, const cf_config* after, char* buf,
                    size_t cap) {
    const cf_config* a = before;
    const cf_config* b = after;
    diff_text        t;

    /* No buffer is measuring, whatever the capacity says. */
    if (buf == NULL) {
        cap = 0;
    }
    if (cap > 0) {
        buf[0] = '\0';
    }
    t.buf = buf;
    t.cap = cap;
    t.at  = 0;
    t.a   = a;
    t.b   = b;
    (void)cf_diff(a, b, diff_line, &t);
    if (cap > 0) {
        buf[(t.at < cap) ? t.at : cap - 1] = '\0';
    }
    return t.at;
}
