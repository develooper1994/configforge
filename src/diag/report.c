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

#include "configforge/cf_numeric.h"
#include "configforge/cf_platform.h"
#include "configforge/cf_report.h"
#include "configforge/cf_version.h"

#include <string.h>

#if CF_WITH_REPORT

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
        h *= ((cf_u64)256U << 32) | (cf_u64)435U; /* 1099511628211, see keyspace.c */
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
    if (d->replaced_by != NULL) {
        /* An added field, as "written" is: schema 1 readers skip it. */
        cf_sink_puts(out, ", \"replaced_by\": ");
        report_quote_z(out, d->replaced_by);
    }
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
    /* What a value with references was written as (D-10), redacted with
       the value it made. An added field: schema 1 readers ignore it. */
    if (c->written != NULL && (c->flags & CF_CAND_TEMPLATE) == 0 && !redact) {
        cf_sink_puts(out, ", \"written\": ");
        report_quote_z(out, c->written);
    }
    cf_sink_putc(out, '}');
}

/* One key's object - its definition, every candidate, the winner - as
   the report writes it under "keys", and as cf_report_key_write writes
   it alone. `e` may be NULL for a key nobody supplied. */
static void report_key(cf_sink* out, const cf_entry* e, const cf_def* d,
                       const cf_source_view* sv, cf_report_mode mode,
                       const char* indent) {
    const cf_candidate* c;
    cf_bool             first = CF_TRUE;

    cf_sink_fmt(out, "{\n%s  \"definition\": ", indent);
    report_definition(out, d);
    cf_sink_fmt(out, ",\n%s  \"candidates\": [", indent);

    /* Nothing is dropped from the chain: a shadowed or refused
       candidate is the whole point of the artifact. */
    for (c = (e != NULL) ? e->chain : NULL; c != NULL; c = c->next) {
        if (!first) {
            cf_sink_putc(out, ',');
        }
        cf_sink_fmt(out, "\n%s    ", indent);
        report_candidate(out, c, sv, d, mode);
        first = CF_FALSE;
    }
    cf_sink_fmt(out, "\n%s  ],\n%s  \"selected\": ", indent, indent);
    if (e != NULL && e->selected != NULL) {
        const cf_source* s = source_of(sv, e->selected->source);
        cf_sink_puts(out, "{ \"source\": ");
        report_quote_z(out, (s != NULL) ? s->name : "?");
        cf_sink_puts(out, ", \"reason\": \"highest precedence with a "
                          "usable value\" }");
    } else {
        cf_sink_puts(out, "null");
    }
    cf_sink_fmt(out, "\n%s}", indent);
}

size_t cf_report_key_write(const cf_keyspace* ks, const cf_defset* ds,
                           const cf_source_view* sv, const char* key, size_t keylen,
                           cf_report_mode mode, char* buf, size_t cap) {
    cf_sink         out;
    const cf_entry* e = NULL;
    size_t          i;

    cf_sink_init(&out, buf, cap);
    if (ks == NULL || key == NULL) {
        cf_sink_terminate(&out);
        return 0;
    }
    for (i = 0; i < cf_ks_count(ks); ++i) {
        const cf_entry* x = cf_ks_at(ks, i);
        if (x->keylen == keylen && memcmp(x->key, key, keylen) == 0) {
            e = x;
            break;
        }
    }
    cf_sink_puts(&out, "{ \"configforge_key\": ");
    cf_sink_fmt(&out, "%d, \"key\": ", CF_REPORT_SCHEMA);
    report_quote(&out, key, keylen);
    cf_sink_puts(&out, ", \"value\": ");
    report_key(&out, e, (ds != NULL) ? cf_defset_find(ds, key, keylen) : NULL, sv, mode,
               "");
    cf_sink_puts(&out, " }\n");
    cf_sink_terminate(&out);
    return out.len;
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
    /* And the commit it was built from (D-40): two builds of a version
       are told apart. Added beside the version, which no reader of
       schema 1 objects to, so the schema number stays. */
    cf_sink_fmt(&out,
                "  \"library\": { \"version\": \"%s\", \"build\": ", cf_version());
    report_quote_z(&out, cf_build_id());
    cf_sink_puts(&out, ", \"features\": [");
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

        if (i > 0) {
            cf_sink_putc(&out, ',');
        }
        cf_sink_puts(&out, "\n    ");
        report_quote(&out, e->key, e->keylen);
        cf_sink_puts(&out, ": ");
        report_key(&out, e, d, sv, mode, "    ");
    }
    cf_sink_puts(&out, "\n  }\n}\n");
    cf_sink_terminate(&out);
    return out.len;
}

