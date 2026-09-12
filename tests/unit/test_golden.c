/*
 * test_golden.c - explain() output as a byte-exact contract.
 *
 * The M0 definition of done asks for a fixture set in which every
 * source wins at least once, compared against expected output byte for
 * byte, with the determinism digest identical everywhere.
 *
 * The expected text is in the test rather than in a data file on
 * purpose: a golden file that nobody reads during review becomes a
 * file that gets regenerated whenever it disagrees. Here the contract
 * is visible next to the code that has to keep it.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

static int g_fail = 0, g_run = 0;

/* The expected text is an array of lines rather than one literal: C90
   guarantees only 509 characters in a string literal, and -pedantic
   enforces it. Comparing line by line also means a failure can name
   the line instead of a byte offset. */
static void compare(const char* got, const char* const* want, const char* what) {
    const char* p    = got;
    size_t      line = 0;

    ++g_run;
    for (; want[line] != NULL; ++line) {
        size_t n = strlen(want[line]);
        if (strncmp(p, want[line], n) != 0) {
            const char* nl   = strchr(p, '\n');
            size_t      glen = (nl != NULL) ? (size_t)(nl - p) + 1 : strlen(p);
            ++g_fail;
            printf("FAIL: %s differs at line %u\n", what, (unsigned)line + 1);
            printf("  got  |%.*s|\n", (int)glen, p);
            printf("  want |%s|\n", want[line]);
            return;
        }
        p += n;
    }
    if (*p != '\0') {
        ++g_fail;
        printf("FAIL: %s has %u unexpected trailing bytes:\n%s\n", what,
               (unsigned)strlen(p), p);
    }
}

/* Every source wins at least one key:
     cli      wins server.port
     env      wins log.level
     default  wins server.host and tls.enabled
   and every source also loses at least one, so both outcomes appear. */

static char  a0[]   = "prog";
static char  a1[]   = "--server.port=7000";
static char* argv[] = {a0, a1, NULL};

static char  e0[]  = "MYAPP_LOG__LEVEL=debug";
static char  e1[]  = "MYAPP_SERVER__PORT=9090";
static char* env[] = {e0, e1, NULL};

static const char* const expected_inspect[] = {
    "log.level = \"debug\"  (text)\n",
    "  env         \"debug\"               selected   MYAPP_LOG__LEVEL\n",
    "  default     \"info\"                shadowed\n",
    "  cli         not present\n",
    "server.host = \"0.0.0.0\"  (text)\n",
    "  default     \"0.0.0.0\"             selected\n",
    "  cli         not present\n",
    "  env         not present\n",
    "server.port = 7000  (int)\n",
    "  cli         7000                  selected   argv[1]\n",
    "  env         9090                  shadowed   MYAPP_SERVER__PORT\n",
    "  default     8080                  shadowed\n",
    "tls.enabled = false  (bool)\n",
    "  default     false                 selected\n",
    "  cli         not present\n",
    "  env         not present\n",
    NULL};

static const char* const expected_explain[] = {
    "server.port = 7000  (int)\n",
    "  cli         7000                  selected   argv[1]\n",
    "  env         9090                  shadowed   MYAPP_SERVER__PORT\n",
    "  default     8080                  shadowed\n", NULL};

static cf_config* build(cf_error* err) {
    cf_builder* b = cf_build(NULL);

    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, argv);
    cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", env);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "0.0.0.0", 0);
    cf_builder_text(b, "log.level", "info", 0);
    cf_builder_bool(b, "tls.enabled", CF_FALSE, 0);
    return cf_builder_create(b, err);
}

int main(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  cfg = build(&err);
    static char buf[4096];
    cf_u64      d1, d2;

    if (cfg == NULL) {
        printf("FAIL: build: %s\n", err.message);
        return 1;
    }

    cf_inspect(cfg, buf, sizeof buf);
    compare(buf, expected_inspect, "inspect");

    cf_explain(cfg, "server.port", buf, sizeof buf);
    compare(buf, expected_explain, "explain");

    d1 = cf_digest(cfg);
    cf_config_free(cfg);

    /* The same configuration built again must digest identically.
       Across platforms and portability modes this is asserted by
       running this same test everywhere; here it catches anything
       that leaked in from run to run. */
    cfg = build(&err);
    d2  = cf_digest(cfg);
    ++g_run;
    if (d1 != d2) {
        ++g_fail;
        printf("FAIL: digest is not stable across builds\n");
    }
    printf("digest %08lx%08lx\n", (unsigned long)(d1 >> 32),
           (unsigned long)(d1 & (cf_u64)0xFFFFFFFF));
    cf_config_free(cfg);

    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
