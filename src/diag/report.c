/*
 * report.c - the provenance report, written and replayed. C89.
 *
 * The report is not a diagnostic dump. It is a complete serialisation
 * of the key space, which is what lets the same artifact serve three
 * consumers: Doctor compares two of them, a foreign adapter produces
 * one, and the library itself reads one back as a source.
 *
 * That third use is the reason the schema is worth this much care. A
 * dump could be regenerated when its shape changed; a source format
 * cannot.
 */

#include "configforge/cf_platform.h"
#include "configforge/cf_report.h"
#include "configforge/cf_version.h"

#include <string.h>

/* ------------------------------------------------------------------ *
 * Writing
 *
 * JSON is emitted directly rather than through the codec's event
 * encoder, because a report is not the document the key space
 * describes: it is a description *of* the key space, with a candidate
 * chain under every key. Routing it through cf_doc_emit would mean
 * inventing a document whose shape happens to match the schema, and
 * then keeping two things in step forever.
 * ------------------------------------------------------------------ */

static void report_quote(cf_sink* out, const char* p, size_t n) {
    size_t i;

    cf_sink_putc(out, '"');
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)p[i];
        switch (c) {
        case '"':
            cf_sink_puts(out, "\\\"");
            break;
        case '\\':
            cf_sink_puts(out, "\\\\");
            break;
        case '\b':
            cf_sink_puts(out, "\\b");
            break;
        case '\f':
            cf_sink_puts(out, "\\f");
            break;
        case '\n':
            cf_sink_puts(out, "\\n");
            break;
        case '\r':
            cf_sink_puts(out, "\\r");
            break;
        case '\t':
            cf_sink_puts(out, "\\t");
            break;
        default:
            if (c < 0x20u) {
                cf_sink_fmt(out, "\\u00%c%c", "0123456789abcdef"[(c >> 4) & 0xF],
                            "0123456789abcdef"[c & 0xF]);
            } else {
                cf_sink_putc(out, (char)c);
            }
            break;
        }
    }
    cf_sink_putc(out, '"');
}

static void report_quote_z(cf_sink* out, const char* z) {
    if (z == NULL) {
        cf_sink_puts(out, "null");
    } else {
        report_quote(out, z, strlen(z));
    }
}

static void report_value(cf_sink* out, const cf_value* v) {
    char tmp[CF_VALUE_TEXT_MAX];

    switch (v->type) {
    case CF_TYPE_TEXT:
        report_quote(out, v->u.t.p, v->u.t.n);
        break;
    case CF_TYPE_NULL:
        cf_sink_puts(out, "null");
        break;
    case CF_TYPE_BOOL:
        cf_sink_puts(out, v->u.b ? "true" : "false");
        break;
    case CF_TYPE_INT:
    case CF_TYPE_REAL:
        cf_sink_write(out, tmp, cf_value_to_text(v, tmp, sizeof tmp));
        break;
    }
}

/* FNV-1a over the value's canonical text. Not a cryptographic digest
   and not presented as one: its job is to answer "did this secret
   change" without revealing what it changed to, in a report that is
   meant to be safe to keep. */
static void report_digest(cf_sink* out, const cf_value* v) {
    char   tmp[CF_VALUE_TEXT_MAX];
    size_t n = cf_value_to_text(v, tmp, sizeof tmp);
    cf_u64 h = ((cf_u64)3421674724U << 32) | (cf_u64)2216829733U;
    size_t i;

    for (i = 0; i < n; ++i) {
        h ^= (cf_u64)(unsigned char)tmp[i];
        h *= (cf_u64)1099511628211U;
    }
    /* Sixteen hex digits written by hand. The library's formatter has
       no width specifiers on purpose, and reaching for "%08x" produced
       a digest that was the literal format string in every report -
       which the first test did not catch, because it only asserted the
       prefix was there. The digits are now built where their count
       cannot depend on a formatter feature. */
    {
        static const char hex[] = "0123456789abcdef";
        char              out16[16];
        int               k;
        for (k = 15; k >= 0; --k) {
            out16[k] = hex[(size_t)(h & (cf_u64)0xF)];
            h >>= 4;
        }
        cf_sink_puts(out, "\"fnv1a:");
        cf_sink_write(out, out16, sizeof out16);
        cf_sink_putc(out, '"');
    }
}

