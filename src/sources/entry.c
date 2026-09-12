/*
 * entry.c - key mapping, the entry sink, and the flat lexers. C89.
 */

#include "configforge/cf_entry.h"

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

cf_bool cf_lex_cli(int argc, char* const* argv, const cf_keymap* map,
                   const cf_entry_sink* out, cf_error* err) {
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
        const char* val;
        size_t      rawlen, klen;

        if (a == NULL) {
            continue;
        }
        if (a[0] != '-' || a[1] != '-') {
            continue; /* positional, not configuration */
        }
        if (a[2] == '\0') {
            break; /* "--" ends option parsing */
        }
        a += 2;

        eq = strchr(a, '=');
        if (eq != NULL) {
            rawlen = (size_t)(eq - a);
            val    = eq + 1;
        } else {
            rawlen = strlen(a);
            /* Only the "=" form carries a value at Tier 0. The separate
               "--key value" form cannot be parsed correctly without
               knowing which options take one, and a parser that guesses
               eats positional arguments. */
            val = "true";
        }

        klen = cf_keymap_to_key(map, a, rawlen, key, sizeof key);
        if (klen == 0) {
            return cf_error_set(err, CF_E_KEY, NULL, "cli",
                                "argv[%d]: '--%S' is not a valid option", i, a, rawlen);
        }
        cf_fmt(at, sizeof at, "argv[%d]", i);

        if (!out->text(out->ctx, key, klen, val, strlen(val), at)) {
            return cf_error_set(err, CF_E_MEMORY, key, at, "could not record '%s'",
                                key);
        }
    }
    return CF_TRUE;
}
