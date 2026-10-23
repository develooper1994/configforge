/*
 * explain.c - rendering the provenance chain. C89.
 */

#include "configforge/cf_explain.h"

#include <string.h>

#define CF_COL_SOURCE 12
#define CF_COL_VALUE 22

static const cf_source* find_source(const cf_source_view* sv, int rank) {
    size_t i;

    if (sv == NULL) {
        return NULL;
    }
    for (i = 0; i < sv->count; ++i) {
        if (sv->items[i] != NULL && sv->items[i]->rank == rank) {
            return sv->items[i];
        }
    }
    return NULL;
}

static size_t pad(char* buf, size_t cap, size_t at, size_t to) {
    while (at < to) {
        if (at + 1 < cap) {
            buf[at] = ' ';
        }
        ++at;
    }
    return at;
}

static size_t explain_value(char* buf, size_t cap, size_t at, const cf_value* v,
                            cf_bool secret) {
    char   tmp[CF_KEY_MAX];
    size_t n;

    if (secret) {
        return (v->type == CF_TYPE_TEXT)
                   ? cf_fmt_append(buf, cap, at, "<sensitive, %zu bytes>", v->u.t.n)
                   : cf_fmt_append(buf, cap, at, "<sensitive>");
    }
    n = cf_value_to_text(v, tmp, sizeof tmp);

    if (n == 0 && v->type != CF_TYPE_TEXT) {
        return cf_fmt_append(buf, cap, at, "<unprintable>");
    }
    if (v->type == CF_TYPE_TEXT) {
        return cf_fmt_append(buf, cap, at, "\"%s\"", tmp);
    }
    return cf_fmt_append(buf, cap, at, "%s", tmp);
}

static cf_bool is_secret(const cf_defset* ds, const cf_entry* e) {
    const cf_def* d = (ds != NULL) ? cf_defset_find(ds, e->key, e->keylen) : NULL;
    return (d != NULL && (d->flags & CF_SENSITIVE) != 0) ? CF_TRUE : CF_FALSE;
}

static size_t explain_entry(const cf_entry* e, const cf_source_view* sv,
                            const cf_defset* ds, char* buf, size_t cap, size_t at) {
    const cf_candidate* c;
    size_t              line;
    cf_bool             secret = is_secret(ds, e);

    at = cf_fmt_append(buf, cap, at, "%s", e->key);
    if (e->selected != NULL) {
        at = cf_fmt_append(buf, cap, at, " = ");
        at = explain_value(buf, cap, at, &e->selected->value, secret);
        at = cf_fmt_append(buf, cap, at, "  (%s)\n",
                           cf_type_name(e->selected->value.type));
    } else {
        at = cf_fmt_append(buf, cap, at, " = <no value>\n");
    }

    for (c = e->chain; c != NULL; c = c->next) {
        const cf_source* s = find_source(sv, c->source);

        at   = cf_fmt_append(buf, cap, at, "  ");
        line = at;
        at   = cf_fmt_append(buf, cap, at, "%s", (s != NULL) ? s->name : "?");
        at   = pad(buf, cap, at, line + CF_COL_SOURCE);

        line = at;
        at   = explain_value(buf, cap, at, &c->value, secret);
        at   = pad(buf, cap, at, line + CF_COL_VALUE);

        at = cf_fmt_append(buf, cap, at, "%s", cf_outcome_name(c->outcome));
        if (c->reason != NULL) {
            at = cf_fmt_append(buf, cap, at, ": %s", c->reason);
        }
        if (c->at != NULL) {
            at = cf_fmt_append(buf, cap, at, "   %s", c->at);
        }
        at = cf_fmt_append(buf, cap, at, "\n");
    }

    /* Sources with no candidate for this key are listed too. "cli: not
       present" is how a reader learns which option they could pass,
       and a chain that only shows what exists cannot answer "why is
       my flag not taking effect". */
    if (sv != NULL) {
        size_t j;
        for (j = 0; j < sv->count; ++j) {
            const cf_source* s       = sv->items[j];
            int              present = 0;

            if (s == NULL) {
                continue;
            }
            for (c = e->chain; c != NULL; c = c->next) {
                if (c->source == s->rank) {
                    present = 1;
                    break;
                }
            }
            if (!present) {
                at   = cf_fmt_append(buf, cap, at, "  ");
                line = at;
                at   = cf_fmt_append(buf, cap, at, "%s", s->name);
                at   = pad(buf, cap, at, line + CF_COL_SOURCE);
                at   = cf_fmt_append(buf, cap, at, "not present\n");
            }
        }
    }
    return at;
}

