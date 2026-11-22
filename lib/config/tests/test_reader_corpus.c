/*
 * test_reader_corpus.c - every reader, every short document over a small
 * alphabet, hashed (A-7, D-102).
 *
 * The proof that a reader is read the same after a refactor: for JSON,
 * JSON5, TOML, the built-in YAML, INI, .properties and dotenv, each
 * document of length 0 to the run's length over the run's alphabet is
 * decoded into a sink that writes down every event (its kind, its name,
 * its text and its place) and, at the end, the result and the message.
 * That text is hashed (FNV-1a, 64 bits); the hashes of one run and
 * length are hashed again into one number. The golden file holds one
 * line a run and length:  json 4 0x....
 *
 *     test_reader_corpus                       compare with the golden
 *     test_reader_corpus --write               rewrite the golden
 *     test_reader_corpus --dump <run> <length> print each document's result
 *
 * The golden is written on the code before a change and checked in; this
 * test must pass before and after it.
 */

#include <configforge/cf_entry.h>
#include <configforge/cf_yamlparse.h>
#include <configforge/configforge.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cf_test.h"

#ifndef CF_READER_CORPUS_GOLDEN
#define CF_READER_CORPUS_GOLDEN "reader_corpus.golden"
#endif

/* ------------------------------------------------------------------ *
 * What a document's result is written as
 * ------------------------------------------------------------------ */

static char   g_buf[1 << 14];
static size_t g_len;
static char   g_text[1 << 12];
static char   g_work[1 << 16];
static size_t g_work_used;

static void put_n(const char* p, size_t n) {
    size_t i;
    for (i = 0; i < n && g_len < sizeof g_buf; ++i) {
        g_buf[g_len++] = p[i];
    }
}
static void put_s(const char* s) {
    put_n(s, strlen(s));
}
static void put_hex64(cf_u64 v) {
    static const char digits[] = "0123456789abcdef";
    int               i;
    for (i = 15; i >= 0; --i) {
        char c = digits[(unsigned)((v >> (4 * i)) & 15u)];
        put_n(&c, 1);
    }
}
static void put_loc(const cf_loc* at) {
    char tmp[64];
    if (at == NULL) {
        put_s("-");
        return;
    }
    sprintf(tmp, "%u:%u", at->line, at->col);
    put_s(tmp);
}

static cf_bool ev_table(void* ctx, const char* name, size_t n) {
    (void)ctx;
    put_s("T");
    put_n(name, n);
    put_s("|");
    return CF_TRUE;
}
static cf_bool ev_array(void* ctx, const char* name, size_t n) {
    (void)ctx;
    put_s("A");
    put_n(name, n);
    put_s("|");
    return CF_TRUE;
}
static cf_bool ev_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                         cf_bool textual, const cf_loc* at) {
    (void)ctx;
    put_s(textual ? "S" : "V");
    put_n(name, n);
    put_s("=");
    switch (v->type) {
    case CF_TYPE_NULL:
        put_s("n");
        break;
    case CF_TYPE_BOOL:
        put_s(v->u.b ? "t" : "f");
        break;
    case CF_TYPE_INT:
        put_hex64((cf_u64)v->u.i);
        break;
    case CF_TYPE_REAL: {
#if CF_WITH_REAL
        cf_u64 bits = 0;
        memcpy(&bits, &v->u.r,
               sizeof bits < sizeof v->u.r ? sizeof bits : sizeof v->u.r);
        put_hex64(bits);
#else
        put_s("r");
#endif
        break;
    }
    case CF_TYPE_TEXT:
        put_n(v->u.t.p, v->u.t.n);
        break;
    }
    put_s("@");
    put_loc(at);
    put_s("|");
    return CF_TRUE;
}
static cf_bool ev_end(void* ctx) {
    (void)ctx;
    put_s("E|");
    return CF_TRUE;
}
static char* ev_reserve_text(void* ctx, size_t n) {
    (void)ctx;
    return (n <= sizeof g_text) ? g_text : NULL;
}
static void* ev_reserve_work(void* ctx, size_t n) {
    void* p;
    (void)ctx;
    n = (n + 15u) & ~(size_t)15u;
    if (n > sizeof g_work - g_work_used) {
        return NULL;
    }
    p = g_work + g_work_used;
    g_work_used += n;
    return p;
}

