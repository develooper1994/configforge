/*
 * entry.c - key mapping, the entry sink, and the flat lexers. C89.
 */

#include "configforge/cf_entry.h"
#include "configforge/cf_serialize.h"

#include <string.h>

/* ================================================================== *
 * Key mapping
 * ================================================================== */

void cf_keymap_init(cf_keymap* m, cf_keymap_kind kind, const char* prefix) {
    if (m == NULL) {
        return;
    }
    m->kind   = kind;
    m->prefix = prefix;
}

size_t cf_keymap_to_key(const cf_keymap* m, const char* raw, size_t n, char* out,
                        size_t cap) {
    size_t plen = (m != NULL && m->prefix != NULL) ? strlen(m->prefix) : 0;
    size_t i, k = 0;

    if (m == NULL || raw == NULL || out == NULL || cap == 0) {
        return 0;
    }
    if (n < plen || (plen > 0 && memcmp(raw, m->prefix, plen) != 0)) {
        return 0;
    }
    if (n == plen) {
        return 0;
    }

    if (m->kind == CF_KEYMAP_DIRECT) {
        if (n - plen + 1 > cap) {
            return 0;
        }
        memcpy(out, raw + plen, n - plen);
        k      = n - plen;
        out[k] = '\0';
        return cf_key_valid(out, k) ? k : 0;
    }

    for (i = plen; i < n; ++i) {
        char c = raw[i];
        if (k + 2 > cap) {
            return 0;
        }
        if (c == '_' && i + 1 < n && raw[i + 1] == '_') {
            out[k++] = '.';
            ++i;
            continue;
        }
        out[k++] = (char)cf_to_lower((unsigned char)c);
    }
    out[k] = '\0';
    return cf_key_valid(out, k) ? k : 0;
}

size_t cf_keymap_from_key(const cf_keymap* m, const char* key, size_t n,
                          cf_bool escaped, char* out, size_t cap) {
    size_t plen = (m != NULL && m->prefix != NULL) ? strlen(m->prefix) : 0;
    size_t i, k = 0;

    if (m == NULL || key == NULL || out == NULL || cap == 0) {
        return 0;
    }
    if (plen + 1 > cap) {
        return 0;
    }
    if (plen > 0) {
        memcpy(out, m->prefix, plen);
        k = plen;
    }

    if (m->kind == CF_KEYMAP_DIRECT) {
        if (k + n + 1 > cap) {
            return 0;
        }
        memcpy(out + k, key, n);
        k += n;
        out[k] = '\0';
        return k;
    }

    for (i = 0; i < n; ++i) {
        char c = key[i];
        if (k + 3 > cap) {
            return 0;
        }
        if (c == '.') {
            out[k++] = '_';
            if (escaped) {
                out[k++] = '_';
            }
            continue;
        }
        out[k++] = (char)cf_to_upper((unsigned char)c);
    }
    out[k] = '\0';
    return k;
}

/* ================================================================== *
 * The sink that records into a key space
 * ================================================================== */

static cf_bool keysink_text(void* ctx, const char* key, size_t klen, const char* value,
                            size_t vlen, const char* at) {
    cf_keysink* k = (cf_keysink*)ctx;

    if (!cf_ks_add_text(k->ks, key, klen, k->rank, value, vlen, at)) {
        k->failed = CF_TRUE;
        return CF_FALSE;
    }
    ++k->count;
    return CF_TRUE;
}

static cf_bool keysink_typed(void* ctx, const char* key, size_t klen, const cf_value* v,
                             const char* at) {
    cf_keysink* k = (cf_keysink*)ctx;

    if (!cf_ks_add(k->ks, key, klen, k->rank, v, at)) {
        k->failed = CF_TRUE;
        return CF_FALSE;
    }
    ++k->count;
    return CF_TRUE;
}

cf_bool cf_keysink_init(cf_keysink* k, cf_keyspace* ks, int rank) {
    if (k == NULL || ks == NULL) {
        return CF_FALSE;
    }
    k->ks         = ks;
    k->rank       = rank;
    k->count      = 0;
    k->failed     = CF_FALSE;
    k->sink.text  = keysink_text;
    k->sink.typed = keysink_typed;
    k->sink.ctx   = k;
    return CF_TRUE;
}

const cf_entry_sink* cf_keysink_of(cf_keysink* k) {
    return (k != NULL) ? &k->sink : NULL;
}

/* ================================================================== *
 * Lexers
 * ================================================================== */