/* ------------------------------------------------------------------ *
 * JSON Schema
 *
 * The definitions as an editor reads them (Design.md, section 41): a
 * file that names the schema gets completion, inline documentation and
 * validation as it is typed, from the contract the program already
 * declared, with no plugin.
 *
 * Two choices differ from a literal reading of the mapping table.
 * "required" is not written: a key the program requires may come from
 * the environment or the command line, so a file without it is not
 * wrong, and a schema that said so would flag correct files. And a
 * sensitive key's default is not written, for the reason a blind report
 * omits its value.
 * ------------------------------------------------------------------ */

typedef struct schema_ctx {
    const cf_defset* ds;
    cf_sink*         out;
    cf_bool          closed;
    cf_error*        err;
    cf_bool          failed;
} schema_ctx;

/* Every definition, then every open prefix, as one sequence: a level of
   the schema is made of both. */
static cf_bool schema_item(const schema_ctx* s, size_t i, const char** key, size_t* n,
                           const cf_def** d) {
    size_t count = cf_defset_count(s->ds);

    if (i < count) {
        *d   = cf_defset_at(s->ds, i);
        *key = (*d)->key;
        *n   = strlen(*key);
        return CF_TRUE;
    }
    i -= count;
    if (i < s->ds->open_count) {
        *d   = NULL;
        *key = s->ds->open_prefixes[i];
        *n   = strlen(*key);
        return CF_TRUE;
    }
    return CF_FALSE;
}

/* Strictly below the prefix (p, plen); everything is below the root. */
static cf_bool schema_under(const char* key, size_t n, const char* p, size_t plen) {
    if (plen == 0) {
        return CF_TRUE;
    }
    return (n > plen && memcmp(key, p, plen) == 0 && key[plen] == '.') ? CF_TRUE
                                                                       : CF_FALSE;
}

static const char* schema_segment(const char* key, size_t n, size_t plen,
                                  size_t* slen) {
    size_t a = (plen == 0) ? 0 : plen + 1;
    size_t b = a;

    while (b < n && key[b] != '.') {
        ++b;
    }
    *slen = b - a;
    return key + a;
}

static void schema_indent(cf_sink* out, int depth) {
    int i;
    for (i = 0; i < depth; ++i) {
        cf_sink_puts(out, "  ");
    }
}

static void schema_leaf(schema_ctx* s, const cf_def* d) {
    cf_sink* out    = s->out;
    cf_bool  secret = ((d->flags & CF_SENSITIVE) != 0) ? CF_TRUE : CF_FALSE;

    cf_sink_fmt(out, "{ \"type\": \"%s\"", cf_type_json_name(d->type));
    if (d->description != NULL) {
        cf_sink_puts(out, ", \"description\": ");
        report_quote_z(out, d->description);
    }
    if ((d->flags & CF_DEPRECATED) != 0) {
        cf_sink_puts(out, ", \"deprecated\": true");
    }
    if (secret) {
        /* What JSON Schema has for "never shown back": an editor that
           honours it will not echo the value in hints. */
        cf_sink_puts(out, ", \"writeOnly\": true");
    }
    if (d->has_default && !secret) {
        cf_sink_puts(out, ", \"default\": ");
        report_value(out, &d->default_value);
    }
    if (d->has_range && d->type == CF_TYPE_INT) {
        cf_sink_fmt(out, ", \"minimum\": %I, \"maximum\": %I", d->imin, d->imax);
    }
#if CF_WITH_REAL
    if (d->has_range && d->type == CF_TYPE_REAL) {
        char lo[CF_F64_DIGITS], hi[CF_F64_DIGITS];
        cf_f64_to_text(d->rmin, lo, sizeof lo);
        cf_f64_to_text(d->rmax, hi, sizeof hi);
        cf_sink_fmt(out, ", \"minimum\": %s, \"maximum\": %s", lo, hi);
    }
#endif
    if (d->type == CF_TYPE_TEXT) {
        if (d->min_len != 0) {
            cf_sink_fmt(out, ", \"minLength\": %zu", d->min_len);
        }
        if (d->max_len != 0) {
            cf_sink_fmt(out, ", \"maxLength\": %zu", d->max_len);
        }
        if (d->allowed != NULL) {
            size_t i;
            cf_sink_puts(out, ", \"enum\": [");
            for (i = 0; d->allowed[i] != NULL; ++i) {
                cf_sink_puts(out, (i == 0) ? "" : ", ");
                report_quote_z(out, d->allowed[i]);
            }
            cf_sink_puts(out, "]");
        }
    }
    cf_sink_puts(out, " }");
}

