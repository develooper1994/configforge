/*
 * ini.c - the built-in INI subset. C89.
 *
 * This is the codec that exists in every build profile, including
 * freestanding, and it is the one inside the amalgamation. That is why
 * it is written here rather than vendored: the grammar is small, and
 * no available C parser is both streaming and allocation-free.
 *
 *     # comment          ; comment
 *     [section]          [section.sub]
 *     key = value        key=value
 *     key = "value"      quotes are stripped, not required
 *     ports[] = 80       one element more of the array `ports`
 *     ports[] = 443
 *     ports[1] = 8443    the element at index 1
 *
 * Arrays are written as INI writes them where it writes them at all -
 * PHP's parse_ini_file, the reader with arrays that most INI files are
 * written for (D-60) - not in a syntax made up here: `name[]` appends,
 * `name[i]` sets element i. The `name[]` lines of one array are
 * consecutive; a second run of them further down would restart the
 * count and overwrite the first, so it is refused with both lines named.
 * INI has no form for an array of tables, of arrays, or an empty one,
 * and those are refused on writing by name; a table is a section.
 *
 * Values are emitted as text. INI carries no types, so the definition
 * decides, and Tier 0 inference decides when there is no definition.
 *
 * A value that holds a line break is not written. INI has no escape for
 * one, the line would end at it, and a file that reads back as something
 * else than what was written is worse than an error that says which key.
 */

#include "configforge/cf_serialize.h"

#include <string.h>

#if CF_WITH_INI

typedef struct ini_scan {
    const char*  p;
    const char*  end;
    const char*  file;
    unsigned int line;
    const char*  line_start;
} ini_scan;

static unsigned int ini_col(const ini_scan* s, const char* at) {
    return (unsigned int)(at - s->line_start) + 1u;
}

static void ini_newline(ini_scan* s) {
    ++s->line;
    s->line_start = s->p;
}

/* Space within a line. A newline is a token here, so cf_is_space is
   not what is wanted. */
static void ini_skip_blank(ini_scan* s) {
    while (s->p < s->end && (*s->p == ' ' || *s->p == '\t' || *s->p == '\r')) {
        ++s->p;
    }
}

static void ini_skip_to_eol(ini_scan* s) {
    while (s->p < s->end && *s->p != '\n') {
        ++s->p;
    }
}

static size_t ini_trim(const char* p, size_t n, const char** out) {
    size_t a = 0;

    while (a < n && (p[a] == ' ' || p[a] == '\t' || p[a] == '\r')) {
        ++a;
    }
    while (n > a && (p[n - 1] == ' ' || p[n - 1] == '\t' || p[n - 1] == '\r')) {
        --n;
    }
    *out = p + a;
    return n - a;
}

/* A section header becomes a key prefix, so its segments must be legal
   keys. Rejecting here rather than at cf_ks_add means the error can
   point at the line. */
static cf_bool ini_emit_path(const cf_doc_sink* out, const char* path, size_t n,
                             int* depth, cf_error* err, const char* file,
                             unsigned int line) {
    size_t i = 0;

    while (*depth > 0) {
        if (!out->end(out->ctx)) {
            /* The consumer said no; a refusal still says where. */
            return cf_error_set(err, CF_E_SOURCE, NULL, file,
                                "%s:%u: the document could not be recorded", file,
                                line);
        }
        --*depth;
    }
    while (i < n) {
        size_t j = i;
        while (j < n && path[j] != '.') {
            ++j;
        }
        if (j == i) {
            return cf_error_set(err, CF_E_KEY, NULL, file,
                                "%s:%u: empty section segment", file, line);
        }
        if (!out->begin_table(out->ctx, path + i, j - i)) {
            return cf_error_set(err, CF_E_KEY, NULL, file,
                                "%s:%u: '%S' is not a valid key segment", file, line,
                                path + i, j - i);
        }
        ++*depth;
        i = (j < n) ? j + 1 : j;
    }
    return CF_TRUE;
}

/* `name[]` (an element appended, *index 0) or `name[i]` (*index i+1):
   the length of `name`, or 0 when `key` is neither. */
