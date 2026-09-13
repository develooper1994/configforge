/*
 * test_starve.c - the fixed-capacity profile, run out of room.
 *
 * The whole freestanding claim rests on cf_arena_fixed: a target with
 * no heap hands over a buffer and the library works inside it. What
 * nothing tested is the interesting half - what happens when the buffer
 * is not big enough.
 *
 * There are exactly two acceptable answers at every size, and the test
 * is that there is never a third:
 *
 *   1. it works, and the configuration is correct
 *   2. it fails, says so, and leaves nothing usable behind
 *
 * The third answer - a configuration that was built from a truncated
 * key space and reads as if it were whole - is the one that would let a
 * device boot with half its settings and no indication. That is worse
 * than not booting.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

static int  g_fail = 0, g_run = 0;
static void check(int c, const char* w) {
    ++g_run;
    if (!c) {
        ++g_fail;
        printf("FAIL: %s\n", w);
    }
}

static char  a0[]     = "prog";
static char  a1[]     = "--server.port=9000";
static char  a2[]     = "--server.host=10.0.0.1";
static char  a3[]     = "--log.level=debug";
static char  a4[]     = "--tls.enabled=true";
static char* g_argv[] = {a0, a1, a2, a3, a4, NULL};

static char  e0[]    = "MYAPP_DB__PASSWORD=secret";
static char  e1[]    = "MYAPP_DB__HOST=db.internal";
static char* g_env[] = {e0, e1, NULL};

/* One complete run inside a buffer of the given size. Returns how far
   it got, so the caller can see the boundary move rather than only
   whether one size worked. */
typedef enum stage {
    STAGE_NO_ARENA = 0,
    STAGE_NO_BUILDER,
    STAGE_NO_CONFIG,
    STAGE_COMPLETE
} stage;

static stage run_in(char* buf, size_t cap, cf_u64* digest, cf_error* err) {
    cf_arena*   ar;
    cf_builder* b;
    cf_config*  c;
    char        scratch[1024];

    *digest = 0;
    ar      = cf_arena_fixed(buf, cap);
    if (ar == NULL) {
        return STAGE_NO_ARENA;
    }
    b = cf_build_in(ar);
    if (b == NULL) {
        return STAGE_NO_BUILDER;
    }
    cf_builder_cli(b, "cli", CF_RANK_CLI, 5, g_argv);
    cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", g_env);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "0.0.0.0", 0);
    cf_builder_text(b, "log.level", "info", 0);
    cf_builder_bool(b, "tls.enabled", CF_FALSE, 0);
    cf_builder_text(b, "db.password", NULL, CF_SENSITIVE);
    cf_builder_text(b, "db.host", "localhost", 0);

    c = cf_builder_create(b, err);
    if (c == NULL) {
        return STAGE_NO_CONFIG;
    }

    /* It claimed to succeed, so everything it promised has to hold. A
       configuration built from a truncated key space would show up
       here, not at construction. */
    check(cf_get_int(c, "server.port", 0) == 9000, "the value is right");
    check(strcmp(cf_get_text(c, "server.host", ""), "10.0.0.1") == 0,
          "and so is the text");
    check(strcmp(cf_get_text(c, "db.password", ""), "secret") == 0,
          "including what the environment supplied");
    check(strcmp(cf_get_text(c, "db.host", ""), "db.internal") == 0,
          "every source got read, not just the first");
    check(cf_get_bool(c, "tls.enabled", CF_FALSE), "and the boolean");
    check(cf_count(c) == 6, "six keys, none dropped");

    /* Diagnostics measure before they write, so they work in a buffer
       that is far too small rather than corrupting it. */
    (void)cf_explain(c, "server.port", scratch, sizeof scratch);
    (void)cf_inspect(c, scratch, sizeof scratch);

    *digest = cf_digest(c);
    cf_config_free(c);
    return STAGE_COMPLETE;
}

/* ---------------------------------------------------------------- */

static void test_every_size(void) {
    static char buf[1 << 16];
    cf_u64      reference = 0;
    cf_u64      digest    = 0;
    cf_error    err       = CF_ERROR_INIT;
    size_t      cap;
    size_t      smallest_that_worked       = 0;
    int         complete                   = 0;
    int         failures_without_a_message = 0;

    check(run_in(buf, sizeof buf, &reference, &err) == STAGE_COMPLETE,
          "the whole buffer is enough");
    check(reference != 0, "and produces a digest");

    /* Down to nothing, in steps small enough to land on the boundary
       from both sides. Every size is a whole run: sources read,
       definitions applied, values converted and validated. */
    for (cap = sizeof buf; cap > 0; cap -= (cap > 512) ? 64 : 8) {
        stage s;

        cf_error_clear(&err);
        s = run_in(buf, cap, &digest, &err);

        if (s == STAGE_COMPLETE) {
            ++complete;
            smallest_that_worked = cap;
            /* Succeeding in less room must not mean a different
               configuration. A digest that moved with the buffer size
               would mean something was silently dropped. */
            if (digest != reference) {
                check(0, "the configuration does not depend on the "
                         "size of the buffer it was built in");
                break;
            }
        } else if (s == STAGE_NO_CONFIG) {
            /* A refusal has to say something. "Out of room" that
               arrives as an empty message is indistinguishable from a
               bug at the call site. */
            if (err.message[0] == '\0') {
                ++failures_without_a_message;
            }
        }
        if (cap <= 8) {
            break;
        }
    }

    check(complete > 0, "some sizes worked");
    check(failures_without_a_message == 0, "every refusal came with a reason");
    printf("  smallest buffer that built a complete configuration: %u bytes\n",
           (unsigned)smallest_that_worked);
}

/* A buffer too small even to hold the arena header, and a NULL one:
   the two cases a caller gets wrong first. */
static void test_degenerate(void) {
    static char tiny[8];
    cf_u64      d   = 0;
    cf_error    err = CF_ERROR_INIT;

    check(run_in(tiny, sizeof tiny, &d, &err) != STAGE_COMPLETE,
          "eight bytes is not a configuration");
    check(cf_arena_fixed(NULL, 4096) == NULL, "no buffer, no arena");
    check(cf_arena_fixed(tiny, 0) == NULL, "no room, no arena");
}

/* The same discipline for the reading side: a diagnostic asked to write
   into a buffer that cannot hold it must report the length it needed
   and terminate what it did write. */
static void test_small_output_buffers(void) {
    static char big[1 << 16];
    char        out[16];
    cf_arena*   ar  = cf_arena_fixed(big, sizeof big);
    cf_builder* b   = cf_build_in(ar);
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c;
    size_t      full, part;

    cf_builder_cli(b, "cli", CF_RANK_CLI, 5, g_argv);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        check(0, "builds");
        return;
    }

    full = cf_explain(c, "server.port", NULL, 0);
    part = cf_explain(c, "server.port", out, sizeof out);
    check(full > sizeof out, "the explanation is longer than the buffer");
    check(part == full, "and the reported length is the whole one, not what fitted");
    check(out[sizeof out - 1] == '\0', "what was written is terminated");

    full = cf_inspect(c, NULL, 0);
    part = cf_inspect(c, out, sizeof out);
    check(part == full, "inspect reports the same way");

    full = cf_emit(c, "json", CF_EMIT_ALL, 0, NULL, 0, &err);
    part = cf_emit(c, "json", CF_EMIT_ALL, 0, out, sizeof out, &err);
    check(part == full, "and so does emit");

    cf_config_free(c);
}

int main(void) {
    test_every_size();
    test_degenerate();
    test_small_output_buffers();

    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