/* The members of the object for the prefix (p, plen): its type, its
   properties in byte order of their names, and whether it is closed.
   Each property is the smallest name after the previous one, found by
   a scan, so the definitions need not be sealed and no buffer is
   needed for a name: every name is a slice of a key that exists. */
static void schema_object(schema_ctx* s, const char* p, size_t plen, cf_bool open,
                          int depth) {
    cf_sink*    out   = s->out;
    const char* last  = NULL;
    size_t      lastn = 0;
    cf_bool     first = CF_TRUE;

    if (depth >= CF_MAX_DEPTH) {
        if (!s->failed) {
            cf_error_set(s->err, CF_E_KEY, NULL, NULL,
                         "the definitions nest deeper than %d levels", CF_MAX_DEPTH);
        }
        s->failed = CF_TRUE;
        return;
    }
    schema_indent(out, depth + 1);
    cf_sink_puts(out, "\"type\": \"object\",\n");
    schema_indent(out, depth + 1);
    cf_sink_puts(out, "\"properties\": {");

    for (;;) {
        const char*   best    = NULL;
        const char*   bestkey = NULL;
        size_t        bestn   = 0;
        const cf_def* leaf    = NULL;
        cf_bool       below   = CF_FALSE;
        cf_bool       opened  = CF_FALSE;
        size_t        clen;
        size_t        i;
        const char*   key;
        size_t        n;
        const cf_def* d;

        for (i = 0; schema_item(s, i, &key, &n, &d); ++i) {
            size_t      sn;
            const char* sg;
            if (!schema_under(key, n, p, plen)) {
                continue;
            }
            sg = schema_segment(key, n, plen, &sn);
            if (last != NULL && cf_cmp_bytes(sg, sn, last, lastn) <= 0) {
                continue;
            }
            if (best == NULL || cf_cmp_bytes(sg, sn, best, bestn) < 0) {
                best    = sg;
                bestn   = sn;
                bestkey = key;
            }
        }
        if (best == NULL) {
            break;
        }
        /* The property's full name is the prefix of the key it came
           from, up to the end of its segment. */
        clen = (size_t)(best - bestkey) + bestn;
        for (i = 0; schema_item(s, i, &key, &n, &d); ++i) {
            if (n == clen && memcmp(key, bestkey, clen) == 0) {
                if (d != NULL) {
                    leaf = d;
                } else {
                    opened = CF_TRUE;
                }
            } else if (schema_under(key, n, bestkey, clen)) {
                below = CF_TRUE;
            }
        }

        cf_sink_puts(out, first ? "\n" : ",\n");
        schema_indent(out, depth + 2);
        report_quote(out, best, bestn);
        cf_sink_puts(out, ": ");
        if (leaf != NULL && !below && !opened) {
            schema_leaf(s, leaf);
        } else {
            cf_sink_puts(out, "{\n");
            schema_object(s, bestkey, clen, (open || opened) ? CF_TRUE : CF_FALSE,
                          depth + 2);
            cf_sink_puts(out, "\n");
            schema_indent(out, depth + 2);
            cf_sink_puts(out, "}");
        }
        first = CF_FALSE;
        last  = best;
        lastn = bestn;
    }
    if (!first) {
        cf_sink_puts(out, "\n");
        schema_indent(out, depth + 1);
    }
    cf_sink_puts(out, "}");
    if (s->closed && !open) {
        cf_sink_puts(out, ",\n");
        schema_indent(out, depth + 1);
        cf_sink_puts(out, "\"additionalProperties\": false");
    }
}