cf_bool cf_lex_env(char* const* environment, const cf_keymap* map,
                   const cf_entry_sink* out, cf_error* err) {
    char         key[CF_KEY_MAX];
    char         at[CF_KEY_MAX];
    char* const* e;

    if (out == NULL || map == NULL) {
        return cf_error_set(err, CF_E_SOURCE, NULL, "env",
                            "nothing to read the environment into");
    }
    if (environment == NULL) {
        return CF_TRUE; /* no environment is not an error */
    }
    for (e = environment; *e != NULL; ++e) {
        const char* line = *e;
        const char* eq   = strchr(line, '=');
        size_t      namelen, klen;

        if (eq == NULL) {
            continue;
        }
        namelen = (size_t)(eq - line);
        klen    = cf_keymap_to_key(map, line, namelen, key, sizeof key);
        if (klen == 0) {
            continue;
        }
        /* The location of an environment value is the variable name:
           what a reader would have to change, not the word
           "environment". */
        if (namelen + 1 > sizeof at) {
            continue;
        }
        memcpy(at, line, namelen);
        at[namelen] = '\0';

        if (!out->text(out->ctx, key, klen, eq + 1, strlen(eq + 1), at)) {
            return cf_error_set(err, CF_E_MEMORY, key, at, "could not record '%s'",
                                key);
        }
    }
    return CF_TRUE;
}

/* Classification is asked for, never guessed. Tier 0 passes no spec and
   gets the two forms that need no knowledge. */
static cf_cli_kind cli_classify(const cf_cli_spec* spec, const char* key, size_t klen) {
    if (spec == NULL || spec->classify == NULL) {
        return CF_CLI_UNKNOWN;
    }
    return spec->classify(spec->ctx, key, klen);
}

/* An alias is resolved before anything else, so "-p" and "--port" reach
   the rest of the lexer as the canonical key they stand for. */
static size_t cli_alias(const cf_cli_spec* spec, const char* raw, size_t n, char* out,
                        size_t cap) {
    if (spec == NULL || spec->alias == NULL) {
        return 0;
    }
    return spec->alias(spec->ctx, raw, n, out, cap);
}

static cf_bool cli_is_option(const char* a) {
    return (a != NULL && a[0] == '-' && a[1] != '\0') ? CF_TRUE : CF_FALSE;
}

cf_bool cf_lex_cli(int argc, char* const* argv, const cf_keymap* map,
                   const cf_cli_spec* spec, const cf_entry_sink* out, cf_error* err) {
    char key[CF_KEY_MAX];
    char at[32];
    int  i;

    if (out == NULL || map == NULL) {
        return cf_error_set(err, CF_E_SOURCE, NULL, "cli",
                            "nothing to read the command line into");
    }
    if (argv == NULL) {
        return CF_TRUE;
    }
    for (i = 1; i < argc; ++i) {
        const char* a = argv[i];
        const char* eq;
        const char* val = NULL;
        const char* raw;
        size_t      rawlen, klen;
        cf_cli_kind kind;
        cf_bool     negated = CF_FALSE;
        cf_bool     had_eq;

        if (a == NULL) {
            continue;
        }
        if (a[0] != '-' || a[1] == '\0') {
            continue; /* positional, not configuration */
        }
        if (a[1] == '-' && a[2] == '\0') {
            break; /* "--" ends option parsing */
        }

        if (a[1] == '-') {
            raw = a + 2;
        } else {
            /* A single dash is an alias, and only a declaration can say
               what it stands for. Bundled short flags such as -abc are
               deliberately absent: unbundling them requires knowing that
               every letter is a flag, and being wrong eats a value. */
            raw = a + 1;
        }

        eq     = strchr(raw, '=');
        had_eq = (eq != NULL) ? CF_TRUE : CF_FALSE;
        rawlen = had_eq ? (size_t)(eq - raw) : strlen(raw);
        if (had_eq) {
            val = eq + 1;
        }

        klen = cli_alias(spec, raw, rawlen, key, sizeof key);
        if (klen == 0) {
            klen = cf_keymap_to_key(map, raw, rawlen, key, sizeof key);
        }

        /* --no-key negates a declared flag. Only a declared one: a key
           legitimately named "no-proxy" exists, and without a contract
           there is no way to tell the two apart, so without one the
           literal reading wins. */
        if (klen == 0 && rawlen > 3 && memcmp(raw, "no-", 3) == 0) {
            size_t blen = cf_keymap_to_key(map, raw + 3, rawlen - 3, key, sizeof key);
            if (blen != 0 && cli_classify(spec, key, blen) == CF_CLI_FLAG) {
                klen    = blen;
                negated = CF_TRUE;
            }
        } else if (!had_eq && rawlen > 3 && memcmp(raw, "no-", 3) == 0 &&
                   cli_classify(spec, key, klen) == CF_CLI_UNKNOWN) {
            size_t blen = 0;
            char   base[CF_KEY_MAX];
            blen = cf_keymap_to_key(map, raw + 3, rawlen - 3, base, sizeof base);
            if (blen != 0 && cli_classify(spec, base, blen) == CF_CLI_FLAG) {
                memcpy(key, base, blen + 1);
                klen    = blen;
                negated = CF_TRUE;
            }
        }

        if (klen == 0) {
            return cf_error_set(err, CF_E_KEY, NULL, "cli",
                                "argv[%d]: '%s' is not a valid option", i, a);
        }

        kind = cli_classify(spec, key, klen);

        if (negated) {
            val = "false";
        } else if (!had_eq) {
            if (kind == CF_CLI_VALUE) {
                /* Safe only because a declaration said this option takes
                   a value. Without one the next argument might be the
                   program's, and eating it is the classic way a parser
                   that guesses ruins a command line. */
                if (i + 1 >= argc || argv[i + 1] == NULL ||
                    cli_is_option(argv[i + 1])) {
                    return cf_error_set(err, CF_E_KEY, key, "cli",
                                        "argv[%d]: '%s' needs a value", i, a);
                }
                val = argv[++i];
            } else {
                val = "true";
            }
        }

        /* The location is the argument index: it is what identifies the
           occurrence when the same option appears twice. */
        cf_fmt(at, sizeof at, "argv[%d]", i);

        if (!out->text(out->ctx, key, klen, val, strlen(val), at)) {
            return cf_error_set(err, CF_E_MEMORY, key, at, "could not record '%s'",
                                key);
        }
    }
    return CF_TRUE;
}

