/*
 * dotnet.c - Doctor's adapter for .NET's IConfigurationRoot.GetDebugView().
 * C89.
 *
 *     Console.Write(((IConfigurationRoot)config).GetDebugView());
 *     ... | doctor import-dotnet > app.json
 *
 * GetDebugView prints the configuration as .NET resolved it, one key a
 * line, indented two spaces a level, each value with the provider that
 * won it:
 *
 *     Logging:
 *       LogLevel:
 *         Default=Information (JsonConfigurationProvider for 'appsettings.json'
 * (Optional)) ConnectionStrings: Main=Server=db
 * (EnvironmentVariablesConfigurationProvider Prefix: '')
 *
 * That is presence and origin, and only the winner: .NET does not say
 * which providers lost. So the report this writes has one candidate a
 * key, and the rules that need a chain - an unreachable value, a dead
 * source - have nothing to say about it, which Doctor.md (section 12)
 * says of every adapter that cannot supply more. It resolves nothing:
 *
 *   keys       .NET's keys are case-insensitive, so they are lowered,
 *              and ':' becomes '.'; one a report key still cannot carry
 *              is left out and counted.
 *   providers  each provider is a source, named as .NET names it, in the
 *              order it was first printed, which says nothing about
 *              precedence and is not used as if it did. Its kind is the
 *              provider's own type - "commandline", "environment",
 *              "json" - and not Doctor's "cli" or "env": the report is
 *              blind, every key in it marked sensitive, and CFD007 would
 *              otherwise call every command-line key a secret.
 *   values     blind unless --full: GetDebugView prints every value in
 *              clear. What a blind .NET report is for is comparison -
 *              `doctor diff` of two environments, by digest.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#include "adapters.h"

#define DOTNET_SOURCES CF_MAX_SOURCES
#define DOTNET_DEPTH CF_MAX_DEPTH

static int dotnet_usage(void) {
    fputs("doctor import-dotnet [--full] [--profile <name>]\n"
          "\n"
          "Reads the text of .NET's IConfigurationRoot.GetDebugView() on\n"
          "stdin and writes a provenance report on stdout, value-blind\n"
          "unless --full. .NET prints only the provider that won each key,\n"
          "so the report has the winners and not the chain.\n",
          stderr);
    return 2;
}

/* The provider's type says what kind of source it is. */
static const char* dotnet_kind(const char* provider) {
    static const struct {
        const char* type;
        const char* kind;
    } kinds[] = {{"CommandLineConfigurationProvider", "commandline"},
                 {"EnvironmentVariablesConfigurationProvider", "environment"},
                 {"JsonConfigurationProvider", "json"},
                 {"IniConfigurationProvider", "ini"},
                 {"XmlConfigurationProvider", "xml"},
                 {"KeyPerFileConfigurationProvider", "keyperfile"},
                 {"MemoryConfigurationProvider", "memory"}};
    size_t i;

    for (i = 0; i < sizeof kinds / sizeof kinds[0]; ++i) {
        if (strncmp(provider, kinds[i].type, strlen(kinds[i].type)) == 0) {
            return kinds[i].kind;
        }
    }
    return "dotnet";
}

/* The provider is the parenthesised group that ends the line, counted
   from the right, because a provider's own name holds parentheses -
   "(Optional)" - and a value may too. Returns where the " (" begins, or
   NULL. */
static const char* dotnet_provider(const char* line, size_t n, const char** prov,
                                   size_t* plen) {
    int    depth = 0;
    size_t i;

    if (n < 3 || line[n - 1] != ')') {
        return NULL;
    }
    for (i = n; i > 0; --i) {
        char c = line[i - 1];
        if (c == ')') {
            ++depth;
        } else if (c == '(') {
            if (--depth == 0) {
                if (i < 2 || line[i - 2] != ' ') {
                    return NULL;
                }
                *prov = line + i;
                *plen = n - i - 1;
                return line + i - 2;
            }
        }
    }
    return NULL;
}

