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

/* ---------------------------------------------------------------- *
 * Every source and every feature, run out of room
 *
 * The run above reads two sources. Each of the others - a file, a
 * .env, a drop-in directory, a directory of secrets - takes its state
 * from the arena, and so does each feature that keeps something: a
 * reference's expansion, a renamed key's copies, a policy, an enum. A
 * buffer that ends inside any one of those is a place a device could
 * run out, so this run uses all of them, and the sweep below walks the
 * boundary through it eight bytes at a time.
 * ---------------------------------------------------------------- */

static cf_bool starve_spill(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        return CF_FALSE;
    }
    fputs(text, f);
    fclose(f);
    return CF_TRUE;
}

static char          s_e0[]   = "SHOP_SERVER__HOST=web";
static char*         s_env[]  = {s_e0, NULL};
static char          s_a1[]   = "--server.port=9000";
static char*         s_argv[] = {a0, s_a1, NULL};
static const cf_pair s_mem[]  = {{"plugins.audit.level", "2"}, {"cache.size", "lots"}};

static stage run_all_in(char* buf, size_t cap, cf_u64* digest, size_t* used,
                        cf_error* err) {
    static const char* const levels[]   = {"debug", "info", "warn", NULL};
    static const char* const from_env[] = {"secrets", "env", NULL};
    cf_arena*                ar;
    cf_builder*              b;
    cf_config*               c;
    cf_def                   d;
    static char              scratch[2048];

    *digest = 0;
    ar      = cf_arena_fixed(buf, cap);
    if (ar == NULL) {
        return STAGE_NO_ARENA;
    }
    b = cf_build_in(ar);
    if (b == NULL) {
        return STAGE_NO_BUILDER;
    }
    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, s_argv);
    cf_builder_env(b, "env", CF_RANK_ENV, "SHOP_", s_env);
    cf_builder_dotenv(b, ".env", 15, "cf_starve.env", "SHOP_", 0);
    cf_builder_file(b, "app.ini", CF_RANK_USER_FILE, "cf_starve.ini", NULL, 0);
#if CF_WITH_DIR
    cf_builder_dir(b, "conf.d", 25, "cf_starve.d", NULL, CF_OPTIONAL);
    cf_builder_secrets(b, "secrets", 5, "cf_starve_secrets", NULL, CF_OPTIONAL);
#endif
    cf_builder_memory(b, "mem", CF_RANK_SYS_FILE, s_mem, 2);

    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "localhost", 0);
    cf_builder_text(b, "server.url", NULL, 0);
    cf_def_init(&d, CF_TYPE_TEXT, "log.level");
    d.allowed     = levels;
    d.has_default = CF_TRUE;
    cf_value_text(&d.default_value, "info", 4);
    cf_builder_define(b, &d);
    cf_def_init(&d, CF_TYPE_INT, "cache.size");
    d.has_default = CF_TRUE;
    cf_value_int(&d.default_value, 32);
    d.on_invalid = CF_INVALID_FALLBACK;
    cf_builder_define(b, &d);
    cf_def_init(&d, CF_TYPE_TEXT, "db.password");
    d.flags  = CF_SENSITIVE;
    d.policy = from_env;
    cf_builder_define(b, &d);
    cf_builder_text(b, "app.name", "app", 0);
    cf_def_init(&d, CF_TYPE_TEXT, "old.name");
    d.flags       = CF_DEPRECATED;
    d.replaced_by = "app.name";
    cf_builder_define(b, &d);
    cf_builder_open(b, "plugins");
    cf_builder_unknown(b, CF_UNKNOWN_REJECT);

    c = cf_builder_create(b, err);
    if (c == NULL) {
        return STAGE_NO_CONFIG;
    }
    check(strcmp(cf_get_text(c, "server.url", ""), "http://web:9000/") == 0,
          "a reference expanded from the winners");
    check(strcmp(cf_get_text(c, "log.level", ""), "debug") == 0, "the .env read");
    check(cf_get_int(c, "cache.size", 0) == 32, "a refused value fell back");
    check(strcmp(cf_get_text(c, "app.name", ""), "legacy") == 0, "a renamed key fed");
#if CF_WITH_DIR
    check(strcmp(cf_get_text(c, "db.password", ""), "hunter2") == 0,
          "the secrets read");
#endif
    check(cf_get_int(c, "plugins.audit.level", 0) == 2, "an open prefix kept");
    (void)cf_explain(c, "server.url", scratch, sizeof scratch);
    (void)cf_inspect(c, scratch, sizeof scratch);
    *digest = cf_digest(c);
    *used   = cf_arena_used(ar);
    cf_config_free(c);
    return STAGE_COMPLETE;
}