size_t cf_schema_write(const cf_defset* ds, const char* title, cf_bool closed,
                       char* buf, size_t cap, cf_error* err) {
    cf_sink    out;
    schema_ctx s;

    cf_sink_init(&out, buf, cap);
    if (ds == NULL) {
        cf_sink_terminate(&out);
        cf_error_set(err, CF_E_SOURCE, NULL, NULL, "no definitions to describe");
        return 0;
    }
    s.ds     = ds;
    s.out    = &out;
    s.closed = closed;
    s.err    = err;
    s.failed = CF_FALSE;

    cf_sink_puts(
        &out, "{\n  \"$schema\": \"https://json-schema.org/draft/2020-12/schema\",\n");
    if (title != NULL) {
        cf_sink_puts(&out, "  \"title\": ");
        report_quote_z(&out, title);
        cf_sink_puts(&out, ",\n");
    }
    schema_object(&s, "", 0, CF_FALSE, 0);
    cf_sink_puts(&out, "\n}\n");
    cf_sink_terminate(&out);
    if (s.failed) {
        if (buf != NULL && cap > 0) {
            buf[0] = '\0';
        }
        return 0;
    }
    return out.len;
}

#else /* !CF_WITH_REPORT */

size_t cf_schema_write(const cf_defset* ds, const char* title, cf_bool closed,
                       char* buf, size_t cap, cf_error* err) {
    (void)ds;
    (void)title;
    (void)closed;
    if (buf != NULL && cap > 0) {
        buf[0] = '\0';
    }
    cf_error_set(err, CF_E_CAPABILITY, NULL, NULL,
                 "a JSON Schema needs CF_WITH_REPORT, and this build was compiled "
                 "without it");
    return 0;
}

size_t cf_report_key_write(const cf_keyspace* ks, const cf_defset* ds,
                           const cf_source_view* sv, const char* key, size_t keylen,
                           cf_report_mode mode, char* buf, size_t cap) {
    (void)ks;
    (void)ds;
    (void)sv;
    (void)key;
    (void)keylen;
    (void)mode;
    if (buf != NULL && cap > 0) {
        buf[0] = '\0';
    }
    return 0;
}

/* Compiled without the report: nothing written, and a length of zero,
   which no report has - its opening brace alone is one byte. */
size_t cf_report_write(const cf_keyspace* ks, const cf_defset* ds,
                       const cf_source_view* sv, const cf_report_info* info, char* buf,
                       size_t cap) {
    (void)ks;
    (void)ds;
    (void)sv;
    (void)info;
    if (buf != NULL && cap > 0) {
        buf[0] = '\0';
    }
    return 0;
}

#endif /* CF_WITH_REPORT */

/* ================================================================== *
 * Report Replay
 * ================================================================== */

typedef struct replay_state {
    const char* path;
    unsigned    flags;
    cf_error    err;
    cf_bool     failed;
} replay_state;

/* The replay reader walks the report's own JSON with the ordinary
   codec and the ordinary flattener, into a scratch key space. What
   comes back is a flat map of the report's structure, which is then
   read for the selected value of every key. Parsing the report by hand
   would mean a second JSON reader that has to agree with the first. */
#if CF_WITH_REPORT

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
        if ((st->flags & CF_OPTIONAL) != 0) {
            /* Absent, and said so by explain(): a source that supplied
               nothing, as an optional file is. */
            cf_error_clear(&st->err);
            return CF_TRUE;
        }
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

#else /* !CF_WITH_REPORT */

static cf_bool replay_load(const cf_source* s, cf_keyspace* ks) {
    replay_state* st = (replay_state*)s->ext;

    (void)ks;
    st->failed = CF_TRUE;
    return cf_error_set(&st->err, CF_E_CAPABILITY, NULL, st->path,
                        "replaying '%s' needs CF_WITH_REPORT, and this build was "
                        "compiled without it",
                        st->path);
}

#endif /* CF_WITH_REPORT */

static void replay_destroy(cf_source* s) {
    (void)s;
}

static const cf_source_vtable cf_replay_vt = {
    sizeof(cf_source_vtable), "replay", replay_load, replay_destroy, NULL, NULL};

size_t cf_source_replay_state_size(void) {
    return sizeof(replay_state);
}

cf_bool cf_source_replay(cf_source* s, const char* name, int rank, const char* path,
                         unsigned flags, void* state, size_t state_size) {
    replay_state* st = (replay_state*)state;

    if (s == NULL || path == NULL || st == NULL || state_size < sizeof(replay_state)) {
        return CF_FALSE;
    }
    memset(st, 0, sizeof *st);
    st->path     = path;
    st->flags    = flags;
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