static size_t ini_bracket(const char* key, size_t klen, size_t* index) {
    size_t open = 0, i;

    if (klen < 3 || key[klen - 1] != ']') {
        return 0;
    }
    while (open < klen && key[open] != '[') {
        ++open;
    }
    if (open == 0 || open >= klen - 1) {
        return 0;
    }
    if (open + 2 == klen) {
        *index = 0;
        return open; /* name[] */
    }
    for (i = open + 1; i < klen - 1; ++i) {
        if (key[i] < '0' || key[i] > '9') {
            return 0;
        }
    }
    if (key[open + 1] == '0' && klen - open - 2 > 1) {
        return 0; /* "01" is not an index */
    }
    *index = 1;
    return open;
}

/* An open run of `name[]` lines: the array the sink has begun. */
typedef struct ini_run {
    char         name[CF_KEY_MAX];
    size_t       n;
    cf_bool      open;
    cf_bool      noted; /* its name is in the section's table */
    unsigned int line;  /* where it began */
} ini_run;

static cf_bool ini_run_close(ini_run* r, const cf_doc_sink* out) {
    if (!r->open) {
        return CF_TRUE;
    }
    r->open = CF_FALSE;
    return out->end(out->ctx);
}

/* The line, before `upto` and after `from`, of an earlier `name[] =` in
   the same section, or 0. The blob is in memory, so looking back at it
   needs no memory of its own; it is what the reader does when the sink
   lends no workspace for the names below, and what they must agree with. */
static unsigned int ini_earlier_run(const char* from, const char* upto,
                                    unsigned int first_line, const char* name,
                                    size_t n) {
    const char*  p    = from;
    unsigned int line = first_line;

    while (p < upto) {
        const char* eol = p;
        const char* k;
        size_t      kl;
        while (eol < upto && *eol != '\n') {
            ++eol;
        }
        kl = ini_trim(p, (size_t)(eol - p), &k);
        if (kl > n + 2 && memcmp(k, name, n) == 0 && k[n] == '[' && k[n + 1] == ']') {
            const char* q = k + n + 2;
            while (q < eol && (*q == ' ' || *q == '\t')) {
                ++q;
            }
            if (q < eol && *q == '=') {
                return line;
            }
        }
        p = (eol < upto) ? eol + 1 : eol;
        ++line;
    }
    return 0;
}

/* The array names a section has had, so that a run's start asks a hash
   and not every line above it: the look back costs the whole section
   each time, and a section of distinct names made reading quadratic.
   A name is where it lies in the blob, so nothing is copied. The table
   lives in the sink's workspace (cf_doc_sink.reserve_work); a sink that
   lends none, or runs out, sends the reader back to the look back, which
   answers the same. A slot belongs to the section numbered `gen`, so a
   header forgets the names by counting, not by clearing the table. */
typedef struct ini_name {
    size_t       at; /* the name's offset in the blob */
    size_t       gen;
    unsigned int line; /* its first line, as the look back finds it */
    unsigned int n;
} ini_name;

typedef struct ini_names {
    const cf_doc_sink* out;
    const char*        base;
    ini_name*          slot;
    size_t             cap;  /* a power of two; 0 until the first name */
    size_t             used; /* in this section */
    size_t             gen;
    cf_bool            off;
} ini_names;

static void ini_names_init(ini_names* t, const cf_doc_sink* out, const char* base) {
    /* How large a sink is that has reserve_work at all. */
    size_t need = offsetof(cf_doc_sink, reserve_work);

    need += sizeof out->reserve_work;

    t->out  = out;
    t->base = base;
    t->slot = NULL;
    t->cap  = 0;
    t->used = 0;
    t->gen  = 1;
    t->off  = CF_TRUE;
    if (out->size >= need && out->reserve_work != NULL) {
        t->off = CF_FALSE;
    }
}

static size_t ini_names_hash(const char* p, size_t n) {
    size_t h = 2166136261u, i;
    for (i = 0; i < n; ++i) {
        h = (h ^ (size_t)(unsigned char)p[i]) * 16777619u;
    }
    return h;
}

/* The slot holding `name` in this section, or the free one where it
   would go. Nothing leaves a section's table, so a probe ends at the
   first slot that is not this section's. */
static ini_name* ini_names_slot(const ini_names* t, const char* name, size_t n) {
    size_t i = ini_names_hash(name, n) & (t->cap - 1);

    for (;;) {
        ini_name* x = &t->slot[i];
        if (x->gen != t->gen ||
            ((size_t)x->n == n && memcmp(t->base + x->at, name, n) == 0)) {
            return x;
        }
        i = (i + 1) & (t->cap - 1);
    }
}