/* ------------------------------------------------------------------ *
 * Response files
 * ------------------------------------------------------------------ */

typedef struct argv_build {
    char**    v;
    int       n;
    int       cap;
    cf_arena* arena;
} argv_build;

static cf_bool argv_push(argv_build* b, const char* p, size_t len) {
    char* copy;

    if (b->n == b->cap) {
        int    ncap = (b->cap < 16) ? 16 : b->cap * 2;
        char** nv   = (char**)cf_arena_alloc(b->arena, (size_t)ncap * sizeof(char*));
        if (nv == NULL) {
            return CF_FALSE;
        }
        if (b->n > 0) {
            memcpy((void*)nv, (const void*)b->v, (size_t)b->n * sizeof(char*));
        }
        b->v   = nv;
        b->cap = ncap;
    }
    copy = cf_arena_dup(b->arena, p, len);
    if (copy == NULL) {
        return CF_FALSE;
    }
    b->v[b->n++] = copy;
    return CF_TRUE;
}

cf_bool cf_cli_expand(int argc, char* const* argv, cf_arena* arena, int* out_argc,
                      char*** out_argv, cf_error* err) {
    argv_build b;
    int        i;

    if (argv == NULL || arena == NULL || out_argc == NULL || out_argv == NULL) {
        return cf_error_set(err, CF_E_SOURCE, NULL, "cli", "nothing to expand");
    }
    b.v     = NULL;
    b.n     = 0;
    b.cap   = 0;
    b.arena = arena;

    for (i = 0; i < argc; ++i) {
        const char* a = argv[i];
        cf_blob     blob;
        size_t      at = 0;

        if (a == NULL) {
            continue;
        }
        /* Only past argv[0], and never after "--": everything beyond
           that belongs to the program, and an argument that happens to
           start with @ is its business. */
        if (i == 0 || a[0] != '@' || a[1] == '\0') {
            if (!argv_push(&b, a, strlen(a))) {
                return cf_error_set(err, CF_E_MEMORY, NULL, "cli",
                                    "no room to expand the command line");
            }
            if (a[0] == '-' && a[1] == '-' && a[2] == '\0') {
                /* Copy the rest verbatim. */
                for (++i; i < argc; ++i) {
                    if (argv[i] != NULL && !argv_push(&b, argv[i], strlen(argv[i]))) {
                        return cf_error_set(err, CF_E_MEMORY, NULL, "cli",
                                            "no room to expand");
                    }
                }
                break;
            }
            continue;
        }

        if (!cf_blob_file(&blob, arena, a + 1, err)) {
            return CF_FALSE;
        }
        while (at < blob.n) {
            size_t      start = at;
            const char* line;
            size_t      len;

            while (at < blob.n && blob.p[at] != '\n') {
                ++at;
            }
            len = at - start;
            while (len > 0 &&
                   (blob.p[start + len - 1] == '\r' || blob.p[start + len - 1] == ' ' ||
                    blob.p[start + len - 1] == '\t')) {
                --len;
            }
            line = blob.p + start;
            while (len > 0 && (*line == ' ' || *line == '\t')) {
                ++line;
                --len;
            }
            if (at < blob.n) {
                ++at;
            }
            if (len == 0 || line[0] == '#') {
                continue;
            }
            if (line[0] == '@') {
                /* A response file that includes another is a command
                   line nobody can follow from the one they typed, and
                   one that includes itself is a loop. */
                return cf_error_set(err, CF_E_SOURCE, NULL, a + 1,
                                    "%s: a response file may not include "
                                    "another",
                                    a + 1);
            }
            if (!argv_push(&b, line, len)) {
                return cf_error_set(err, CF_E_MEMORY, NULL, a + 1,
                                    "no room to expand '%s'", a + 1);
            }
        }
    }
    *out_argc = b.n;
    *out_argv = b.v;
    return CF_TRUE;
}

