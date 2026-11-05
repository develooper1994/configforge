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

    /* INI writes an array of values as PHP's name[] lines (D-60), each
       element once and in order. */
    {
        cf_error e2 = CF_ERROR_INIT;
        check(cf_emit(c, "ini", CF_EMIT_ALL, 0, buf, sizeof buf, &e2) > 0,
              "INI writes an array of values");
        has(buf, "t[] = x\nt[] = y\n", "as name[] lines, in order");
    }
    cf_config_free(c);
}

/* What name[] lines cannot hold - an array of tables, of arrays, or an
   empty one - is refused before anything is written, rather than
   halfway through a file. */
static void test_ini_refuses_what_it_cannot_hold(void) {
    static char  t1[]     = "--s.0.host=a";
    static char  t2[]     = "--s.count=1";
    static char* tables[] = {a0, t1, t2, NULL};
    static char  n1[]     = "--m.0.0=1";
    static char  n2[]     = "--m.0.count=1";
    static char  n3[]     = "--m.count=1";
    static char* nested[] = {a0, n1, n2, n3, NULL};
    static char  e1[]     = "--e.count=0";
    static char* empty[]  = {a0, e1, NULL};
    char* const* cases[3];
    int          argc[3] = {3, 4, 2};
    int          i;
    static char  buf[4096];

    cases[0] = tables;
    cases[1] = nested;
    cases[2] = empty;
    for (i = 0; i < 3; ++i) {
        cf_error    err = CF_ERROR_INIT;
        cf_builder* b   = cf_build(NULL);
        cf_config*  c;
        cf_builder_cli(b, "cli", CF_RANK_CLI, argc[i], cases[i]);
        c = cf_builder_create(b, &err);
        if (c == NULL) {
            check(0, "builds");
            continue;
        }
        check(cf_emit(c, "ini", CF_EMIT_ALL, 0, buf, sizeof buf, &err) == 0 &&
                  err.code == CF_E_CAPABILITY && buf[0] == '\0',
              "INI refuses an array it has no form for, and writes nothing");
        has(err.message, "arrays of values only", "saying what it can write");
        cf_config_free(c);
    }
}

/* A table with a member called `count` is a table. Taken for an array,
   it was written as `[]` with its size gone - and INI, which has no
   arrays, refused to write it at all. */
static void test_count_is_a_key(void) {
    static char  a1[]   = "--pool.count=4";
    static char  a2[]   = "--pool.size=10";
    static char* argv[] = {a0, a1, a2, NULL};
    cf_error     err    = CF_ERROR_INIT;
    cf_builder*  b      = cf_build(NULL);
    cf_config*   c;
    static char  buf[1024];

    cf_builder_cli(b, "cli", CF_RANK_CLI, 3, argv);
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        check(0, "builds");
        return;
    }
    check(cf_emit(c, "ini", CF_EMIT_ALL, 0, buf, sizeof buf, &err) > 0,
          "INI writes a table that has a count");
    has(buf, "[pool]", "as a table");
    has(buf, "count = 4", "keeping the count");
    has(buf, "size = 10", "and the rest of it");

    check(cf_emit(c, "json", CF_EMIT_ALL, 0, buf, sizeof buf, &err) > 0,
          "JSON writes it too");
    has(buf, "\"count\": 4", "with the count a member");
    has(buf, "\"size\": 10", "beside the size");
    lacks(buf, "[]", "and not an empty array");
    cf_config_free(c);
}

/* INI is refused for an array that would be written, not for one the
   selection leaves out: the check asks what the encoder will be given. */
static void test_refusal_follows_the_selection(void) {
    /* an array of tables, which INI has no form for */
    static const cf_pair low[]  = {{"t.0.h", "x"}, {"t.1.h", "y"}, {"t.count", "2"}};
    static const cf_pair high[] = {{"server.port", "9000"}};
    cf_error             err    = CF_ERROR_INIT;
    cf_error             e2     = CF_ERROR_INIT;
    cf_builder*          b      = cf_build(NULL);
    cf_config*           c;
    static char          buf[1024];

    cf_builder_memory(b, "low", CF_RANK_USER_FILE, low, 3);
    cf_builder_memory(b, "high", CF_RANK_CLI, high, 1);
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        check(0, "builds");
        return;
    }
    check(cf_emit(c, "ini", CF_EMIT_FROM_RANK, CF_RANK_CLI, buf, sizeof buf, &err) > 0,
          "INI writes what one source set when no array is among it");
    has(buf, "9000", "which is the setting");
    lacks(buf, "[t]", "and not the array another source set");

    check(cf_emit(c, "ini", CF_EMIT_ALL, 0, buf, sizeof buf, &e2) == 0,
          "but refuses the whole configuration, which has one");
    check(e2.code == CF_E_CAPABILITY, "as a capability limit");
    check(buf[0] == '\0', "and writes nothing");
    cf_config_free(c);
}