/* The line where `name[]` first came in this section, or 0. */
static unsigned int ini_names_line(const ini_names* t, const char* name, size_t n) {
    const ini_name* x;

    if (t->cap == 0) {
        return 0;
    }
    x = ini_names_slot(t, name, n);
    return (x->gen == t->gen) ? x->line : 0;
}

/* Twice the room, this section's names moved over. What was outgrown
   stays where the sink put it until the decode is over. */
static cf_bool ini_names_grow(ini_names* t) {
    size_t    c      = (t->cap == 0) ? 16 : t->cap * 2, i;
    ini_name* old    = t->slot;
    size_t    oldcap = t->cap;
    ini_name* slot;

    if (c > (size_t)-1 / sizeof *slot) {
        return CF_FALSE;
    }
    slot = (ini_name*)t->out->reserve_work(t->out->ctx, c * sizeof *slot);
    if (slot == NULL) {
        return CF_FALSE;
    }
    memset(slot, 0, c * sizeof *slot);
    t->slot = slot;
    t->cap  = c;
    for (i = 0; i < oldcap; ++i) {
        if (old[i].gen == t->gen) {
            *ini_names_slot(t, t->base + old[i].at, old[i].n) = old[i];
        }
    }
    return CF_TRUE;
}

static void ini_names_add(ini_names* t, const char* name, size_t n, unsigned int line) {
    ini_name* x;

    if (t->off) {
        return;
    }
    if ((t->used + 1) * 2 > t->cap && !ini_names_grow(t)) {
        t->off = CF_TRUE;
        return;
    }
    x = ini_names_slot(t, name, n);
    if (x->gen != t->gen) {
        x->at   = (size_t)(name - t->base);
        x->gen  = t->gen;
        x->line = line;
        x->n    = (unsigned int)n;
        ++t->used;
    }
}

/* Whether the look back would see this line: it takes `name[]` followed
   by spaces or tabs and the '=', and the key's trim also passes over a
   carriage return there, so a line written `p[]<CR>=` is a run it does
   not find. The table remembers only what the look back would see. */
static cf_bool ini_names_seen(const char* after, const char* eq) {
    while (after < eq) {
        if (*after != ' ' && *after != '\t') {
            return CF_FALSE;
        }
        ++after;
    }
    return CF_TRUE;
}

