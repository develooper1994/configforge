/*
 * git.c - Doctor's adapter for git config. C89.
 *
 *     git config --list --show-origin --show-scope | doctor import-git > git.json
 *
 * Doctor owns the report format and not anybody's resolution logic
 * (Doctor.md, section 12), and git publishes its own origins - which
 * makes this the cheapest adapter there is. Git says where each value
 * came from and in which scope; this writes that down in the shape
 * Doctor reads, through the library's own report writer, so the report
 * cannot drift from the schema everything else emits.
 *
 * What it decides, and why:
 *
 *   precedence    git's documented order, command > worktree > local >
 *                 global > system; within one scope the later line
 *                 wins, which is ConfigForge's rule as well.
 *   multi-values  remote.origin.fetch twice looks exactly like a key
 *                 set twice and overridden, and nothing in this output
 *                 tells them apart. They are recorded as git printed
 *                 them. There are no line numbers either, so CFD001 -
 *                 the one rule that would call the earlier one a
 *                 mistake - cannot fire on them.
 *   keys          a key a report cannot carry (an uppercase subsection,
 *                 a URL in one) is left out and counted, rather than
 *                 mangled into a spelling that collides with another.
 *   values        blind unless --full: git config holds credentials more
 *                 often than it looks, in http.extraheader and in URLs.
 *   valueless     "core.bare" with no "=" is git's spelling of true.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#include "adapters.h"

#define GIT_SCOPES 5

static const char* const git_scope_names[GIT_SCOPES] = {"command", "worktree", "local",
                                                        "global", "system"};

static const cf_source_vtable git_vt = {"git", NULL, NULL, NULL, NULL};

static int git_scope(const char* s, size_t n) {
    int i;
    for (i = 0; i < GIT_SCOPES; ++i) {
        if (strlen(git_scope_names[i]) == n && memcmp(git_scope_names[i], s, n) == 0) {
            return i;
        }
    }
    return -1;
}

/* "file:/home/u/.gitconfig" is the file; "command line:" is the command
   line; a quoted path loses its quotes. Anything else is kept as git
   wrote it, because it is still the best name for the place. */
static void git_origin(const char* s, size_t n, char* out, size_t cap) {
    if (n >= 5 && memcmp(s, "file:", 5) == 0) {
        s += 5;
        n -= 5;
    } else if (n > 0 && s[n - 1] == ':') {
        --n;
    }
    if (n >= 2 && s[0] == '"' && s[n - 1] == '"') {
        ++s;
        n -= 2;
    }
    if (n + 1 > cap) {
        n = cap - 1;
    }
    memcpy(out, s, n);
    out[n] = '\0';
}

static int git_usage(void) {
    fputs("doctor import-git [--full] [--profile <name>]\n"
          "\n"
          "Reads `git config --list --show-origin --show-scope` on stdin\n"
          "and writes a provenance report on stdout, value-blind unless\n"
          "--full: git config holds credentials more often than it\n"
          "looks.\n",
          stderr);
    return 2;
}