#if 2 * (CF_MAX_DEPTH + 1) + 8 <= CF_KEY_MAX

#define DEEP_KEY (2 * (CF_MAX_DEPTH + 1) + 8)
#define DEEP_OUT (8192 + 4 * (CF_MAX_DEPTH + 2) * (CF_MAX_DEPTH + 2))

static cf_pair g_deep[CF_MAX_DEPTH + 3];
static char    g_deep_key[CF_MAX_DEPTH + 3][DEEP_KEY];
static char    g_deep_out[DEEP_OUT];

/* `levels` tables around one value, or `levels` arrays, each holding the
   next as its only member. A value of its own comes first, so that the
   writer has written something by the time it finds the document too
   deep. */
static cf_config* deep(int levels, cf_bool arrays, cf_error* err) {
    cf_builder* b = cf_build(NULL);
    size_t      n = 2;
    size_t      at;
    int         i, j;

    g_deep[0].key   = "lead";
    g_deep[0].value = "1";
    if (!arrays) {
        at = 0;
        for (i = 0; i < levels; ++i) {
            at = cf_fmt_append(g_deep_key[1], DEEP_KEY, at, "z.");
        }
        cf_fmt_append(g_deep_key[1], DEEP_KEY, at, "v");
        g_deep[1].key   = g_deep_key[1];
        g_deep[1].value = "1";
    } else {
        for (i = 0; i <= levels; ++i) {
            at = cf_fmt(g_deep_key[i + 1], DEEP_KEY, "z");
            for (j = 0; j < i; ++j) {
                at = cf_fmt_append(g_deep_key[i + 1], DEEP_KEY, at, ".0");
            }
            if (i < levels) {
                cf_fmt_append(g_deep_key[i + 1], DEEP_KEY, at, ".count");
            }
            g_deep[i + 1].key   = g_deep_key[i + 1];
            g_deep[i + 1].value = (i < levels) ? "1" : "x";
        }
        n = (size_t)levels + 2;
    }
    cf_builder_memory(b, "deep", CF_RANK_USER_FILE, g_deep, n);
    return cf_builder_create(b, err);
}

/* The document and CF_MAX_DEPTH - 1 levels inside it is the most that
   any format reads, so it is the most that any writes: all of it, or a
   configuration that deep cannot be saved, and no more, or a file is
   written that cannot be read back. One level past it is refused in the
   same words whichever format was asked for, and what had been written
   before the walk came to it is not left in the buffer. */
static void test_deepest_document(void) {
    static const char* const formats[] = {"json", "toml", "yaml", "ini"};
    static char              msg[160];
    size_t                   f;
    int                      arrays;

    for (f = 0; f < sizeof formats / sizeof formats[0]; ++f) {
        const cf_codec* codec = cf_codec_by_name(formats[f]);

        if (codec == NULL) {
            continue;
        }
        for (arrays = 0; arrays < 2; ++arrays) {
            const char* kind = arrays ? "arrays" : "tables";
            cf_error    err  = CF_ERROR_INIT;
            cf_error    e2   = CF_ERROR_INIT;
            cf_config*  c;
            size_t      n;

            if (arrays && (codec->caps & CF_CODEC_ARRAYS) == 0) {
                continue;
            }

            c = deep(CF_MAX_DEPTH - 1, (cf_bool)arrays, &err);
            cf_fmt(msg, sizeof msg, "%d levels of %s build", CF_MAX_DEPTH - 1, kind);
            check(c != NULL, msg);
            if (c != NULL) {
                n = cf_emit(c, formats[f], CF_EMIT_ALL, 0, g_deep_out,
                            sizeof g_deep_out, &err);
                cf_fmt(msg, sizeof msg, "%s writes %d levels of %s", formats[f],
                       CF_MAX_DEPTH - 1, kind);
                check(n > 0 && n < sizeof g_deep_out, msg);
                cf_config_free(c);
            }

            c = deep(CF_MAX_DEPTH, (cf_bool)arrays, &err);
            cf_fmt(msg, sizeof msg, "%d levels of %s build", CF_MAX_DEPTH, kind);
            check(c != NULL, msg);
            if (c == NULL) {
                continue;
            }
            g_deep_out[0] = 'x';
            n = cf_emit(c, formats[f], CF_EMIT_ALL, 0, g_deep_out, sizeof g_deep_out,
                        &err);
            cf_fmt(msg, sizeof msg, "%s refuses %d levels of %s", formats[f],
                   CF_MAX_DEPTH, kind);
            check(n == 0, msg);
            check(err.code == CF_E_KEY, "as a key that is out of bounds");
            has(err.message, "nests deeper", "and says the document is too deep");
            check(g_deep_out[0] == '\0', "leaving nothing in the buffer");
            check(cf_emit(c, formats[f], CF_EMIT_ALL, 0, NULL, 0, &e2) == 0,
                  "measuring it is refused as well");
            check(e2.code == CF_E_KEY, "for the same reason");
            cf_config_free(c);
        }
    }
}