static cf_bool ini_decode(const cf_blob* in, const cf_doc_sink* out, cf_error* err) {
    ini_scan     s;
    int          depth = 0;
    ini_run      run;
    ini_names    names;
    const char*  section_start;
    unsigned int section_line = 1;

    run.open  = CF_FALSE;
    run.noted = CF_FALSE;
    run.n     = 0;
    ini_names_init(&names, out, in->p);
    s.p           = in->p + cf_blob_skip_bom(in);
    s.end         = in->p + in->n;
    s.file        = (in->name != NULL) ? in->name : "<ini>";
    s.line        = 1;
    s.line_start  = s.p;
    section_start = s.p;

    if (!out->begin_table(out->ctx, NULL, 0)) {
        return cf_error_set(err, CF_E_SOURCE, NULL, s.file, "sink refused");
    }

    while (s.p < s.end) {
        ini_skip_blank(&s);
        if (s.p >= s.end) {
            break;
        }
        if (*s.p == '\n') {
            ++s.p;
            ini_newline(&s);
            continue;
        }
        if (*s.p == '#' || *s.p == ';') {
            ini_skip_to_eol(&s);
            continue;
        }

        if (*s.p == '[') {
            const char* start;
            const char* trimmed;
            size_t      n;

            if (!ini_run_close(&run, out)) {
                return cf_error_set(err, CF_E_SOURCE, NULL, s.file,
                                    "%s:%u: the document could not be recorded", s.file,
                                    s.line);
            }
            ++s.p;
            start = s.p;
            while (s.p < s.end && *s.p != ']' && *s.p != '\n') {
                ++s.p;
            }
            if (s.p >= s.end || *s.p != ']') {
                return cf_error_set(err, CF_E_SOURCE, NULL, s.file,
                                    "%s:%u: section header is not closed", s.file,
                                    s.line);
            }
            n = ini_trim(start, (size_t)(s.p - start), &trimmed);
            ++s.p;
            if (n == 0) {
                return cf_error_set(err, CF_E_SOURCE, NULL, s.file,
                                    "%s:%u: empty section header", s.file, s.line);
            }
            if (!ini_emit_path(out, trimmed, n, &depth, err, s.file, s.line)) {
                return CF_FALSE;
            }
            ini_skip_blank(&s);
            if (s.p < s.end && *s.p != '\n' && *s.p != '#' && *s.p != ';') {
                return cf_error_set(err, CF_E_SOURCE, NULL, s.file,
                                    "%s:%u: trailing text after a section "
                                    "header",
                                    s.file, s.line);
            }
            ini_skip_to_eol(&s);
            section_start = s.p;
            section_line  = s.line;
            ++names.gen;
            names.used = 0;
            continue;
        }

        {
            const char*  kstart = s.p;
            const char*  eq;
            const char*  key;
            const char*  val;
            size_t       klen, vlen;
            cf_value     v;
            cf_loc       at;
            unsigned int col = ini_col(&s, s.p);

            while (s.p < s.end && *s.p != '=' && *s.p != '\n') {
                ++s.p;
            }
            if (s.p >= s.end || *s.p != '=') {
                return cf_error_set(err, CF_E_SOURCE, NULL, s.file,
                                    "%s:%u: expected 'key = value'", s.file, s.line);
            }
            klen = ini_trim(kstart, (size_t)(s.p - kstart), &key);
            eq   = s.p;
            ++s.p;

            {
                const char* vstart = s.p;
                ini_skip_to_eol(&s);
                vlen = ini_trim(vstart, (size_t)(s.p - vstart), &val);
            }
            /* Quotes are stripped when they wrap the whole value.
               They are never required, so a value that merely contains
               a quote is left alone. */
            if (vlen >= 2 && ((val[0] == '"' && val[vlen - 1] == '"') ||
                              (val[0] == '\'' && val[vlen - 1] == '\''))) {
                ++val;
                vlen -= 2;
            }
            if (klen == 0) {
                return cf_error_set(err, CF_E_SOURCE, NULL, s.file, "%s:%u: empty key",
                                    s.file, s.line);
            }

            at.file = s.file;
            at.line = s.line;
            at.col  = col;
            cf_value_text(&v, val, vlen);

            {
                size_t index = 0;
                size_t nlen  = ini_bracket(key, klen, &index);
                char   flat[CF_KEY_MAX];

                if (nlen > 0 && index == 0) {
                    /* name[]: one more element of the run, or a new run */
                    if (!run.open || run.n != nlen ||
                        memcmp(run.name, key, nlen) != 0) {
                        unsigned int earlier;
                        if (!ini_run_close(&run, out)) {
                            return cf_error_set(
                                err, CF_E_SOURCE, NULL, s.file,
                                "%s:%u: the document could not be recorded", s.file,
                                s.line);
                        }
                        earlier = names.off ? ini_earlier_run(section_start, kstart,
                                                              section_line, key, nlen)
                                            : ini_names_line(&names, key, nlen);
                        if (earlier != 0) {
                            return cf_error_set(
                                err, CF_E_SOURCE, NULL, s.file,
                                "%s:%u: '%S[]' again after line %u; an array's "
                                "lines go together, or name each index",
                                s.file, s.line, key, nlen, earlier);
                        }
                        if (nlen >= sizeof run.name ||
                            !out->begin_array(out->ctx, key, nlen)) {
                            return cf_error_set(err, CF_E_KEY, NULL, s.file,
                                                "%s:%u: '%S' is not a valid key",
                                                s.file, s.line, key, nlen);
                        }
                        memcpy(run.name, key, nlen);
                        run.n     = nlen;
                        run.open  = CF_TRUE;
                        run.noted = CF_FALSE;
                        run.line  = s.line;
                    }
                    if (!run.noted && ini_names_seen(key + klen, eq)) {
                        ini_names_add(&names, key, nlen, s.line);
                        run.noted = CF_TRUE;
                    }
                    if (!out->scalar(out->ctx, NULL, 0, &v, CF_TRUE, &at)) {
                        return cf_error_set(err, CF_E_KEY, NULL, s.file,
                                            "%s:%u: '%S' could not be recorded", s.file,
                                            s.line, key, klen);
                    }
                    continue;
                }
                if (!ini_run_close(&run, out)) {
                    return cf_error_set(err, CF_E_SOURCE, NULL, s.file,
                                        "%s:%u: the document could not be recorded",
                                        s.file, s.line);
                }
                if (nlen > 0) {
                    /* name[i]: the flat name.i every format's element is */
                    if (klen + 1 > sizeof flat) {
                        return cf_error_set(err, CF_E_KEY, NULL, s.file,
                                            "%s:%u: '%S' is too long", s.file, s.line,
                                            key, klen);
                    }
                    memcpy(flat, key, nlen);
                    flat[nlen] = '.';
                    memcpy(flat + nlen + 1, key + nlen + 1, klen - nlen - 2);
                    key  = flat;
                    klen = klen - 1;
                }
                if (!out->scalar(out->ctx, key, klen, &v, CF_TRUE, &at)) {
                    return cf_error_set(err, CF_E_KEY, NULL, s.file,
                                        "%s:%u: '%S' is not a valid key", s.file,
                                        s.line, key, klen);
                }
            }
        }
    }

    if (!ini_run_close(&run, out)) {
        return cf_error_set(err, CF_E_SOURCE, NULL, s.file,
                            "%s: the document could not be recorded", s.file);
    }
    while (depth > 0) {
        if (!out->end(out->ctx)) {
            return cf_error_set(err, CF_E_SOURCE, NULL, s.file,
                                "%s: the document could not be recorded", s.file);
        }
        --depth;
    }
    if (!out->end(out->ctx)) {
        return cf_error_set(err, CF_E_SOURCE, NULL, s.file,
                            "%s: the document could not be recorded", s.file);
    }
    return CF_TRUE;
}

