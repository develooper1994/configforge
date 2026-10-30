/*
 * yaml.c - YAML, read through libyaml and written by hand. C89.
 *
 * Reading is libyaml's event parser mapped onto the document events,
 * one to one: a mapping is a table, a sequence an array, a scalar a
 * scalar. Writing is a few dozen lines of block style here, because
 * every value this library holds can be written in a handful of forms
 * and libyaml's emitter would bring an allocation and a second state
 * machine for them.
 *
 * What it reads is YAML as configuration is written, and the rest is
 * refused by name, as TOML's dates are:
 *
 *   anchors and aliases   a value written once and used twice is a
 *                         value whose origin explain() cannot name
 *                         once; repeat it
 *   tags (!!str, !custom) a type is the definition's to decide
 *   several documents     one file is one document
 *   a key that is not a   a configuration key is text
 *   scalar
 *
 * The decision that matters is about types (D-39). A plain scalar is
 * handed over as text, to be typed by its definition, exactly as an INI
 * value is; a quoted one is text, and says so. YAML 1.1 reads
 * "country: no" as false and "version: 3.10" as 3.1, and which of 1.1
 * and 1.2 a reader follows decides it - the problem every YAML
 * configuration has met. Here the program's contract decides, and the
 * library's one spelling of a boolean (D-24, "true" and "false" only)
 * means "no" is the text "no" under either version and under a
 * definition too. Without a contract, Tier 0 inference applies, as it
 * does to INI, and "3.10" written plain is the number it looks like:
 * quote what is meant as text. The one plain
 * scalar given a type of its own is null - "~", "null", or nothing -
 * which is a value here, as JSON's null is.
 */

#include "configforge/cf_serialize.h"

#include <string.h>

#if CF_WITH_YAML

#include <yaml.h>

/* ================================================================== *
 * Decoding
 * ================================================================== */

typedef struct yaml_level {
    unsigned char is_map;   /* a mapping, or else a sequence           */
    unsigned char want_key; /* in a mapping: the next scalar is a key  */
} yaml_level;

typedef struct yaml_scan {
    const char*        file;
    const cf_doc_sink* out;
    cf_error*          err;
    yaml_level         level[CF_MAX_DEPTH];
    int                depth; /* -1 before the top-level mapping */
    char               key[CF_KEY_MAX];
    size_t             klen;
    int                documents;
} yaml_scan;

static cf_bool yaml_fail(yaml_scan* s, const yaml_mark_t* m, const char* what) {
    return cf_error_set(s->err, CF_E_SOURCE, NULL, s->file, "%s:%u:%u: %s", s->file,
                        (unsigned int)m->line + 1u, (unsigned int)m->column + 1u, what);
}

/* An anchor or a tag on a node, refused with what to write instead. */
static cf_bool yaml_plain_node(yaml_scan* s, const yaml_event_t* ev,
                               const yaml_char_t* anchor, const yaml_char_t* tag) {
    if (anchor != NULL) {
        return yaml_fail(
            s, &ev->start_mark,
            "anchors and aliases are not in the YAML this library reads: "
            "a value used twice has no single origin to explain; repeat it");
    }
    if (tag != NULL) {
        return yaml_fail(
            s, &ev->start_mark,
            "tags are not in the YAML this library reads: a value's type is "
            "its definition's to decide");
    }
    return CF_TRUE;
}

/* The name a node at the current position goes by: the pending key in
   a mapping, nothing in a sequence. */
static cf_bool yaml_name(yaml_scan* s, const yaml_event_t* ev, const char** name,
                         size_t* n) {
    yaml_level* l = &s->level[s->depth];

    if (!l->is_map) {
        *name = NULL;
        *n    = 0;
        return CF_TRUE;
    }
    if (l->want_key) {
        return yaml_fail(s, &ev->start_mark,
                         "a key has to be a scalar: a mapping or a sequence cannot be "
                         "a configuration key");
    }
    *name       = s->key;
    *n          = s->klen;
    l->want_key = 1; /* this value answers the pending key */
    return CF_TRUE;
}