int doctor_import_git(int argc, char** argv) {
    static char      line[8192];
    static char      where[1024];
    cf_arena*        ar = cf_arena_new(NULL, 1 << 16);
    cf_keyspace      ks;
    cf_defset        ds;
    cf_source        src[GIT_SCOPES];
    const cf_source* view[GIT_SCOPES];
    cf_source_view   sv;
    cf_report_info   info;
    cf_bool          full    = CF_FALSE;
    const char*      profile = NULL;
    int              lineno  = 0;
    unsigned long    skipped = 0;
    char             first_skipped[CF_KEY_MAX];
    int              i;
    size_t           n;
    char*            out;
    cf_bool          seen[GIT_SCOPES];

    for (i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--full") == 0) {
            full = CF_TRUE;
        } else if (strcmp(argv[i], "--profile") == 0 && i + 1 < argc) {
            profile = argv[++i];
        } else {
            cf_arena_free(ar);
            return git_usage();
        }
    }
    if (ar == NULL || !cf_ks_init(&ks, ar, 256) || !cf_defset_init(&ds, ar, 256)) {
        fprintf(stderr, "doctor: cannot start\n");
        cf_arena_free(ar);
        return 3;
    }
    first_skipped[0] = '\0';
    memset(seen, 0, sizeof seen);

    while (fgets(line, (int)sizeof line, stdin) != NULL) {
        const char* p = line;
        const char* tab1;
        const char* tab2;
        const char* eq;
        size_t      len = strlen(line);
        int         scope;
        const char* value;
        size_t      klen;
        cf_value    v;

        ++lineno;
        if (len > 0 && line[len - 1] != '\n' && !feof(stdin)) {
            fprintf(stderr, "doctor: stdin:%d: longer than %u bytes\n", lineno,
                    (unsigned)sizeof line);
            cf_arena_free(ar);
            return 2;
        }
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0) {
            continue;
        }
        tab1 = strchr(p, '\t');
        tab2 = (tab1 != NULL) ? strchr(tab1 + 1, '\t') : NULL;
        if (tab2 == NULL) {
            /* Refused rather than guessed: without both fields this is
               not the output the adapter was written for, and a report
               built from a misread one would be confidently wrong. */
            fprintf(stderr,
                    "doctor: stdin:%d: expected scope, origin and key; run git "
                    "config --list --show-origin --show-scope\n",
                    lineno);
            cf_arena_free(ar);
            return 2;
        }
        scope = git_scope(p, (size_t)(tab1 - p));
        if (scope < 0) {
            fprintf(stderr, "doctor: stdin:%d: unknown scope '%.*s'\n", lineno,
                    (int)(tab1 - p), p);
            cf_arena_free(ar);
            return 2;
        }
        git_origin(tab1 + 1, (size_t)(tab2 - tab1 - 1), where, sizeof where);
        p  = tab2 + 1;
        eq = strchr(p, '=');
        if (eq != NULL) {
            klen  = (size_t)(eq - p);
            value = eq + 1;
        } else {
            klen  = strlen(p);
            value = "true";
        }
        if (!cf_key_valid(p, klen)) {
            if (skipped++ == 0) {
                size_t k =
                    (klen + 1 < sizeof first_skipped) ? klen : sizeof first_skipped - 1;
                memcpy(first_skipped, p, k);
                first_skipped[k] = '\0';
            }
            continue;
        }
        cf_value_text(&v, value, strlen(value));
        if (!cf_ks_add(&ks, p, klen, scope * 10, &v, where)) {
            fprintf(stderr, "doctor: stdin:%d: cannot record it\n", lineno);
            cf_arena_free(ar);
            return 3;
        }
        seen[scope] = CF_TRUE;
    }
    if (!cf_ks_resolve(&ks)) {
        fprintf(stderr, "doctor: out of memory resolving\n");
        cf_arena_free(ar);
        return 3;
    }

    /* Blind: every key a sensitive one, so the writer records presence,
       origin, length and digest - everything Doctor's rules need and
       nothing a credential would want. */
    if (!full) {
        size_t k;
        for (k = 0; k < cf_ks_count(&ks); ++k) {
            const cf_entry* e   = cf_ks_at(&ks, k);
            cf_error        err = CF_ERROR_INIT;
            /* In the arena, because a definition keeps its key. */
            const char* key = cf_arena_dup(ar, e->key, e->keylen);
            if (key == NULL || !cf_def_text(&ds, key, NULL, CF_SENSITIVE, &err)) {
                fprintf(stderr, "doctor: %s\n", err.message);
                cf_arena_free(ar);
                return 3;
            }
        }
    }

    sv.count = 0;
    for (i = 0; i < GIT_SCOPES; ++i) {
        if (!seen[i]) {
            continue;
        }
        memset(&src[i], 0, sizeof src[i]);
        src[i].vt        = &git_vt;
        src[i].name      = git_scope_names[i];
        src[i].rank      = i * 10;
        view[sv.count++] = &src[i];
    }
    sv.items = view;

    memset(&info, 0, sizeof info);
    info.mode        = full ? CF_REPORT_FULL : CF_REPORT_BLIND;
    info.profile     = profile;
    info.application = "git";

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
        /* Said, not hidden: a key left out is a key no rule looked at. */
        fprintf(stderr,
                "doctor: %lu key(s) left out because a report key cannot carry "
                "them, e.g. %s\n",
                skipped, first_skipped);
    }
    cf_arena_free(ar);
    return 0;
}