#if CF_WITH_EMIT

/* ------------------------------------------------------------------ *
 * Encoding
 *
 * Sections are emitted for every table, and a scalar at the top level
 * comes first so it does not land inside the previous section - the
 * classic way a hand-written INI writer corrupts its own output.
 * ------------------------------------------------------------------ */

typedef struct ini_enc {
    cf_sink* out;
    char     path[CF_KEY_MAX];
    size_t   plen;
    cf_u16   stack[CF_MAX_DEPTH]; /* lengths within path */
    int      depth;
    cf_bool  header_pending;
    /* Inside an array: its name is path[arr_from, plen), written before
       each element's "[] = ". */
    cf_bool arrays;
    size_t  arr_from;
} ini_enc;

/* cf_emit keeps this on its own frame, in CF_ENCODER_STATE_MAX bytes. */
typedef char ini_enc_fits[(sizeof(ini_enc) <= CF_ENCODER_STATE_MAX) ? 1 : -1];

static cf_bool ini_enc_push(ini_enc* e, const char* name, size_t n) {
    if (e->depth >= (int)(sizeof e->stack / sizeof e->stack[0])) {
        return CF_FALSE;
    }
    e->stack[e->depth++] = (cf_u16)e->plen;
    if (name != NULL) {
        if (e->plen + n + 2 > sizeof e->path) {
            return CF_FALSE;
        }
        if (e->plen > 0) {
            e->path[e->plen++] = '.';
        }
        memcpy(e->path + e->plen, name, n);
        e->plen += n;
        e->header_pending = CF_TRUE;
    }
    return CF_TRUE;
}

static void ini_enc_header(ini_enc* e) {
    if (e->header_pending && e->plen > 0) {
        cf_sink_puts(e->out, "\n[");
        cf_sink_write(e->out, e->path, e->plen);
        cf_sink_puts(e->out, "]\n");
        e->header_pending = CF_FALSE;
    }
}

/* A table inside an array, and an array inside one, have no INI form;
   cf_emit asks first (CF_CODEC_SCALAR_ARRAYS), so these refusals are
   the last line of defence, not the way it is found. */
static cf_bool ini_enc_table(void* ctx, const char* name, size_t n) {
    ini_enc* e = (ini_enc*)ctx;
    if (e->arrays) {
        return CF_FALSE;
    }
    return ini_enc_push(e, name, n);
}

/* The array's elements become "name[] = value" lines, PHP's form (D-60),
   in the section of the table around it - whose header goes out first,
   since the array itself is not a section. */
static cf_bool ini_enc_array(void* ctx, const char* name, size_t n) {
    ini_enc* e = (ini_enc*)ctx;
    size_t   from;

    if (e->arrays || name == NULL) {
        return CF_FALSE;
    }
    ini_enc_header(e);
    from = (e->plen > 0) ? e->plen + 1 : 0;
    if (!ini_enc_push(e, name, n)) {
        return CF_FALSE;
    }
    e->header_pending = CF_FALSE;
    e->arrays         = CF_TRUE;
    e->arr_from       = from;
    return CF_TRUE;
}