static cf_bool yaml_open(yaml_scan* s, const yaml_event_t* ev, cf_bool is_map) {
    const char* name = NULL;
    size_t      n    = 0;
    cf_bool     ok;

    if (s->depth < 0) {
        if (!is_map) {
            return yaml_fail(s, &ev->start_mark,
                             "a configuration is a mapping at the top, not a sequence");
        }
        ok = s->out->begin_table(s->out->ctx, NULL, 0);
    } else {
        if (!yaml_name(s, ev, &name, &n)) {
            return CF_FALSE;
        }
        if (s->depth + 1 >= CF_MAX_DEPTH) {
            return yaml_fail(s, &ev->start_mark, "nesting is too deep");
        }
        ok = is_map ? s->out->begin_table(s->out->ctx, name, n)
                    : s->out->begin_array(s->out->ctx, name, n);
    }
    if (!ok) {
        return yaml_fail(s, &ev->start_mark, "not a valid key");
    }
    ++s->depth;
    s->level[s->depth].is_map   = (unsigned char)(is_map ? 1 : 0);
    s->level[s->depth].want_key = 1;
    return CF_TRUE;
}

/* "~", "null" in any of YAML's three spellings, or nothing at all. */
static cf_bool yaml_is_null(const char* p, size_t n) {
    return (n == 0 || (n == 1 && p[0] == '~') ||
            (n == 4 && (memcmp(p, "null", 4) == 0 || memcmp(p, "Null", 4) == 0 ||
                        memcmp(p, "NULL", 4) == 0)))
               ? CF_TRUE
               : CF_FALSE;
}

static cf_bool yaml_scalar(yaml_scan* s, const yaml_event_t* ev) {
    const char* p = (const char*)ev->data.scalar.value;
    size_t      n = ev->data.scalar.length;
    cf_bool     plain =
        (ev->data.scalar.style == YAML_PLAIN_SCALAR_STYLE) ? CF_TRUE : CF_FALSE;
    yaml_level* l;
    const char* name;
    size_t      nlen;
    cf_value    v;
    cf_loc      at;

    if (s->depth < 0) {
        /* A document that is one scalar - or empty, which libyaml reads
           as a single empty one. */
        if (plain && n == 0) {
            return CF_TRUE;
        }
        return yaml_fail(s, &ev->start_mark,
                         "a configuration is a mapping at the top, not a single value");
    }
    l = &s->level[s->depth];
    if (l->is_map && l->want_key) {
        if (n == 0 || n >= sizeof s->key) {
            return yaml_fail(s, &ev->start_mark,
                             (n == 0) ? "an empty key" : "the key is too long");
        }
        memcpy(s->key, p, n);
        s->key[n]   = '\0';
        s->klen     = n;
        l->want_key = 0;
        return CF_TRUE;
    }
    if (!yaml_name(s, ev, &name, &nlen)) {
        return CF_FALSE;
    }
    if (plain && yaml_is_null(p, n)) {
        cf_value_null(&v);
        plain = CF_FALSE;
    } else {
        cf_value_text(&v, p, n);
    }
    at.file = s->file;
    at.line = (unsigned int)ev->start_mark.line + 1u;
    at.col  = (unsigned int)ev->start_mark.column + 1u;
    if (!s->out->scalar(s->out->ctx, name, nlen, &v, plain, &at)) {
        return yaml_fail(s, &ev->start_mark, "not a valid key");
    }
    return CF_TRUE;
}

/* One event. Sets *done at the end of the stream. */
static cf_bool yaml_event(yaml_scan* s, const yaml_event_t* ev, cf_bool* done) {
    switch (ev->type) {
    case YAML_NO_EVENT:
    case YAML_STREAM_START_EVENT:
    case YAML_DOCUMENT_END_EVENT:
        return CF_TRUE;
    case YAML_DOCUMENT_START_EVENT:
        if (++s->documents > 1) {
            return yaml_fail(s, &ev->start_mark,
                             "a second document: one file is one configuration");
        }
        return CF_TRUE;
    case YAML_STREAM_END_EVENT:
        *done = CF_TRUE;
        if (s->depth < 0) {
            /* Nothing in the file: an empty configuration, not an
               error, which is what an empty INI or TOML file is too. */
            return (s->out->begin_table(s->out->ctx, NULL, 0) &&
                    s->out->end(s->out->ctx))
                       ? CF_TRUE
                       : yaml_fail(s, &ev->start_mark,
                                   "the document could not be recorded");
        }
        return CF_TRUE;
    case YAML_ALIAS_EVENT:
        return yaml_plain_node(s, ev, ev->data.alias.anchor, NULL);
    case YAML_SCALAR_EVENT:
        return yaml_plain_node(s, ev, ev->data.scalar.anchor,
                               ev->data.scalar.plain_implicit ||
                                       ev->data.scalar.quoted_implicit
                                   ? NULL
                                   : ev->data.scalar.tag) &&
               yaml_scalar(s, ev);
    case YAML_SEQUENCE_START_EVENT:
        return yaml_plain_node(s, ev, ev->data.sequence_start.anchor,
                               ev->data.sequence_start.implicit
                                   ? NULL
                                   : ev->data.sequence_start.tag) &&
               yaml_open(s, ev, CF_FALSE);
    case YAML_MAPPING_START_EVENT:
        return yaml_plain_node(s, ev, ev->data.mapping_start.anchor,
                               ev->data.mapping_start.implicit
                                   ? NULL
                                   : ev->data.mapping_start.tag) &&
               yaml_open(s, ev, CF_TRUE);
    case YAML_SEQUENCE_END_EVENT:
    case YAML_MAPPING_END_EVENT:
        --s->depth;
        return s->out->end(s->out->ctx)
                   ? CF_TRUE
                   : yaml_fail(s, &ev->start_mark,
                               "the document could not be recorded");
    }
    return CF_TRUE;
}