#endif

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

static void test_value_container_refusal(void) {
    static const char*   formats[] = {"json", "toml", "yaml", "ini"};
    static const cf_pair low[]     = {{"a", "value"}, {"z", "unrelated"}};
    static const cf_pair high[]    = {{"a.child", "child"}};
    cf_error             err       = CF_ERROR_INIT;
    cf_builder*          b         = cf_build(NULL);
    cf_config*           c;
    char                 buf[512];
    size_t               i;

    cf_builder_memory(b, "low", CF_RANK_USER_FILE, low, 2);
    cf_builder_memory(b, "high", CF_RANK_CLI, high, 1);
    c = cf_builder_create(b, &err);
    check(c != NULL, "flat keys may share a value and a container prefix");
    if (c == NULL) {
        return;
    }
    for (i = 0; i < sizeof formats / sizeof formats[0]; ++i) {
        if (cf_codec_by_name(formats[i]) == NULL) {
            continue;
        }
        buf[0] = '!';
        check(cf_emit(c, formats[i], CF_EMIT_ALL, 0, buf, sizeof buf, &err) == 0,
              "no format writes conflicting names");
        check(err.code == CF_E_KEY, "the conflict is a key error");
        check(strcmp(err.key, "a") == 0, "the error names the full conflicting key");
        has(err.message, "both a value and a container",
            "the error explains the conflict");
        check(buf[0] == '\0', "a conflict leaves no partial document");
        check(cf_emit(c, formats[i], CF_EMIT_ALL, 0, NULL, 0, &err) == 0,
              "measurement refuses the same conflict");
        check(cf_emit(c, formats[i], CF_EMIT_FROM_RANK, CF_RANK_USER_FILE, buf,
                      sizeof buf, &err) > 0,
              "selecting only the value can be written");
        lacks(buf, "child", "a filtered-out container causes no refusal");
        check(cf_emit(c, formats[i], CF_EMIT_FROM_RANK, CF_RANK_CLI, buf, sizeof buf,
                      &err) > 0,
              "selecting only the container can be written");
        lacks(buf, "unrelated", "other values remain filtered");
    }
    cf_config_free(c);
}

static cf_config* read_emitted(const char* text, const cf_codec* codec, cf_error* err) {
    cf_arena*   ar = cf_arena_new(NULL, 4096);
    cf_keyspace ks;
    cf_flatten  fl;
    cf_blob     blob;
    cf_pair     pairs[16];
    static char converted[16][CF_VALUE_TEXT_MAX];
    cf_builder* b;
    cf_config*  c;
    size_t      i, count;

    if (ar == NULL) {
        check(0, "reader arena exists");
        return NULL;
    }
    cf_ks_init(&ks, ar, 16);
    cf_flatten_init(&fl, &ks, 0);
    cf_blob_mem(&blob, text, strlen(text), "emitted");
    if (!codec->decode(&blob, cf_flatten_sink(&fl), err)) {
        check(0, "emitted literals decode");
        cf_arena_free(ar);
        return NULL;
    }
    cf_ks_resolve(&ks);
    count = cf_ks_count(&ks);
    check(count <= sizeof pairs / sizeof pairs[0], "pairs fit");
    if (count > sizeof pairs / sizeof pairs[0]) {
        cf_arena_free(ar);
        return NULL;
    }
    for (i = 0; i < count; ++i) {
        const cf_entry* e = cf_ks_at(&ks, i);
        if (e->selected == NULL) {
            check(0, "emitted keys have values");
            cf_arena_free(ar);
            return NULL;
        }
        pairs[i].key = e->key;
        if (e->selected->value.type == CF_TYPE_TEXT) {
            pairs[i].value = e->selected->value.u.t.p;
        } else {
            check(cf_value_to_text(&e->selected->value, converted[i],
                                   sizeof converted[i]) != 0,
                  "an array count converts back to source text");
            pairs[i].value = converted[i];
        }
    }
    b = cf_build(NULL);
    cf_builder_memory(b, "emitted", CF_RANK_USER_FILE, pairs, count);
    c = cf_builder_create(b, err);
    cf_arena_free(ar);
    return c;
}