static cf_bool en_text(void* ctx, const char* key, size_t klen, const char* value,
                       size_t vlen, const char* at) {
    (void)ctx;
    put_s("K");
    put_n(key, klen);
    put_s("=");
    put_n(value, vlen);
    put_s("@");
    put_s(at != NULL ? at : "-");
    put_s("|");
    return CF_TRUE;
}
static cf_bool en_typed(void* ctx, const char* key, size_t klen, const cf_value* v,
                        const char* at) {
    (void)ctx;
    (void)v;
    put_s("Y");
    put_n(key, klen);
    put_s("@");
    put_s(at != NULL ? at : "-");
    put_s("|");
    return CF_TRUE;
}

/* The document through the reader called `name` ("dotenv" is no codec),
   its whole result in g_buf. */
static void read_one(const char* name, const char* doc, size_t n) {
    cf_error err = CF_ERROR_INIT;
    cf_bool  ok;
    char     origin[32];

    g_len       = 0;
    g_work_used = 0;
    sprintf(origin, "corpus.%s", name);
    if (strcmp(name, "dotenv") == 0) {
        cf_keymap     map;
        cf_entry_sink sink;
        sink.text  = en_text;
        sink.typed = en_typed;
        sink.ctx   = NULL;
        cf_keymap_init(&map, CF_KEYMAP_ENV, NULL);
        ok = cf_lex_dotenv(doc, n, origin, &map, &sink, &err);
    } else {
        const cf_codec* codec = cf_codec_by_name(name);
        cf_blob         b;
        cf_doc_sink     sink;

        if (codec == NULL) {
            put_s("nocodec");
            return;
        }
        cf_blob_mem(&b, doc, n, origin);
        memset(&sink, 0, sizeof sink);
        sink.size         = sizeof(cf_doc_sink);
        sink.begin_table  = ev_table;
        sink.begin_array  = ev_array;
        sink.scalar       = ev_scalar;
        sink.end          = ev_end;
        sink.reserve_text = ev_reserve_text;
        sink.reserve_work = ev_reserve_work;
        ok                = cf_codec_decode(codec, &b, &sink, &err);
    }
    put_s(ok ? "ok" : "no");
    put_s("|");
    put_s(err.message);
}

/* ------------------------------------------------------------------ *
 * The runs
 * ------------------------------------------------------------------ */

#define LS "\xE2\x80\xA8"
#define FF "\f"

typedef struct run {
    const char*        label;  /* the golden's name */
    const char*        reader; /* codec name, or "dotenv" */
    const char* const* alpha;
    size_t             symbols;
    unsigned           length;
} run;

static const char* const a_json[]   = {"{",  "}",  "\"", ":", ",", "a",  "0", " ",
                                       "\t", "\r", "\n", "/", "*", "\\", "u", LS};
static const char* const a_json5b[] = {"'", "x",  "1",  "v",  "\\", "u", "0", "D",
                                       "8", "\r", "\n", "\"", "{",  "}", ":", "a"};
static const char* const a_toml[]   = {"[",  "]",  "=",  "a",  "1", "\"", "'", "#", " ",
                                       "\t", "\r", "\n", "\\", ",", "{",  "}", "."};
static const char* const a_tomlb[]  = {"\"", "\\", "u",  "x",  "U", "0", "D", "8",
                                       "e",  "b",  "\n", "\r", "=", "a", " ", "'"};
static const char* const a_yaml[]   = {"a",  ":",  " ", "\t", "-", "#", "\n",
                                       "\r", "\"", "'", "\\", "|", "[", "]",
                                       "{",  "}",  ",", "!",  "%"};