size_t cf_ks_explain(const cf_keyspace* ks, const cf_source_view* sv, const char* key,
                     size_t keylen, char* buf, size_t cap) {
    return cf_ks_explain_redacted(ks, sv, NULL, key, keylen, buf, cap);
}

size_t cf_ks_inspect(const cf_keyspace* ks, const cf_source_view* sv, char* buf,
                     size_t cap) {
    return cf_ks_inspect_redacted(ks, sv, NULL, buf, cap);
}

size_t cf_ks_explain_redacted(const cf_keyspace* ks, const cf_source_view* sv,
                              const cf_defset* ds, const char* key, size_t keylen,
                              char* buf, size_t cap) {
    const cf_entry* e = cf_ks_find(ks, key, keylen);
    size_t          at;

    /* No buffer is measuring, whatever the capacity says. */
    if (buf == NULL) {
        cap = 0;
    }
    if (buf != NULL && cap > 0) {
        buf[0] = '\0';
    }
    if (e == NULL) {
        return 0;
    }
    at = explain_entry(e, sv, ds, buf, cap, 0);
    if (buf != NULL && cap > 0) {
        buf[(at < cap) ? at : cap - 1] = '\0';
    }
    return at;
}

size_t cf_ks_inspect_redacted(const cf_keyspace* ks, const cf_source_view* sv,
                              const cf_defset* ds, char* buf, size_t cap) {
    size_t at = 0;
    size_t i, j;

    if (buf == NULL) {
        cap = 0;
    }
    if (buf != NULL && cap > 0) {
        buf[0] = '\0';
    }
    if (ks == NULL) {
        return 0;
    }
    for (i = 0; i < cf_ks_count(ks); ++i) {
        at = explain_entry(cf_ks_at(ks, i), sv, ds, buf, cap, at);
    }

    /* Two different findings, and conflating them wastes the more
       important one.

       A source that supplied nothing is usually just empty: a command
       line with no options, or a file that does not exist. Worth a
       quiet line, because it is also what a misspelled environment
       prefix looks like.

       A source that supplied values and won none of them is the
       dangerous case. Someone is editing a file that has no effect,
       and nothing else in a configuration system would ever say so. */
    if (sv != NULL && sv->count > 0) {
        unsigned short supplied[CF_MAX_SOURCES];
        unsigned short wins[CF_MAX_SOURCES];
        size_t         nsrc = (sv->count < CF_MAX_SOURCES) ? sv->count : CF_MAX_SOURCES;

        memset(supplied, 0, nsrc * sizeof supplied[0]);
        memset(wins, 0, nsrc * sizeof wins[0]);

        for (i = 0; i < cf_ks_count(ks); ++i) {
            const cf_entry*     e = cf_ks_at(ks, i);
            const cf_candidate* c;
            for (c = e->chain; c != NULL; c = c->next) {
                for (j = 0; j < nsrc; ++j) {
                    if (sv->items[j] != NULL && sv->items[j]->rank == c->source) {
                        if (supplied[j] < 0xFFFF) {
                            ++supplied[j];
                        }
                        if (c->outcome == CF_OUT_SELECTED && wins[j] < 0xFFFF) {
                            ++wins[j];
                        }
                        break;
                    }
                }
            }
        }
        for (j = 0; j < nsrc; ++j) {
            if (sv->items[j] == NULL) {
                continue;
            }
            if (supplied[j] == 0) {
                at = cf_fmt_append(buf, cap, at, "note: source '%s' supplied nothing\n",
                                   sv->items[j]->name);
            } else if (wins[j] == 0) {
                at = cf_fmt_append(buf, cap, at,
                                   "note: source '%s' supplied %u values and "
                                   "won none; every one is shadowed\n",
                                   sv->items[j]->name, (unsigned)supplied[j]);
            }
        }
    }

    if (buf != NULL && cap > 0) {
        buf[(at < cap) ? at : cap - 1] = '\0';
    }
    return at;
}