int doctor_import_dotnet(int argc, char** argv) {
    static char             line[8192];
    static char             path[DOTNET_DEPTH][CF_KEY_MAX];
    static size_t           plens[DOTNET_DEPTH];
    static char*            names[DOTNET_SOURCES];
    static cf_source        src[DOTNET_SOURCES];
    static cf_source_vtable vts[DOTNET_SOURCES];
    const cf_source*        view[DOTNET_SOURCES];
    cf_arena*               ar = cf_arena_new(NULL, 1 << 16);
    cf_keyspace             ks;
    cf_defset               ds;
    cf_source_view          sv;
    cf_report_info          info;
    cf_bool                 full     = CF_FALSE;
    const char*             profile  = NULL;
    size_t                  nsources = 0;
    unsigned long           skipped  = 0;
    char                    first_skipped[CF_KEY_MAX];
    int                     lineno = 0, k;
    size_t                  i, n;
    char*                   out;

    for (k = 0; k < argc; ++k) {
        if (strcmp(argv[k], "--full") == 0) {
            full = CF_TRUE;
        } else if (strcmp(argv[k], "--profile") == 0 && k + 1 < argc) {
            profile = argv[++k];
        } else {
            cf_arena_free(ar);
            return dotnet_usage();
        }
    }
    if (ar == NULL || !cf_ks_init(&ks, ar, 256) || !cf_defset_init(&ds, ar, 256)) {
        fprintf(stderr, "doctor: cannot start\n");
        cf_arena_free(ar);
        return 3;
    }
    first_skipped[0] = '\0';

    while (fgets(line, (int)sizeof line, stdin) != NULL) {
        size_t      len    = strlen(line);
        size_t      indent = 0, level, j;
        const char* p;
        const char* eq;
        const char* cut;
        const char* prov    = NULL;
        size_t      provlen = 0, namelen;
        char        key[CF_KEY_MAX];
        size_t      klen   = 0;
        cf_bool     ok     = CF_TRUE;
        int         source = -1;
        cf_value    v;

        ++lineno;
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0) {
            continue;
        }
        while (indent < len && line[indent] == ' ') {
            ++indent;
        }
        if (indent % 2 != 0 || indent / 2 >= DOTNET_DEPTH) {
            fprintf(stderr, "doctor: stdin:%d: not GetDebugView's indentation\n",
                    lineno);
            cf_arena_free(ar);
            return 2;
        }
        level = indent / 2;
        p     = line + indent;
        len -= indent;

        /* "Name:" opens a section; "Name=value (Provider)" is a value,
           and may open a section as well. */
        eq = (const char*)memchr(p, '=', len);
        if (eq == NULL) {
            if (p[len - 1] != ':') {
                fprintf(stderr,
                        "doctor: stdin:%d: expected Key=Value (Provider) or Key:\n",
                        lineno);
                cf_arena_free(ar);
                return 2;
            }
            namelen = len - 1;
        } else {
            namelen = (size_t)(eq - p);
        }
        if (namelen + 1 > CF_KEY_MAX) {
            namelen = CF_KEY_MAX - 1;
            ok      = CF_FALSE;
        }
        memcpy(path[level], p, namelen);
        path[level][namelen] = '\0';
        plens[level]         = namelen;
        if (eq == NULL) {
            continue;
        }

        cut = dotnet_provider(p, len, &prov, &provlen);
        if (cut == NULL || cut < eq) {
            fprintf(stderr, "doctor: stdin:%d: a value with no (Provider) after it\n",
                    lineno);
            cf_arena_free(ar);
            return 2;
        }

        /* The key: the sections above and this name, lowered, ':' a '.'. */
        for (j = 0; j <= level && ok; ++j) {
            size_t c;
            if (klen + plens[j] + 1 >= sizeof key) {
                ok = CF_FALSE;
                break;
            }
            if (j > 0) {
                key[klen++] = '.';
            }
            for (c = 0; c < plens[j]; ++c) {
                key[klen++] = (char)cf_to_lower((unsigned char)path[j][c]);
            }
        }
        key[klen] = '\0';
        if (!ok || !cf_key_valid(key, klen)) {
            if (skipped++ == 0) {
                cf_fmt(first_skipped, sizeof first_skipped, "%s", key);
            }
            continue;
        }

        /* The provider, as a source: one each, in the order first seen. */
        for (i = 0; i < nsources; ++i) {
            if (strlen(names[i]) == provlen && memcmp(names[i], prov, provlen) == 0) {
                source = (int)i;
                break;
            }
        }
        if (source < 0) {
            if (nsources >= DOTNET_SOURCES) {
                fprintf(stderr, "doctor: stdin:%d: more than %d providers\n", lineno,
                        DOTNET_SOURCES);
                cf_arena_free(ar);
                return 2;
            }
            names[nsources] = cf_arena_dup(ar, prov, provlen);
            if (names[nsources] == NULL) {
                fprintf(stderr, "doctor: out of memory\n");
                cf_arena_free(ar);
                return 3;
            }
            source = (int)nsources++;
        }
        cf_value_text(&v, eq + 1, (size_t)(cut - eq - 1));
        if (!cf_ks_add_text(&ks, key, klen, source * 10, v.u.t.p, v.u.t.n,
                            names[source])) {
            fprintf(stderr, "doctor: stdin:%d: cannot record it\n", lineno);
            cf_arena_free(ar);
            return 3;
        }
    }
    if (nsources == 0) {
        fprintf(stderr, "doctor: stdin holds no values; is it GetDebugView()?\n");
        cf_arena_free(ar);
        return 2;
    }
    if (!cf_ks_resolve(&ks)) {
        fprintf(stderr, "doctor: out of memory resolving\n");
        cf_arena_free(ar);
        return 3;
    }
    if (!full) {
        for (i = 0; i < cf_ks_count(&ks); ++i) {
            const cf_entry* e   = cf_ks_at(&ks, i);
            cf_error        err = CF_ERROR_INIT;
            const char*     key = cf_arena_dup(ar, e->key, e->keylen);
            if (key == NULL || !cf_def_text(&ds, key, NULL, CF_SENSITIVE, &err)) {
                fprintf(stderr, "doctor: %s\n", err.message);
                cf_arena_free(ar);
                return 3;
            }
        }
    }
    for (i = 0; i < nsources; ++i) {
        memset(&vts[i], 0, sizeof vts[i]);
        vts[i].kind = dotnet_kind(names[i]);
        memset(&src[i], 0, sizeof src[i]);
        src[i].vt   = &vts[i];
        src[i].name = names[i];
        src[i].rank = (int)i * 10;
        view[i]     = &src[i];
    }
    sv.items = view;
    sv.count = nsources;

    memset(&info, 0, sizeof info);
    info.mode        = full ? CF_REPORT_FULL : CF_REPORT_BLIND;
    info.profile     = profile;
    info.application = "dotnet";

    n   = cf_report_write(&ks, full ? NULL : &ds, &sv, &info, NULL, 0);
    out = (char*)cf_arena_alloc(ar, n + 1);
    if (out == NULL) {
        fprintf(stderr, "doctor: out of memory writing\n");
        cf_arena_free(ar);
        return 3;
    }
    cf_report_write(&ks, full ? NULL : &ds, &sv, &info, out, n + 1);
    fwrite(out, 1, n, stdout);
    if (skipped > 0) {
        fprintf(stderr,
                "doctor: %lu key(s) left out because a report key cannot carry "
                "them, e.g. %s\n",
                skipped, first_skipped);
    }
    cf_arena_free(ar);
    return 0;
}