/* How a text value goes into its line: 0 as it stands, 1 in quotes, and
   -1 not at all. The reader takes the blanks and the carriage returns at
   either end of a value to be the line's, and a pair of quotes round the
   whole of it to be quoting, so a value that has either of those at its
   ends has to be quoted to come back as it was; and one that holds a
   space, a quote or a comment character is quoted for the sake of the
   readers that are not this one. A line break has no escape in INI, and
   a value that holds one is not written: the line would end there. */
static int ini_text_form(const char* p, size_t n) {
    int    form = (n == 0) ? 1 : 0;
    size_t i;

    for (i = 0; i < n; ++i) {
        if (p[i] == '\n') {
            return -1;
        }
        if (p[i] == ' ' || p[i] == '\t' || p[i] == '\r' || p[i] == '"' || p[i] == '#' ||
            p[i] == ';') {
            form = 1;
        }
    }
    if (n >= 2 && p[0] == '\'' && p[n - 1] == '\'') {
        form = 1;
    }
    return form;
}

static cf_bool ini_enc_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                              cf_bool textual, const cf_loc* at) {
    ini_enc* e = (ini_enc*)ctx;
    char     tmp[CF_VALUE_TEXT_MAX];
    size_t   len;
    int      form = 0;

    (void)textual;
    (void)at;
    if (v->type == CF_TYPE_TEXT) {
        form = ini_text_form(v->u.t.p, v->u.t.n);
        if (form < 0) {
            return CF_FALSE;
        }
    }
    ini_enc_header(e);
    if (e->arrays) {
        cf_sink_write(e->out, e->path + e->arr_from, e->plen - e->arr_from);
        cf_sink_puts(e->out, "[]");
    } else if (name != NULL) {
        cf_sink_write(e->out, name, n);
    }
    cf_sink_puts(e->out, " = ");

    if (v->type == CF_TYPE_TEXT) {
        /* The quotes are the only escape INI has, and what is between
           them is written as it is. */
        if (form) {
            cf_sink_putc(e->out, '"');
        }
        cf_sink_write(e->out, v->u.t.p, v->u.t.n);
        if (form) {
            cf_sink_putc(e->out, '"');
        }
    } else {
        len = cf_value_to_text(v, tmp, sizeof tmp);
        cf_sink_write(e->out, tmp, len);
    }
    cf_sink_putc(e->out, '\n');
    return CF_TRUE;
}

static cf_bool ini_enc_end(void* ctx) {
    ini_enc* e = (ini_enc*)ctx;

    if (e->depth <= 0) {
        return CF_FALSE;
    }
    e->plen           = e->stack[--e->depth];
    e->arrays         = CF_FALSE;
    e->header_pending = CF_FALSE;
    return CF_TRUE;
}

static cf_bool ini_encoder(cf_sink* out, void* state, cf_doc_sink* sink) {
    ini_enc* e = (ini_enc*)state;

    if (e == NULL || sink == NULL) {
        return CF_FALSE;
    }
    memset(e, 0, sizeof *e);
    e->out = out;

    sink->size        = offsetof(cf_doc_sink, reserve_text);
    sink->begin_table = ini_enc_table;
    sink->begin_array = ini_enc_array;
    sink->scalar      = ini_enc_scalar;
    sink->end         = ini_enc_end;
    sink->ctx         = e;
    return CF_TRUE;
}

const cf_codec cf_codec_ini = {
    sizeof(cf_codec),  "ini",
    ".ini;.cfg;.conf", CF_CODEC_READ | CF_CODEC_WRITE | CF_CODEC_SCALAR_ARRAYS,
    ini_decode,        ini_encoder,
    sizeof(ini_enc)};

#else /* !CF_WITH_EMIT: read only */

const cf_codec cf_codec_ini = {sizeof(cf_codec),
                               "ini",
                               ".ini;.cfg;.conf",
                               CF_CODEC_READ | CF_CODEC_SCALAR_ARRAYS,
                               ini_decode,
                               NULL,
                               0};

#endif /* CF_WITH_EMIT */

#endif /* CF_WITH_INI */
