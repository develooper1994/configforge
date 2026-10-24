/*
 * test_emit.c - writing a configuration out.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

/* A format the library knows and this build left out, for the checks of
   what that does - XML unless it is in, then YAML; none when every one
   is in, and then those checks have nothing to say. */
#if !CF_WITH_XML
#define ABSENT "xml"
#define ABSENT_OPTION "CF_WITH_XML"
#elif !CF_WITH_YAML
#define ABSENT "yaml"
#define ABSENT_OPTION "CF_WITH_YAML"
#endif

#if CF_WITH_EMIT

static int  g_fail = 0, g_run = 0;
static void check(int c, const char* w) {
    ++g_run;
    if (!c) {
        ++g_fail;
        printf("FAIL: %s\n", w);
    }
}
static void has(const char* hay, const char* needle, const char* w) {
    ++g_run;
    if (hay == NULL || strstr(hay, needle) == NULL) {
        ++g_fail;
        printf("FAIL: %s  missing \"%s\" in:\n%s\n", w, needle, hay ? hay : "");
    }
}
static void lacks(const char* hay, const char* needle, const char* w) {
    ++g_run;
    if (hay != NULL && strstr(hay, needle) != NULL) {
        ++g_fail;
        printf("FAIL: %s  found \"%s\" in:\n%s\n", w, needle, hay);
    }
}

static char a0[] = "prog";

static cf_config* build(cf_error* err) {
    static char  a1[]   = "--server.port=9000";
    static char* argv[] = {a0, a1, NULL};
    cf_builder*  b      = cf_build(NULL);

    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, argv);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "0.0.0.0", 0);
    cf_builder_text(b, "log.level", "info", 0);
    return cf_builder_create(b, err);
}

static void test_configured_only(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c   = build(&err);
    static char buf[4096];
    size_t      n;

    check(c != NULL, "builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }

    n = cf_emit(c, "json", CF_EMIT_CONFIGURED, 0, buf, sizeof buf, &err);
    check(n > 0, "configured-only emits");
    if (n == 0) {
        printf("  %s\n", err.message);
    }

    /* An overrides file should hold what someone configured and nothing
       else, so a later release can move a default the user never
       touched. */
    has(buf, "9000", "the value that was set");
    lacks(buf, "log", "a key nobody configured stays out");
    lacks(buf, "0.0.0.0", "and so does an untouched default");

    /* Measure-or-write, like everything else that renders. */
    check(cf_emit(c, "json", CF_EMIT_CONFIGURED, 0, NULL, 0, &err) == n,
          "it can be measured first");

    n = cf_emit(c, "json", CF_EMIT_ALL, 0, buf, sizeof buf, &err);
    has(buf, "log", "a full snapshot has everything");
    has(buf, "0.0.0.0", "defaults included");

    cf_config_free(c);
}

static void test_round_trip(cf_arena* ar) {
    cf_error        err = CF_ERROR_INIT;
    cf_config*      c   = build(&err);
    static char     buf[4096];
    const cf_codec* json = cf_codec_by_name("json");
    cf_keyspace     ks;
    cf_flatten      fl;
    cf_blob         blob;

    if (c == NULL) {
        check(0, "builds");
        return;
    }
    cf_emit(c, "json", CF_EMIT_ALL, 0, buf, sizeof buf, &err);
    cf_blob_mem(&blob, buf, strlen(buf), "emitted");
    cf_ks_init(&ks, ar, 32);
    cf_flatten_init(&fl, &ks, 0);
    check(json->decode(&blob, cf_flatten_sink(&fl), &err), "it reads back");
    cf_ks_resolve(&ks);

    /* A full snapshot written and read is the same configuration. */
    check(cf_ks_digest(&ks) == cf_digest(c),
          "a full emit round trips to the same digest");
    cf_config_free(c);
}

static void test_arrays_are_all_or_nothing(cf_arena* ar) {
    static char  a1[]   = "--t.0=x";
    static char  a2[]   = "--t.1=y";
    static char  a3[]   = "--t.count=2";
    static char* argv[] = {a0, a1, a2, a3, NULL};
    cf_error     err    = CF_ERROR_INIT;
    cf_builder*  b      = cf_build(NULL);
    cf_config*   c;
    static char  buf[4096];

    (void)ar;
    cf_builder_cli(b, "cli", CF_RANK_CLI, 4, argv);
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        check(0, "builds");
        return;
    }
    check(cf_emit(c, "json", CF_EMIT_CONFIGURED, 0, buf, sizeof buf, &err) > 0,
          "an array emits");
    has(buf, "\"x\"", "first element");
    has(buf, "\"y\"", "and the second, because an array is all or nothing");

    /* INI has no array syntax, and inventing one would make every file
       this library writes unreadable by every other INI reader. The
       refusal happens before anything is written rather than halfway
       through a file. */
    {
        cf_error e2 = CF_ERROR_INIT;
        check(cf_emit(c, "ini", CF_EMIT_ALL, 0, buf, sizeof buf, &e2) == 0,
              "INI refuses an array");
        check(e2.code == CF_E_CAPABILITY, "as a capability limit");
        has(e2.message, "no syntax for one", "saying what is missing");
        check(buf[0] == '\0', "and writes nothing at all");
    }
    cf_config_free(c);
}