static cf_bool yaml_decode(const cf_blob* in, const cf_doc_sink* out, cf_error* err) {
    yaml_parser_t parser;
    yaml_event_t  ev;
    yaml_scan     s;
    cf_bool       ok   = CF_TRUE;
    cf_bool       done = CF_FALSE;

    memset(&s, 0, sizeof s);
    s.file  = (in->name != NULL) ? in->name : "<yaml>";
    s.out   = out;
    s.err   = err;
    s.depth = -1;

    if (!yaml_parser_initialize(&parser)) {
        return cf_error_set(err, CF_E_MEMORY, NULL, s.file,
                            "no room for a YAML parser");
    }
    yaml_parser_set_input_string(&parser, (const unsigned char*)in->p, in->n);
    yaml_parser_set_encoding(&parser, YAML_UTF8_ENCODING);
    while (ok && !done) {
        if (!yaml_parser_parse(&parser, &ev)) {
            ok = yaml_fail(&s, &parser.problem_mark,
                           (parser.problem != NULL) ? parser.problem : "not YAML");
            break;
        }
        ok = yaml_event(&s, &ev, &done);
        yaml_event_delete(&ev);
    }
    yaml_parser_delete(&parser);
    return ok;
}

/* ================================================================== *
 * Encoding
 *
 * Block style, two spaces a level, every string double-quoted - which
 * is also how JSON writes one, and JSON's escapes are a subset of
 * YAML's - so that nothing written here reads back as anything else
 * under 1.1 or 1.2. A key is written bare when it is a word no YAML
 * reader could take for a value, and quoted otherwise.
 * ================================================================== */

#if CF_WITH_EMIT

typedef struct yaml_enc {
    cf_sink*      out;
    int           depth; /* containers open, the top-level one included */
    unsigned char is_seq[CF_MAX_DEPTH + 1];
    unsigned char empty[CF_MAX_DEPTH + 1]; /* opened, nothing in it yet */
} yaml_enc;

/* cf_emit keeps this on its own frame, in CF_ENCODER_STATE_MAX bytes. */
typedef char yaml_enc_fits[(sizeof(yaml_enc) <= CF_ENCODER_STATE_MAX) ? 1 : -1];

static void yaml_quote(cf_sink* out, const char* p, size_t n) {
    size_t i;

    cf_sink_putc(out, '"');
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)p[i];
        if (c == '"' || c == '\\') {
            cf_sink_putc(out, '\\');
            cf_sink_putc(out, (char)c);
        } else if (c == '\n') {
            cf_sink_puts(out, "\\n");
        } else if (c == '\t') {
            cf_sink_puts(out, "\\t");
        } else if (c < 0x20u || c == 0x7Fu) {
            cf_sink_fmt(out, "\\x%c%c", "0123456789abcdef"[(c >> 4) & 0xF],
                        "0123456789abcdef"[c & 0xF]);
        } else {
            cf_sink_putc(out, (char)c);
        }
    }
    cf_sink_putc(out, '"');
}

/* Words a YAML reader of either version takes for something other than
   a string, as a key or as a value. */