/* ------------------------------------------------------------------ *
 * A .env file
 *
 * An environment written down, so it takes the same key mapping and the
 * escape and the natural spelling behave identically in both.
 * ------------------------------------------------------------------ */

#if CF_WITH_DOTENV

static size_t dotenv_trim(const char* p, size_t n, const char** out) {
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

cf_bool cf_lex_dotenv(const char* text, size_t n, const char* origin,
                      const cf_keymap* map, const cf_entry_sink* out, cf_error* err) {
    char         key[CF_KEY_MAX];
    char         at[CF_KEY_MAX];
    size_t       i    = 0;
    unsigned int line = 1;

    if (text == NULL || out == NULL || map == NULL) {
        return cf_error_set(err, CF_E_SOURCE, NULL, origin,
                            "nothing to read the file into");
    }
    if (origin == NULL) {
        origin = ".env";
    }
    while (i < n) {
        size_t      start = i;
        size_t      len;
        const char* raw;
        const char* eq;
        const char* name;
        const char* val;
        size_t      namelen, vallen, klen;

        while (i < n && text[i] != '\n') {
            ++i;
        }
        len = dotenv_trim(text + start, i - start, &raw);
        if (i < n) {
            ++i;
        }
        if (len == 0 || raw[0] == '#' || raw[0] == ';') {
            ++line;
            continue;
        }
        /* "export KEY=value" is what a shell-sourced file looks like.
           Accepting and ignoring the word costs three lines and saves
           every user who copied their file from somewhere. */
        if (len > 7 && memcmp(raw, "export ", 7) == 0) {
            len = dotenv_trim(raw + 7, len - 7, &raw);
        }

        eq = (const char*)memchr(raw, '=', len);
        if (eq == NULL) {
            return cf_error_set(err, CF_E_SOURCE, NULL, origin,
                                "%s:%u: expected NAME=value", origin, line);
        }
        namelen = dotenv_trim(raw, (size_t)(eq - raw), &name);
        vallen  = dotenv_trim(eq + 1, len - (size_t)(eq - raw) - 1, &val);

        if (vallen >= 2 && ((val[0] == '"' && val[vallen - 1] == '"') ||
                            (val[0] == '\'' && val[vallen - 1] == '\''))) {
            ++val;
            vallen -= 2;
        }
        klen = cf_keymap_to_key(map, name, namelen, key, sizeof key);
        if (klen == 0) {
            /* Unlike the environment, this file belongs to this program,
               so a name that derives no key is a mistake rather than
               somebody else's variable. */
            return cf_error_set(err, CF_E_KEY, NULL, origin,
                                "%s:%u: '%S' is not a usable name", origin, line, name,
                                namelen);
        }
        cf_fmt(at, sizeof at, "%s:%u", origin, line);
        if (!out->text(out->ctx, key, klen, val, vallen, at)) {
            return cf_error_set(err, CF_E_MEMORY, key, at, "could not record '%s'",
                                key);
        }
        ++line;
    }
    return CF_TRUE;
}

#else /* !CF_WITH_DOTENV */

cf_bool cf_lex_dotenv(const char* text, size_t n, const char* origin,
                      const cf_keymap* map, const cf_entry_sink* out, cf_error* err) {
    (void)text;
    (void)n;
    (void)map;
    (void)out;
    return cf_error_set(err, CF_E_CAPABILITY, NULL, origin,
                        "this build was compiled without CF_WITH_DOTENV");
}

#endif /* CF_WITH_DOTENV */