static const char* const a_yamlb[]  = {"\"", "\\", "x",  "u",  "U", "0", "D", "8",
                                       "N",  "L",  "\n", "\r", "a", ":", " ", "/"};
static const char* const a_ini[]    = {"[",  "]",  "=", "a", " ", "\t",
                                       "\r", "\n", "#", ";", "\""};
static const char* const a_prop[]   = {"a",  "=", ":", " ",  "\t", FF, "\r",
                                       "\n", "#", "!", "\\", "u",  "0"};
static const char* const a_propb[]  = {"\\", "u", "D", "8", "0",  "4",
                                       "1",  "t", "=", "a", "\n", "\r"};
static const char* const a_env[]    = {"A", "=", " ", "\t", "\r", "\n", "#", "\"", "'"};

#define N(a) (sizeof(a) / sizeof((a)[0]))
static const run g_runs[] = {{"json", "json", a_json, N(a_json), 4},
                             {"json5", "json5", a_json, N(a_json), 4},
                             {"json5b", "json5", a_json5b, N(a_json5b), 4},
                             {"toml", "toml", a_toml, N(a_toml), 4},
                             {"tomlb", "toml", a_tomlb, N(a_tomlb), 4},
                             {"yaml", "yaml", a_yaml, N(a_yaml), 4},
                             {"yamlb", "yaml", a_yamlb, N(a_yamlb), 4},
                             {"ini", "ini", a_ini, N(a_ini), 5},
                             {"properties", "properties", a_prop, N(a_prop), 4},
                             {"propertiesb", "properties", a_propb, N(a_propb), 4},
                             {"dotenv", "dotenv", a_env, N(a_env), 5}};

/* The 64-bit FNV prime is 0x100000001B3, which a 32-bit literal cannot
   spell on every compiler: built from its halves. */
static cf_u64 fnv_prime(void) {
    return ((cf_u64)0x100u << 32) | (cf_u64)0x1B3u;
}
static cf_u64 fnv64(cf_u64 h, const char* p, size_t n) {
    size_t i;
    cf_u64 prime = fnv_prime();
    for (i = 0; i < n; ++i) {
        h ^= (cf_u64)(unsigned char)p[i];
        h *= prime;
    }
    return h;
}
static cf_u64 fnv_basis(void) {
    return ((cf_u64)0xCBF29CE4u << 32) | (cf_u64)0x84222325u;
}

/* One line for the run at one length; with `dump`, each document too. */
static cf_u64 run_length(const run* r, unsigned len, int dump) {
    cf_u64 total = fnv_basis();
    size_t count = 1, i, k;
    size_t idx[8];
    char   doc[64];

    for (i = 0; i < len; ++i) {
        count *= r->symbols;
    }
    for (i = 0; i < count; ++i) {
        size_t n = 0, rest = i;
        cf_u64 h;
        char   bytes[8];

        for (k = 0; k < len; ++k) {
            idx[k] = rest % r->symbols;
            rest /= r->symbols;
        }
        for (k = 0; k < len; ++k) {
            const char* s = r->alpha[idx[k]];
            size_t      m = strlen(s);
            memcpy(doc + n, s, m);
            n += m;
        }
        read_one(r->reader, doc, n);
        h = fnv64(fnv_basis(), g_buf, g_len);
        if (dump) {
            size_t j;
            printf("doc \"");
            for (j = 0; j < n; ++j) {
                unsigned char c = (unsigned char)doc[j];
                if (c >= 32 && c < 127 && c != '\\') {
                    putchar(c);
                } else {
                    printf("\\x%02x", (unsigned)c);
                }
            }
            printf("\" -> %.*s\n", (int)g_len, g_buf);
        }
        for (k = 0; k < 8; ++k) {
            bytes[k] = (char)((h >> (8 * k)) & 0xFFu);
        }
        total = fnv64(total, bytes, 8);
    }
    return total;
}

