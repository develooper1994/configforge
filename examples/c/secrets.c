/*
 * secrets.c - a database password that never leaks.
 *
 * A secret in a configuration leaks in four ordinary ways: someone
 * commits it to a config file, a debugging printout shows it, a log of
 * "what changed" shows it, and a crash dump or a reused buffer still
 * holds it after the program is done with it. Each has a defence here,
 * and each is one line of the contract or one call:
 *
 *     a file may not supply it      a per-key policy: { "env", NULL }
 *     explain() never prints it     CF_SENSITIVE
 *     a change log never prints it  cf_diff_text, under the same flag
 *     a report never carries it     CF_REPORT_BLIND
 *     memory does not keep it       cf_arena_wipe on the program's buffer
 *
 *     ./secrets
 *
 * Every line of its output is checked by the suite.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#define FILE_NAME "secrets.ini"

/* The program's memory for its configuration: one buffer it owns, so it
   can wipe it. On a server this could be the heap through a custom
   allocator; a static buffer keeps the example short. */
static char g_arena[65536];

static int contains(const char* hay, size_t n, const char* needle) {
    size_t k = strlen(needle);
    size_t i;
    for (i = 0; i + k <= n; ++i) {
        if (memcmp(hay + i, needle, k) == 0) {
            return 1;
        }
    }
    return 0;
}

/* The recipe, called twice: at startup, and after the password is
   rotated in the environment. */
static cf_config* load(cf_arena* arena, char* const* environment, cf_error* err) {
    /* Only the environment - which is where a deployment's secret store
       puts it - may supply the password. The file is still read, so a
       password someone wrote there is seen, refused and reported, rather
       than silently winning or silently ignored. */
    static const char* const only_env[] = {"env", NULL};
    cf_builder*              b          = cf_build_in(arena);
    cf_def                   d;

    cf_builder_file(b, "file", CF_RANK_USER_FILE, FILE_NAME, NULL, 0);
    cf_builder_env(b, "env", CF_RANK_ENV, "SECRETS_", environment);

    cf_builder_text(b, "db.user", "app", 0);
    cf_def_init(&d, CF_TYPE_TEXT, "db.password");
    d.flags  = CF_SENSITIVE | CF_REQUIRED;
    d.policy = only_env;
    cf_builder_define(b, &d);
    return cf_builder_create(b, err);
}

int main(void) {
    static char    e_first[]  = "SECRETS_DB__PASSWORD=correct-horse";
    static char    e_second[] = "SECRETS_DB__PASSWORD=battery-staple";
    static char    e_user[]   = "SECRETS_DB__USER=reporting";
    static char*   first[]    = {e_first, e_user, NULL};
    static char*   second[]   = {e_second, e_user, NULL};
    static char    text[4096];
    cf_error       err = CF_ERROR_INIT;
    cf_arena*      arena;
    cf_config*     cfg;
    cf_config*     rotated;
    cf_report_info info;
    size_t         n;
    FILE*          f = fopen(FILE_NAME, "wb");

    /* The mistake that happens in every team eventually: a password
       committed to the file in the repository. */
    if (f == NULL) {
        return 1;
    }
    fputs("[db]\nuser = app\npassword = committed-by-accident\n", f);
    fclose(f);

    arena = cf_arena_fixed(g_arena, sizeof g_arena);
    cfg   = load(arena, first, &err);
    if (cfg == NULL) {
        fprintf(stderr, "secrets: %s\n", err.message);
        return 1;
    }

    /* The program uses the secret - connects to the database - and
       prints only that it has one. */
    printf("connecting as %s with a password of %u characters\n\n",
           cf_get_text(cfg, "db.user", "?"),
           (unsigned)strlen(cf_get_text(cfg, "db.password", "")));

    /* Why is the password what it is? The whole chain, every source and
       every location, and not one value: the question is where it came
       from, and that is not the secret. The file's attempt is there,
       refused by the policy. */
    cf_explain(cfg, "db.password", text, sizeof text);
    fputs(text, stdout);

    /* The password is rotated. A log of what changed says that it did,
       and not to what. */
    rotated = load(arena, second, &err);
    if (rotated == NULL) {
        fprintf(stderr, "secrets: %s\n", err.message);
        return 1;
    }
    cf_diff_text(cfg, rotated, text, sizeof text);
    printf("\nafter rotation:\n%s", text);

    /* A provenance report for Doctor or an audit. The blind form keeps
       presence, origin, length and a digest of the secret - enough to
       ask "is production's password set, and from where" - and not the
       secret. */
    memset(&info, 0, sizeof info);
    info.mode        = CF_REPORT_BLIND;
    info.application = "secrets";
    n                = cf_report(rotated, &info, text, sizeof text);
    printf("\nblind report: %u bytes, the password %s in it\n", (unsigned)n,
           (n < sizeof text && contains(text, n, "battery-staple")) ? "IS" : "is not");

    cf_config_free(cfg);
    cf_config_free(rotated);

    /* Freed is not gone: the bytes are still in the buffer. Wiping it is
       the program's decision, because the buffer is the program's. */
    printf("in memory after free: %s\n",
           contains(g_arena, sizeof g_arena, "battery-staple") ? "still there"
                                                               : "gone");
    cf_arena_wipe(arena);
    printf("in memory after wipe: %s\n",
           contains(g_arena, sizeof g_arena, "battery-staple") ? "still there"
                                                               : "gone");
    remove(FILE_NAME);
    return 0;
}