static cf_bool yaml_reserved(const char* p, size_t n) {
    static const char* const words[] = {"y",    "n",     "yes",  "no", "on", "off",
                                        "true", "false", "null", "~",  NULL};
    size_t                   i;

    for (i = 0; words[i] != NULL; ++i) {
        if (cf_cmp_ascii_ci(p, n, words[i], strlen(words[i])) == 0) {
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}

static void yaml_key(cf_sink* out, const char* p, size_t n) {
    size_t  i;
    cf_bool bare = (n > 0 && !yaml_reserved(p, n) && cf_is_alpha((unsigned char)p[0]))
                       ? CF_TRUE
                       : CF_FALSE;

    for (i = 0; bare && i < n; ++i) {
        unsigned char c = (unsigned char)p[i];
        if (!cf_is_alnum(c) && c != '_' && c != '-') {
            bare = CF_FALSE;
        }
    }
    if (bare) {
        cf_sink_write(out, p, n);
    } else {
        yaml_quote(out, p, n);
    }
}

/* Where a member starts: its indentation, its name or its dash. A
   container that had nothing in it yet is given its line break first,
   since until now it might have been written as {} or []. */
static void yaml_lead(yaml_enc* e, const char* name, size_t n) {
    int i;

    if (e->depth > 1 && e->empty[e->depth]) {
        cf_sink_putc(e->out, '\n');
    }
    e->empty[e->depth] = 0;
    for (i = 1; i < e->depth; ++i) {
        cf_sink_puts(e->out, "  ");
    }
    if (e->is_seq[e->depth]) {
        cf_sink_putc(e->out, '-');
    } else {
        yaml_key(e->out, name, n);
        cf_sink_putc(e->out, ':');
    }
}

static cf_bool yaml_enc_open(yaml_enc* e, const char* name, size_t n, cf_bool seq) {
    if (e->depth >= CF_MAX_DEPTH) {
        return CF_FALSE;
    }
    if (e->depth > 0) {
        yaml_lead(e, name, n);
    }
    ++e->depth;
    e->is_seq[e->depth] = (unsigned char)(seq ? 1 : 0);
    e->empty[e->depth]  = 1;
    return CF_TRUE;
}

static cf_bool yaml_enc_table(void* ctx, const char* name, size_t n) {
    return yaml_enc_open((yaml_enc*)ctx, name, n, CF_FALSE);
}

static cf_bool yaml_enc_array(void* ctx, const char* name, size_t n) {
    return yaml_enc_open((yaml_enc*)ctx, name, n, CF_TRUE);
}

static cf_bool yaml_enc_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                               cf_bool textual, const cf_loc* at) {
    yaml_enc* e = (yaml_enc*)ctx;
    char      tmp[CF_VALUE_TEXT_MAX];

    (void)textual;
    (void)at;
    yaml_lead(e, name, n);
    cf_sink_putc(e->out, ' ');
    switch (v->type) {
    case CF_TYPE_NULL:
        cf_sink_puts(e->out, "null");
        break;
    case CF_TYPE_BOOL:
        cf_sink_puts(e->out, v->u.b ? "true" : "false");
        break;
    case CF_TYPE_INT:
    case CF_TYPE_REAL:
        cf_sink_write(e->out, tmp, cf_value_to_text(v, tmp, sizeof tmp));
        break;
    case CF_TYPE_TEXT:
        yaml_quote(e->out, v->u.t.p, v->u.t.n);
        break;
    }
    cf_sink_putc(e->out, '\n');
    return CF_TRUE;
}

static cf_bool yaml_enc_end(void* ctx) {
    yaml_enc* e = (yaml_enc*)ctx;

    if (e->depth <= 0) {
        return CF_FALSE;
    }
    if (e->depth > 1 && e->empty[e->depth]) {
        cf_sink_puts(e->out, e->is_seq[e->depth] ? " []\n" : " {}\n");
    } else if (e->depth == 1 && e->empty[e->depth]) {
        cf_sink_puts(e->out, "{}\n");
    }
    --e->depth;
    return CF_TRUE;
}

static cf_bool yaml_encoder(cf_sink* out, void* state, cf_doc_sink* sink) {
    yaml_enc* e = (yaml_enc*)state;

    if (e == NULL || sink == NULL) {
        return CF_FALSE;
    }
    memset(e, 0, sizeof *e);
    e->out            = out;
    sink->size        = sizeof(cf_doc_sink);
    sink->begin_table = yaml_enc_table;
    sink->begin_array = yaml_enc_array;
    sink->scalar      = yaml_enc_scalar;
    sink->end         = yaml_enc_end;
    sink->ctx         = e;
    return CF_TRUE;
}

const cf_codec cf_codec_yaml = {
    sizeof(cf_codec), "yaml",
    ".yaml;.yml",     CF_CODEC_READ | CF_CODEC_WRITE | CF_CODEC_ARRAYS,
    yaml_decode,      yaml_encoder,
    sizeof(yaml_enc)};

#else /* !CF_WITH_EMIT: read only */

const cf_codec cf_codec_yaml = {sizeof(cf_codec),
                                "yaml",
                                ".yaml;.yml",
                                CF_CODEC_READ | CF_CODEC_ARRAYS,
                                yaml_decode,
                                NULL,
                                0};

#endif /* CF_WITH_EMIT */

#endif /* CF_WITH_YAML */
