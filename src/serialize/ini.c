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
 *
 * Deliberately absent: arrays. INI has no agreed syntax for them, and
 * inventing one would mean every INI file this library writes is
 * unreadable by every other INI reader. A configuration that needs
 * arrays uses JSON or TOML, and the error says so.
 *
 * Values are emitted as text. INI carries no types, so the definition
 * decides, and Tier 0 inference decides when there is no definition.
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
            return CF_FALSE;
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

static cf_bool ini_decode(const cf_blob* in, const cf_doc_sink* out, cf_error* err) {
    ini_scan s;
    int      depth = 0;

    s.p          = in->p;
    s.end        = in->p + in->n;
    s.file       = (in->name != NULL) ? in->name : "<ini>";
    s.line       = 1;
    s.line_start = in->p;

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
            continue;
        }

        {
            const char*  kstart = s.p;
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
            if (!out->scalar(out->ctx, key, klen, &v, CF_TRUE, &at)) {
                return cf_error_set(err, CF_E_KEY, NULL, s.file,
                                    "%s:%u: '%S' is not a valid key", s.file, s.line,
                                    key, klen);
            }
        }
    }

    while (depth > 0) {
        out->end(out->ctx);
        --depth;
    }
    out->end(out->ctx);
    return CF_TRUE;
}

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
    cf_bool  in_array;
    cf_bool  header_pending;
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

static cf_bool ini_enc_table(void* ctx, const char* name, size_t n) {
    return ini_enc_push((ini_enc*)ctx, name, n);
}

static cf_bool ini_enc_array(void* ctx, const char* name, size_t n) {
    ini_enc* e = (ini_enc*)ctx;
    /* INI has no array syntax, and inventing one would make every file
       this library writes unreadable by every other INI reader. The
       caller checks CF_CODEC_ARRAYS before encoding, so reaching here
       means the check was skipped. */
    e->in_array = CF_TRUE;
    return ini_enc_push(e, name, n);
}

static cf_bool ini_enc_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                              cf_bool textual, const cf_loc* at) {
    ini_enc* e = (ini_enc*)ctx;
    char     tmp[CF_VALUE_TEXT_MAX];
    size_t   len;

    (void)textual;
    (void)at;
    if (e->in_array) {
        return CF_FALSE;
    }
    if (e->header_pending && e->plen > 0) {
        cf_sink_puts(e->out, "\n[");
        cf_sink_write(e->out, e->path, e->plen);
        cf_sink_puts(e->out, "]\n");
        e->header_pending = CF_FALSE;
    }
    if (name != NULL) {
        cf_sink_write(e->out, name, n);
    }
    cf_sink_puts(e->out, " = ");

    if (v->type == CF_TYPE_TEXT) {
        /* Quoted when the value would otherwise lose its shape: it is
           the only escape INI has. */
        const char* p = v->u.t.p;
        size_t      i;
        cf_bool     need = (v->u.t.n == 0) ? CF_TRUE : CF_FALSE;
        for (i = 0; i < v->u.t.n; ++i) {
            if (p[i] == ' ' || p[i] == '\t' || p[i] == '"' || p[i] == '#' ||
                p[i] == ';' || p[i] == '\n') {
                need = CF_TRUE;
                break;
            }
        }
        if (need) {
            cf_sink_putc(e->out, '"');
            cf_sink_write(e->out, p, v->u.t.n);
            cf_sink_putc(e->out, '"');
        } else {
            cf_sink_write(e->out, p, v->u.t.n);
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
    e->in_array       = CF_FALSE;
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

    sink->begin_table = ini_enc_table;
    sink->begin_array = ini_enc_array;
    sink->scalar      = ini_enc_scalar;
    sink->end         = ini_enc_end;
    sink->ctx         = e;
    return CF_TRUE;
}

const cf_codec cf_codec_ini = {
    "ini",      ".ini;.cfg;.conf", CF_CODEC_READ | CF_CODEC_WRITE,
    ini_decode, ini_encoder,       sizeof(ini_enc)};

#endif /* CF_WITH_INI */