static const cf_source* source_of(const cf_source_view* sv, int rank) {
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

static void report_definition(cf_sink* out, const cf_def* d) {
    if (d == NULL) {
        cf_sink_puts(out, "null");
        return;
    }
    cf_sink_fmt(out, "{ \"type\": \"%s\"", cf_type_name(d->type));
    cf_sink_fmt(out, ", \"required\": %s",
                ((d->flags & CF_REQUIRED) != 0) ? "true" : "false");
    cf_sink_fmt(out, ", \"sensitive\": %s",
                ((d->flags & CF_SENSITIVE) != 0) ? "true" : "false");
    cf_sink_fmt(out, ", \"deprecated\": %s",
                ((d->flags & CF_DEPRECATED) != 0) ? "true" : "false");
    if (d->has_default) {
        cf_sink_puts(out, ", \"default\": ");
        report_value(out, &d->default_value);
    }
    if (d->has_range && d->type == CF_TYPE_INT) {
        cf_sink_fmt(out, ", \"min\": %I, \"max\": %I", d->imin, d->imax);
    }
    if (d->description != NULL) {
        cf_sink_puts(out, ", \"description\": ");
        report_quote_z(out, d->description);
    }
    if (d->policy != NULL) {
        /* The policy travels with the report so an audit can answer
           "what may set this key" without the program that wrote it. */
        int i;
        cf_sink_puts(out, ", \"policy\": [");
        for (i = 0; d->policy[i] != NULL; ++i) {
            if (i > 0) {
                cf_sink_puts(out, ", ");
            }
            report_quote_z(out, d->policy[i]);
        }
        cf_sink_putc(out, ']');
    }
    cf_sink_putc(out, '}');
}

/* The file a location names: "app.ini:14:3" is app.ini. Read from the
   right, so a drive letter keeps its colon. */
static cf_bool report_file_of(const char* at, char* out, size_t cap) {
    size_t n = strlen(at), cut = n, fields = 0;

    while (fields < 2) {
        size_t i = cut;
        while (i > 0 && cf_is_digit((unsigned char)at[i - 1])) {
            --i;
        }
        if (i == cut || i == 0 || at[i - 1] != ':') {
            break;
        }
        cut = i - 1;
        ++fields;
    }
    if (cut == 0 || cut + 1 > cap) {
        return CF_FALSE;
    }
    memcpy(out, at, cut);
    out[cut] = '\0';
    return CF_TRUE;
}

static void report_candidate(cf_sink* out, const cf_candidate* c,
                             const cf_source_view* sv, const cf_def* d,
                             cf_report_mode mode) {
    const cf_source* s = source_of(sv, c->source);
    cf_bool          redact =
        (mode == CF_REPORT_BLIND) && d != NULL && (d->flags & CF_SENSITIVE) != 0;

    cf_sink_puts(out, "{ \"source\": ");
    report_quote_z(out, (s != NULL) ? s->name : "?");
    cf_sink_puts(out, ", \"present\": true");

    if (redact) {
        /* Presence, origin, length and a digest. Every value-blind
           Doctor rule works on exactly this, which is how a production
           report can be audited without leaving the secret in it. */
        char   tmp[CF_VALUE_TEXT_MAX];
        size_t n = cf_value_to_text(&c->value, tmp, sizeof tmp);
        cf_sink_puts(out, ", \"redacted\": true");
        cf_sink_fmt(out, ", \"length\": %zu", n);
        cf_sink_puts(out, ", \"digest\": ");
        report_digest(out, &c->value);
    } else {
        cf_sink_puts(out, ", \"value\": ");
        report_value(out, &c->value);
    }
    if (c->at != NULL) {
        cf_sink_puts(out, ", \"at\": ");
        report_quote_z(out, c->at);
    }
    /* For a secret that came from a file: whether everyone on the
       machine can read that file. Recorded here because this is the
       only point where the file is at hand - Doctor reads reports from
       other machines and must never look at its own disk to judge them.
       Absent when the platform cannot say. */
    if (d != NULL && (d->flags & CF_SENSITIVE) != 0 && c->at != NULL && s != NULL &&
        s->vt != NULL &&
        (strcmp(s->vt->kind, "file") == 0 || strcmp(s->vt->kind, "dir") == 0 ||
         strcmp(s->vt->kind, "dotenv") == 0)) {
        char path[CF_PLATFORM_NAME_MAX];
        int  wr = report_file_of(c->at, path, sizeof path)
                      ? cf_platform_world_readable(path)
                      : -1;
        if (wr >= 0) {
            cf_sink_fmt(out, ", \"world_readable\": %s", (wr > 0) ? "true" : "false");
        }
    }
    if (c->outcome != CF_OUT_SELECTED) {
        cf_sink_puts(out, ", \"outcome\": ");
        report_quote_z(out, cf_outcome_name(c->outcome));
    }
    if (c->reason != NULL) {
        cf_sink_puts(out, ", \"reason\": ");
        report_quote_z(out, c->reason);
    }
    cf_sink_putc(out, '}');
}

size_t cf_report_write(const cf_keyspace* ks, const cf_defset* ds,
                       const cf_source_view* sv, const cf_report_info* info, char* buf,
                       size_t cap) {
    cf_sink        out;
    size_t         i;
    cf_report_mode mode = (info != NULL) ? info->mode : CF_REPORT_BLIND;

    cf_sink_init(&out, buf, cap);
    if (ks == NULL) {
        cf_sink_terminate(&out);
        return 0;
    }

    cf_sink_fmt(&out, "{\n  \"configforge_report\": %d,\n", CF_REPORT_SCHEMA);
    cf_sink_fmt(&out, "  \"mode\": \"%s\",\n",
                (mode == CF_REPORT_FULL) ? "full" : "blind");
    cf_sink_puts(&out, "  \"profile\": ");
    report_quote_z(&out, (info != NULL) ? info->profile : NULL);
    cf_sink_puts(&out, ",\n  \"application\": { \"name\": ");
    report_quote_z(&out, (info != NULL) ? info->application : NULL);
    cf_sink_puts(&out, ", \"version\": ");
    report_quote_z(&out, (info != NULL) ? info->app_version : NULL);
    cf_sink_puts(&out, " },\n");

    /* The compiled-in format set, so a report from a restricted build
       is distinguishable from one produced by a full build. Without it
       Doctor would read "this source was unreadable by that build" as
       real drift. */
    cf_sink_fmt(&out, "  \"library\": { \"version\": \"%s\", \"features\": [",
                cf_version());
    for (i = 0; i < cf_codec_count(); ++i) {
        if (i > 0) {
            cf_sink_puts(&out, ", ");
        }
        report_quote_z(&out, cf_codec_at(i)->name);
    }
    cf_sink_puts(&out, "] },\n  \"generated\": ");
    report_quote_z(&out, (info != NULL) ? info->generated : NULL);
    cf_sink_puts(&out, ",\n");

    cf_sink_puts(&out, "  \"sources\": [");
    if (sv != NULL) {
        for (i = 0; i < sv->count; ++i) {
            if (sv->items[i] == NULL) {
                continue;
            }
            if (i > 0) {
                cf_sink_puts(&out, ", ");
            }
            cf_sink_puts(&out, "{ \"name\": ");
            report_quote_z(&out, sv->items[i]->name);
            cf_sink_fmt(&out, ", \"kind\": \"%s\", \"rank\": %d }",
                        (sv->items[i]->vt != NULL) ? sv->items[i]->vt->kind : "?",
                        sv->items[i]->rank);
        }
    }
    cf_sink_puts(&out, "],\n  \"keys\": {");

    for (i = 0; i < cf_ks_count(ks); ++i) {
        const cf_entry* e = cf_ks_at(ks, i);
        const cf_def*   d = (ds != NULL) ? cf_defset_find(ds, e->key, e->keylen) : NULL;
        const cf_candidate* c;
        cf_bool             first = CF_TRUE;

        if (i > 0) {
            cf_sink_putc(&out, ',');
        }
        cf_sink_puts(&out, "\n    ");
        report_quote(&out, e->key, e->keylen);
        cf_sink_puts(&out, ": {\n      \"definition\": ");
        report_definition(&out, d);
        cf_sink_puts(&out, ",\n      \"candidates\": [");

        /* Nothing is dropped from the chain: a shadowed or refused
           candidate is the whole point of the artifact. */
        for (c = e->chain; c != NULL; c = c->next) {
            if (!first) {
                cf_sink_putc(&out, ',');
            }
            cf_sink_puts(&out, "\n        ");
            report_candidate(&out, c, sv, d, mode);
            first = CF_FALSE;
        }
        cf_sink_puts(&out, "\n      ],\n      \"selected\": ");
        if (e->selected != NULL) {
            const cf_source* s = source_of(sv, e->selected->source);
            cf_sink_puts(&out, "{ \"source\": ");
            report_quote_z(&out, (s != NULL) ? s->name : "?");
            cf_sink_puts(&out, ", \"reason\": \"highest precedence with a "
                               "usable value\" }");
        } else {
            cf_sink_puts(&out, "null");
        }
        cf_sink_puts(&out, "\n    }");
    }
    cf_sink_puts(&out, "\n  }\n}\n");
    cf_sink_terminate(&out);
    return out.len;
}

/* ================================================================== *
 * Report Replay
 * ================================================================== */

typedef struct replay_state {
    const char* path;
    cf_error    err;
    cf_bool     failed;
} replay_state;

/* The replay reader walks the report's own JSON with the ordinary
   codec and the ordinary flattener, into a scratch key space. What
   comes back is a flat map of the report's structure, which is then
   read for the selected value of every key. Parsing the report by hand
   would mean a second JSON reader that has to agree with the first. */
static cf_bool replay_load(const cf_source* s, cf_keyspace* ks) {
    replay_state*   st   = (replay_state*)s->ext;
    cf_arena*       ar   = cf_ks_arena(ks);
    const cf_codec* json = cf_codec_by_name("json");
    cf_keyspace     doc;
    cf_flatten      fl;
    cf_blob         blob;
    size_t          i;
    const cf_value* mode;

    st->failed = CF_FALSE;
    cf_error_clear(&st->err);

    if (json == NULL) {
        st->failed = CF_TRUE;
        return cf_error_set(&st->err, CF_E_CAPABILITY, NULL, st->path,
                            "replay needs JSON support; this build was "
                            "compiled without CF_WITH_JSON");
    }
    if (!cf_blob_file(&blob, ar, st->path, &st->err)) {
        st->failed = CF_TRUE;
        return CF_FALSE;
    }
    if (!cf_ks_init(&doc, ar, 256) || !cf_flatten_init(&fl, &doc, 0)) {
        st->failed = CF_TRUE;
        return cf_error_set(&st->err, CF_E_MEMORY, NULL, st->path,
                            "no room to read '%s'", st->path);
    }
    if (!json->decode(&blob, cf_flatten_sink(&fl), &st->err)) {
        st->failed = CF_TRUE;
        return CF_FALSE;
    }
    cf_ks_resolve(&doc);

    {
        const cf_value* schema = cf_ks_get(&doc, "configforge_report", 18);
        if (schema == NULL || schema->type != CF_TYPE_INT) {
            st->failed = CF_TRUE;
            return cf_error_set(&st->err, CF_E_SOURCE, NULL, st->path,
                                "'%s' is not a provenance report", st->path);
        }
        if (schema->u.i != CF_REPORT_SCHEMA) {
            st->failed = CF_TRUE;
            return cf_error_set(&st->err, CF_E_SOURCE, NULL, st->path,
                                "report schema %I, this build speaks %d", schema->u.i,
                                CF_REPORT_SCHEMA);
        }
    }
    mode = cf_ks_get(&doc, "mode", 4);
    if (mode == NULL || mode->type != CF_TYPE_TEXT ||
        strcmp(mode->u.t.p, "full") != 0) {
        /* A blind report has no values to replay. Refusing outright is
           the only honest answer: half a configuration would look like
           a whole one. */
        st->failed = CF_TRUE;
        return cf_error_set(&st->err, CF_E_CAPABILITY, NULL, st->path,
                            "'%s' is a value-blind report and cannot be "
                            "replayed; replay needs a full report",
                            st->path);
    }

    /* Every "keys.<key>.candidates.<n>.value" whose candidate carries
       no outcome is the selected one. */
    for (i = 0; i < cf_ks_count(&doc); ++i) {
        const cf_entry* e = cf_ks_at(&doc, i);
        const char*     k = e->key;
        size_t          n = e->keylen;
        size_t          klen;

        if (n < 6 || memcmp(k, "keys.", 5) != 0) {
            continue;
        }
        if (n < 6 + 6 || memcmp(k + n - 6, ".value", 6) != 0) {
            continue;
        }
        k += 5;
        n -= 5 + 6;

        /* Strip ".candidates.<n>" from the end. */
        {
            size_t j = n;
            while (j > 0 && k[j - 1] != '.') {
                --j;
            }
            if (j == 0 || !cf_is_digit((unsigned char)k[j])) {
                continue;
            }
            j -= 1;
            if (j < 11 || memcmp(k + j - 11, ".candidates", 11) != 0) {
                continue;
            }
            klen = j - 11;
        }
        if (klen == 0 || e->selected == NULL) {
            continue;
        }
        /* Only the candidate that won: a replay reproduces what the
           process was running, not every value it considered. */
        {
            char   probe[CF_KEY_MAX];
            size_t plen = e->keylen - 6;
            if (plen + 9 > sizeof probe) {
                continue;
            }
            memcpy(probe, e->key, plen);
            memcpy(probe + plen, ".outcome", 9); /* and its terminator */
            if (cf_ks_find(&doc, probe, plen + 8) != NULL) {
                continue; /* shadowed, refused or truncated */
            }
        }
        if (!cf_ks_add(ks, k, klen, s->rank, &e->selected->value, st->path)) {
            st->failed = CF_TRUE;
            return cf_error_set(&st->err, CF_E_MEMORY, NULL, st->path,
                                "no room for a replayed key");
        }
    }
    return CF_TRUE;
}

static void replay_destroy(cf_source* s) {
    (void)s;
}

static const cf_source_vtable cf_replay_vt = {"replay", replay_load, replay_destroy,
                                              NULL};

size_t cf_source_replay_state_size(void) {
    return sizeof(replay_state);
}

cf_bool cf_source_replay(cf_source* s, const char* name, int rank, const char* path,
                         void* state, size_t state_size) {
    replay_state* st = (replay_state*)state;

    if (s == NULL || path == NULL || st == NULL || state_size < sizeof(replay_state)) {
        return CF_FALSE;
    }
    memset(st, 0, sizeof *st);
    st->path     = path;
    st->err.size = sizeof(cf_error);

    s->vt   = &cf_replay_vt;
    s->name = (name != NULL) ? name : "replay";
    s->rank = rank;
    s->ext  = st;
    s->ref  = NULL;
    return CF_TRUE;
}

const cf_error* cf_source_replay_error(const cf_source* s) {
    const replay_state* st;

    if (s == NULL || s->vt != &cf_replay_vt || s->ext == NULL) {
        return NULL;
    }
    st = (const replay_state*)s->ext;
    return st->failed ? &st->err : NULL;
}