static void line_for(const run* r, unsigned len, char* out) {
    cf_u64 h = run_length(r, len, 0);
    sprintf(out, "%s %u 0x%08lx%08lx\n", r->label, len, (unsigned long)(h >> 32),
            (unsigned long)(h & 0xFFFFFFFFu));
}

static char g_got[1 << 14];

static void all_lines(void) {
    size_t   i;
    unsigned len;
    size_t   used = 0;
    for (i = 0; i < N(g_runs); ++i) {
#if CF_YAML_LIBFYAML || CF_YAML_LIBYAML
        if (strcmp(g_runs[i].reader, "yaml") == 0) {
            continue; /* the golden is the built-in parser's */
        }
#endif
        for (len = 0; len <= g_runs[i].length; ++len) {
            char line[96];
            line_for(&g_runs[i], len, line);
            memcpy(g_got + used, line, strlen(line) + 1);
            used += strlen(line);
        }
    }
}

int main(int argc, char** argv) {
    FILE*       f;
    static char want[1 << 14];
    size_t      n;

    volatile unsigned key_max     = CF_KEY_MAX;
    volatile unsigned message_max = CF_MESSAGE_MAX;
    volatile unsigned path_max    = CF_PATH_MAX;
    volatile unsigned max_depth   = CF_MAX_DEPTH;
    volatile unsigned with_real   = CF_WITH_REAL;
    volatile unsigned with_dotenv = CF_WITH_DOTENV;

    if (argc >= 4 && strcmp(argv[1], "--dump") == 0) {
        size_t i;
        for (i = 0; i < N(g_runs); ++i) {
            if (strcmp(g_runs[i].label, argv[2]) == 0) {
                (void)run_length(&g_runs[i], (unsigned)atoi(argv[3]), 1);
                return 0;
            }
        }
        printf("no run called %s\n", argv[2]);
        return 1;
    }
    if (key_max != 256u || message_max != 256u || path_max != 512u ||
        max_depth != 32u || !with_real || !with_dotenv) {
        printf("skipped: the golden is the default build's "
               "(limits %u/%u/%u/%u, CF_WITH_REAL=%u, CF_WITH_DOTENV=%u)\n",
               key_max, message_max, path_max, max_depth, with_real, with_dotenv);
        return 0;
    }
    all_lines();
    if (argc >= 2 && strcmp(argv[1], "--write") == 0) {
        f = fopen(CF_READER_CORPUS_GOLDEN, "wb");
        if (f == NULL) {
            printf("cannot write %s\n", CF_READER_CORPUS_GOLDEN);
            return 1;
        }
        fwrite(g_got, 1, strlen(g_got), f);
        fclose(f);
        return 0;
    }
    f = fopen(CF_READER_CORPUS_GOLDEN, "rb");
    check(f != NULL, "the golden file is there");
    if (f == NULL) {
        return cf_test_done();
    }
    n       = fread(want, 1, sizeof want - 1, f);
    want[n] = '\0';
    fclose(f);
    {
        const char* g = g_got;
        while (*g != '\0') {
            const char* ge     = strchr(g, '\n');
            size_t      gl     = (size_t)(ge - g);
            size_t      prefix = (size_t)(strstr(g, "0x") - g);
            const char* w      = want;
            int         same   = 0;
            /* The line of the golden with this run and length (a run the
               build leaves out has none to compare). */
            while (*w != '\0') {
                const char* we = strchr(w, '\n');
                size_t      wl = (we != NULL) ? (size_t)(we - w) : strlen(w);
                if (wl >= prefix && memcmp(g, w, prefix) == 0) {
                    same = (wl == gl && memcmp(g, w, gl) == 0);
                    break;
                }
                w += wl + (we != NULL ? 1u : 0u);
            }
            if (!same) {
                char what[128];
                sprintf(what, "reader corpus differs: %.*s", (int)gl, g);
                check(0, what);
            }
            g = ge + 1;
        }
    }
    return cf_test_done();
}