/* A directory listing takes room for every name at its longest, so the
   buffer is larger than the first run's; the sweep starts just above
   what a complete run used, where every allocation lies. */
static void test_every_size_every_source(void) {
    static char buf[1 << 19];
    size_t      used = 0, ignored = 0;
    cf_u64      reference = 0, digest = 0;
    cf_error    err = CF_ERROR_INIT;
    size_t      cap;
    int         complete = 0, silent = 0;

    /* A build without the parts this run reads has nothing to sweep. */
    if (!cf_has_feature("ini") || !cf_has_feature("dotenv")) {
        return;
    }
    check(starve_spill("cf_starve.ini",
                       "[server]\nurl = http://${server.host}:${server.port}/\n"
                       "[old]\nname = legacy\n"),
          "wrote the file");
    check(starve_spill("cf_starve.env", "SHOP_LOG__LEVEL=debug\n"), "wrote the .env");
#if CF_WITH_DIR
    check(starve_spill("cf_starve.d/10-a.ini", "[server]\nhost = ignored\n"),
          "wrote the drop-in");
    check(starve_spill("cf_starve_secrets/db.password", "hunter2\n"),
          "wrote the secret");
#endif
    check(run_all_in(buf, sizeof buf, &reference, &used, &err) == STAGE_COMPLETE,
          "every source in the whole buffer");
    if (reference == 0) {
        printf("  %s\n", err.message);
        return;
    }
    for (cap = used + 4096; cap > 8; cap -= 8) {
        stage st;
        cf_error_clear(&err);
        st = run_all_in(buf, cap, &digest, &ignored, &err);
        if (st == STAGE_COMPLETE) {
            ++complete;
            if (digest != reference) {
                check(0, "every source: the result does not depend on the buffer");
                break;
            }
        } else if (st == STAGE_NO_CONFIG && err.message[0] == '\0') {
            ++silent;
        }
    }
    check(complete > 0, "every source: some sizes worked");
    check(silent == 0, "every source: every refusal came with a reason");
}

/* A change at run time in a buffer of its own (cf_derive_in), swept the
   same way: the derived configuration copies the original's sources,
   definitions, grants and candidates, and each copy can be the one
   that does not fit. */
static void test_derive_every_size(void) {
    static char    base_buf[1 << 16];
    static char    buf[1 << 16];
    static cf_pair change[] = {{"server.port", "9443"}, {"old.name", "renamed"}};
    cf_error       err      = CF_ERROR_INIT;
    cf_u64         unused;
    cf_arena*      ar = cf_arena_fixed(base_buf, sizeof base_buf);
    cf_builder*    b  = cf_build_in(ar);
    cf_config*     base;
    size_t         cap;
    int            complete = 0, silent = 0;
    cf_def         d;

    (void)unused;
    /* It reads the file the run above writes, so it needs what that needs. */
    if (!cf_has_feature("ini") || !cf_has_feature("dotenv")) {
        return;
    }
    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, s_argv);
    cf_builder_file(b, "app.ini", CF_RANK_USER_FILE, "cf_starve.ini", NULL, 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "localhost", 0);
    cf_builder_text(b, "server.url", NULL, 0);
    cf_builder_text(b, "app.name", "app", 0);
    cf_def_init(&d, CF_TYPE_TEXT, "old.name");
    d.flags       = CF_DEPRECATED;
    d.replaced_by = "app.name";
    cf_builder_define(b, &d);
    base = cf_builder_create(b, &err);
    check(base != NULL, "a base to derive from");
    if (base == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    for (cap = sizeof buf; cap > 8; cap -= 8) {
        cf_config* next;
        cf_error_clear(&err);
        next = cf_derive_in(base, cf_arena_fixed(buf, cap), "admin", change, 2, &err);
        if (next != NULL) {
            ++complete;
            check(cf_get_int(next, "server.port", 0) == 9443 &&
                      strcmp(cf_get_text(next, "app.name", ""), "renamed") == 0,
                  "a derived configuration that fits is whole");
            cf_config_free(next);
        } else if (err.message[0] == '\0') {
            ++silent;
        }
    }
    check(complete > 0, "derive: some sizes worked");
    check(silent == 0, "derive: every refusal came with a reason");
    cf_config_free(base);
}

int main(void) {
    test_every_size();
    test_degenerate();
    test_small_output_buffers();
    test_every_size_every_source();
    test_derive_every_size();

    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