static void test_literal_references(cf_bool arrays) {
    static const cf_pair values[]   = {{"plain", "unchanged"},
                                       {"dollar", "$${missing}"},
                                       {"chain", "$$${missing}"},
                                       {"open", "$${"},
                                       {"nested.value", "a$${missing}b$${}c"},
                                       {"nested.last", "\"\\\t$${missing}"},
                                       {"tail", "end$$"}};
    static const cf_pair elements[] = {
        {"list.0", "$${missing}"}, {"list.1", "quote\"$${"}, {"list.count", "2"}};
    static const char* formats[] = {"json", "toml", "yaml", "ini"};
    static char        raw[4096], expected[8192], actual[8192];
    cf_error           err = CF_ERROR_INIT;
    cf_builder*        b   = cf_build(NULL);
    cf_config*         c;
    size_t             f;

    cf_builder_memory(b, "memory", CF_RANK_USER_FILE, values,
                      sizeof values / sizeof values[0]);
    if (arrays) {
        cf_builder_memory(b, "elements", CF_RANK_USER_FILE, elements,
                          sizeof elements / sizeof elements[0]);
    }
    cf_builder_text(b, "default", "${untouched}", 0);
    c = cf_builder_create(b, &err);
    check(c != NULL, "literal configuration builds");
    if (c == NULL) {
        return;
    }
    for (f = 0; f < sizeof formats / sizeof formats[0]; ++f) {
        const cf_codec* codec = cf_codec_by_name(formats[f]);
        cf_doc_sink     sink;
        cf_sink         out;
        cf_config*      reread;
        size_t          i, n = 0, cap;
        static union {
            cf_i64 align;
            char   b[CF_ENCODER_STATE_MAX];
        } state;

        if (codec == NULL || (arrays && (codec->caps & CF_CODEC_ARRAYS) == 0)) {
            continue;
        }
        cf_sink_init(&out, raw, sizeof raw);
        check(codec->encoder(&out, state.b, &sink), "raw encoder opens");
        check(cf_doc_emit(cf_keys(c), &sink, &err), "raw document emits");
        cf_sink_terminate(&out);
        has(raw, "${untouched}", "raw document keeps the literal untouched");
        lacks(raw, "$${untouched}", "only configuration emission escapes references");
        for (i = 0; raw[i] != '\0'; ++i) {
            if (raw[i] == '$' && raw[i + 1] == '{') {
                expected[n++] = '$';
            }
            expected[n++] = raw[i];
        }
        expected[n] = '\0';
        check(cf_emit(c, formats[f], CF_EMIT_ALL, 0, actual, sizeof actual, &err) == n,
              "configuration length counts each extra dollar");
        check(strcmp(actual, expected) == 0, "configuration escapes exactly each ${");
        reread = read_emitted(actual, codec, &err);
        check(reread != NULL, "emitted literals survive reference expansion");
        if (reread != NULL) {
            check(cf_digest(reread) == cf_digest(c),
                  "literal values have the same digest");
            check(strcmp(cf_get_text(reread, "default", ""), "${untouched}") == 0,
                  "a default is still literal after being written as configuration");
            check(strcmp(cf_get_text(reread, "chain", ""), "$${missing}") == 0,
                  "an already doubled dollar gains exactly one escape");
            cf_config_free(reread);
        }
        for (cap = 0; cap <= n + 2; ++cap) {
            size_t stored = cap == 0 ? 0 : (cap - 1 < n ? cap - 1 : n);
            memset(actual, '!', sizeof actual);
            check(cf_emit(c, formats[f], CF_EMIT_ALL, 0, actual, cap, &err) == n,
                  "every capacity measures the full escaped length");
            check(memcmp(actual, expected, stored) == 0,
                  "every truncated prefix is the full escaped output's prefix");
            check(cap == 0 || actual[stored] == '\0',
                  "each stored prefix is terminated");
            check(actual[cap] == '!', "nothing beyond capacity is written");
            check(cf_emit(c, formats[f], CF_EMIT_ALL, 0, NULL, cap, &err) == n,
                  "a null buffer measures the same length at every capacity");
        }
    }
    cf_config_free(c);
}

int main(void) {
    cf_arena* ar = cf_arena_new(NULL, 1 << 16);

    test_configured_only();
    test_round_trip(ar);
    test_arrays_are_all_or_nothing(ar);
    test_ini_refuses_what_it_cannot_hold();
    test_count_is_a_key();
    test_refusal_follows_the_selection();
#if 2 * (CF_MAX_DEPTH + 1) + 8 <= CF_KEY_MAX
    test_deepest_document();
#endif
    test_format_refusals();
    test_from_one_rank();
    test_transient_is_not_written();
    test_open_prefix();
    test_value_container_refusal();
    test_literal_references(CF_FALSE);
    test_literal_references(CF_TRUE);

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