static void test_format_refusals(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c   = build(&err);
    static char buf[1024];

    if (c == NULL) {
        check(0, "builds");
        return;
    }
#ifdef ABSENT
    check(cf_emit(c, ABSENT, CF_EMIT_ALL, 0, buf, sizeof buf, &err) == 0,
          "a format this build lacks is refused");
    has(err.message, ABSENT_OPTION, "naming the option to turn on");
#endif
#if CF_WITH_XML
    /* XML is read only (D-23): writing it is refused, and says so. */
    check(cf_emit(c, "xml", CF_EMIT_ALL, 0, buf, sizeof buf, &err) == 0,
          "XML is not written");
    has(err.message, "can be read but not written", "and the reason is given");
#endif

    check(cf_emit(c, "nonesuch", CF_EMIT_ALL, 0, buf, sizeof buf, &err) == 0,
          "an unknown format is refused");
    has(err.message, "no format called", "and says so");
    cf_config_free(c);
}

static void test_from_one_rank(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c   = build(&err);
    static char buf[4096];

    if (c == NULL) {
        check(0, "builds");
        return;
    }
    cf_emit(c, "ini", CF_EMIT_FROM_RANK, CF_RANK_CLI, buf, sizeof buf, &err);
    has(buf, "9000", "what the command line supplied");
    lacks(buf, "info", "and nothing another source did");
    cf_config_free(c);
}

/* A command is never written out. --cf.explain asks a question; a file
   that recorded it would ask the question again on every run that reads
   the file. */
static void test_transient_is_not_written(void) {
    static char  a1[]   = "--server.port=9000";
    static char  a2[]   = "--cf.explain=server.port";
    static char* argv[] = {a0, a1, a2, NULL};
    cf_error     err    = CF_ERROR_INIT;
    cf_builder*  b      = cf_build(NULL);
    cf_config*   c;
    static char  buf[2048];

    cf_builder_cli(b, "cli", CF_RANK_CLI, 3, argv);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "cf.explain", NULL, CF_TRANSIENT);
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        check(0, "builds");
        printf("  %s\n", err.message);
        return;
    }
    check(cf_has(c, "cf.explain"), "the command is readable");
    cf_emit(c, "json", CF_EMIT_CONFIGURED, 0, buf, sizeof buf, &err);
    has(buf, "9000", "the setting is written");
    lacks(buf, "explain", "the command is not");
    cf_config_free(c);
}

/* A key space the program cannot know in advance. Without this, Tier 1
   and a dynamic key space are mutually exclusive, which the first
   example that tried to iterate plugins discovered immediately. */
static void test_open_prefix(void) {
    static char  a1[]   = "--plugins.auth.enabled=true";
    static char  a2[]   = "--server.port=9000";
    static char* argv[] = {a0, a1, a2, NULL};
    cf_error     err    = CF_ERROR_INIT;
    cf_builder*  b      = cf_build(NULL);
    cf_config*   c;

    cf_builder_cli(b, "cli", CF_RANK_CLI, 3, argv);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_unknown(b, CF_UNKNOWN_REJECT);
    cf_builder_open(b, "plugins");
    c = cf_builder_create(b, &err);
    check(c != NULL, "an open prefix survives strict rejection");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_get_bool(c, "plugins.auth.enabled", CF_FALSE), "and is readable");
    cf_config_free(c);

    /* Everything outside the open prefix is still checked, which is the
       point: the exemption is a hole of a stated size. */
    {
        static char  b1[]    = "--serverr.port=1";
        static char* bargv[] = {a0, b1, NULL};
        cf_error     e2      = CF_ERROR_INIT;
        b                    = cf_build(NULL);
        cf_builder_cli(b, "cli", CF_RANK_CLI, 2, bargv);
        cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
        cf_builder_unknown(b, CF_UNKNOWN_REJECT);
        cf_builder_open(b, "plugins");
        check(cf_builder_create(b, &e2) == NULL, "a typo is still caught");
        has(e2.message, "did you mean", "with the suggestion intact");
    }

    /* At a segment boundary only: opening "plugins" must not open
       "pluginsomething". */
    {
        static char  c1[]    = "--pluginsomething=1";
        static char* cargv[] = {a0, c1, NULL};
        cf_error     e3      = CF_ERROR_INIT;
        b                    = cf_build(NULL);
        cf_builder_cli(b, "cli", CF_RANK_CLI, 2, cargv);
        cf_builder_unknown(b, CF_UNKNOWN_REJECT);
        cf_builder_open(b, "plugins");
        check(cf_builder_create(b, &e3) == NULL,
              "an open prefix does not open a longer name");
    }
}

int main(void) {
    cf_arena* ar = cf_arena_new(NULL, 1 << 16);

    test_configured_only();
    test_round_trip(ar);
    test_arrays_are_all_or_nothing(ar);
    test_format_refusals();
    test_from_one_rank();
    test_transient_is_not_written();
    test_open_prefix();

    cf_arena_free(ar);
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}

#else /* !CF_WITH_EMIT */

int main(void) {
    printf("skipped: this build has no emitter (CF_WITH_EMIT=OFF)\n");
    return 0;
}

#endif /* CF_WITH_EMIT */
