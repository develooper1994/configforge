/*
 * test_oom_paths.c - the allocation sweep over every load and write path
 * of the configuration layer (K-9b, D-237).
 *
 * test_oom.c sweeps the builder run, the key space, the mapped write and
 * the list reload. This sweeps the rest: each way a document or a file or
 * an image becomes a configuration, a derived configuration, the edit, the
 * live reload and commit, the image. Each case is one function that
 * refuses the 1st, 2nd, ... nth allocation in turn (tests/support/cf_oom.h)
 * and holds three things:
 *
 *   - the failure is CF_E_MEMORY with a message;
 *   - nothing is left outstanding (the allocator counts, and the sanitize
 *     preset runs LeakSanitizer over the same programs);
 *   - for a `strong` function, the state is as it was.
 *
 * lib/config/tests/oom_paths.txt maps every public load or write name to
 * the case here, or says why it needs none; gate.oom reads it.
 */

#include <configforge/configforge.h>
#include <configforge/cf_keyspace.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cf_test.h"

#include "cf_oom.h"

#define DIR "cf_oom.d"

/* ---------------------------------------------------------------- *
 * A target an edit can write: a store source whose callback keeps the
 * last text in memory, and so is the same in every build.
 * ---------------------------------------------------------------- */

static char     g_target[32] = "old";
static unsigned g_target_writes;

static cf_bool target_load(const cf_source* s, cf_keyspace* ks) {
    return cf_ks_add_text(ks, "level", 5, s->rank, g_target, strlen(g_target),
                          "target");
}

static cf_bool target_write(const cf_source* s, const cf_write_item* items,
                            size_t count, const char* meta, cf_error* err) {
    (void)s;
    (void)meta;
    (void)err;
    ++g_target_writes;
    if (count == 1 && items[0].op == CF_WRITE_SET &&
        items[0].value.type == CF_TYPE_TEXT && items[0].value.u.t.n < sizeof g_target) {
        memcpy(g_target, items[0].value.u.t.p, items[0].value.u.t.n);
        g_target[items[0].value.u.t.n] = '\0';
    }
    return CF_TRUE;
}

static const cf_source_vtable target_vt = {
    sizeof(cf_source_vtable), "target-store", target_load, NULL, NULL, target_write};

static cf_builder* target_build(const cf_allocator* alloc) {
    cf_builder* b = cf_build(alloc);
    cf_source   s;

    if (b == NULL) {
        return NULL;
    }
    cf_builder_text(b, "level", "info", 0);
    memset(&s, 0, sizeof s);
    s.vt   = &target_vt;
    s.name = "store";
    s.rank = CF_RANK_USER_FILE;
    cf_builder_source(b, &s);
    cf_builder_writable(b, "store");
    return b;
}

static cf_u64 hash_text(const char* t) {
    /* 32-bit FNV-1a, kept under 2^32 so that no product wraps
       (ubsan-strict refuses an unsigned overflow even where C defines it). */
    cf_u64 h = 2166136261u;

    while (*t != '\0') {
        h = ((h ^ (unsigned char)*t++) * 16777619u) & 0xFFFFFFFFu;
    }
    return h;
}

/* ---------------------------------------------------------------- *
 * cf_edit_begin takes the configuration's allocator, not malloc's
 * ---------------------------------------------------------------- */

static void test_edit_uses_the_allocator(void) {
    cf_oom       o;
    cf_allocator a;
    cf_error     err = CF_ERROR_INIT;
    cf_config*   c;
    cf_edit*     e;
    size_t       built;

    cf_oom_init(&o, &a);
    c = cf_builder_create(target_build(&a), &err);
    check(c != NULL, "a configuration built with the caller's allocator");
    if (c == NULL) {
        return;
    }
    built = o.calls;
    e     = cf_edit_begin(c, "store", &err);
    check(e != NULL, "an edit begins");
    check(o.calls > built,
          "the edit's memory comes from the configuration's allocator, not malloc");
    cf_edit_abort(e);
    cf_config_free(c);
    check(o.live == 0, "and goes back to it");
}

/* ---------------------------------------------------------------- *
 * The edit: begin, set, commit
 * ---------------------------------------------------------------- */

/* The target holds what it held unless the commit worked. */
static int try_edit_commit(void* ctx, const cf_allocator* alloc, cf_u64* digest,
                           cf_error* err) {
    cf_config* c;
    cf_edit*   e;
    cf_bool    set;
    int        rc;

    (void)ctx;
    *digest = 0;
    cf_oom_pause(alloc);
    strcpy(g_target, "old");
    g_target_writes = 0;
    c               = cf_builder_create(target_build(alloc), err);
    if (c == NULL) {
        check(0, "the configuration the edit starts from is built");
        return CF_OOM_BAD;
    }
    cf_oom_resume(alloc);

    e = cf_edit_begin(c, "store", err);
    if (e == NULL) {
        rc = CF_OOM_FAILED;
    } else {
        set = cf_edit_set_text(e, "level", "new");
        /* A refused set is held, and commit reports it. */
        if (cf_edit_commit(e, "note", err)) {
            check(set, "a commit cannot work after a refused set");
            rc      = CF_OOM_DONE;
            *digest = hash_text(g_target);
        } else {
            rc = CF_OOM_FAILED;
        }
    }
    if (rc == CF_OOM_FAILED) {
        check(strcmp(g_target, "old") == 0 && g_target_writes == 0,
              "a failed commit leaves the target as it was");
    } else {
        check(strcmp(g_target, "new") == 0 && g_target_writes == 1,
              "a commit that worked wrote once");
    }
    cf_config_free(c);
    return rc;
}

static void test_edit_commit(void) {
    (void)cf_oom_sweep("an edit, set and commit", try_edit_commit, NULL);
}

/* ---------------------------------------------------------------- *
 * A configuration to start from, built with every allocation allowed
 * ---------------------------------------------------------------- */

static cf_config* base_config(const cf_allocator* a, cf_error* err) {
    static const cf_pair pairs[] = {
        {"server.port", "8080"}, {"server.host", "10.0.0.1"}, {"log.level", "info"}};
    cf_builder* b = cf_build(a);

    if (b == NULL) {
        return NULL;
    }
    cf_builder_memory(b, "mem", CF_RANK_SYS_FILE, pairs, 3);
    return cf_builder_create(b, err);
}

static int no_room(cf_error* err, const char* what) {
    cf_error_set(err, CF_E_MEMORY, NULL, NULL, "no room for %s", what);
    return CF_OOM_FAILED;
}

/* ---------------------------------------------------------------- *
 * Load: a document, a file, a directory, a .env file
 * ---------------------------------------------------------------- */

typedef struct load_case {
    const char* format;
    const char* text;
    const char* path; /* a file the case writes first, or NULL */
} load_case;

/* What every load below ends with: the configuration or the failure. */
static int finish_create(cf_builder* b, cf_u64* digest, cf_error* err) {
    cf_config* c = cf_builder_create(b, err);

    if (c == NULL) {
        return CF_OOM_FAILED;
    }
    check(cf_count(c) >= 1, "a configuration that was created holds its keys");
    *digest = cf_digest(c);
    cf_config_free(c);
    return CF_OOM_DONE;
}

static int try_builder_document(void* ctx, const cf_allocator* a, cf_u64* digest,
                                cf_error* err) {
    const load_case* lc = (const load_case*)ctx;
    cf_builder*      b  = cf_build(a);

    *digest = 0;
    if (b == NULL) {
        return no_room(err, "a builder");
    }
    (void)cf_builder_open(b, "open.");
    (void)cf_builder_document(b, "doc", CF_RANK_USER_FILE, lc->text, strlen(lc->text),
                              lc->format, 0);
    return finish_create(b, digest, err);
}

static void test_builder_document(void) {
    static load_case cases[] = {
        {"json", "{\"server\": {\"port\": 8080}, \"tags\": [\"a\", \"b\"]}", NULL},
        {"toml", "[server]\nport = 8080\ntags = [\"a\", \"b\"]\n", NULL},
        {"yaml", "server:\n  port: 8080\ntags:\n  - a\n  - b\n", NULL},
        {"ini", "[server]\nport = 8080\n", NULL},
        {"properties", "server.port=8080\nserver.host=h\n", NULL}};
    size_t i;

    for (i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        char what[64];

        if (cf_codec_by_name(cases[i].format) == NULL) {
            continue; /* this build left the format out */
        }
        cf_fmt(what, sizeof what, "a %s document", cases[i].format);
        (void)cf_oom_sweep(what, try_builder_document, (void*)&cases[i]);
    }
}

static int try_builder_file(void* ctx, const cf_allocator* a, cf_u64* digest,
                            cf_error* err) {
    const load_case* lc = (const load_case*)ctx;
    cf_builder*      b  = cf_build(a);

    *digest = 0;
    if (b == NULL) {
        return no_room(err, "a builder");
    }
    (void)cf_builder_file(b, "file", CF_RANK_USER_FILE, lc->path, NULL, 0);
    return finish_create(b, digest, err);
}

static void test_builder_file(void) {
    static load_case lc = {"json", "{\"server\": {\"port\": 8080}, \"name\": \"x\"}",
                           "cf_oom_paths.json"};

    if (cf_codec_by_name("json") == NULL) {
        return;
    }
    check(spill(lc.path, lc.text), "the file is written");
    (void)cf_oom_sweep("a file", try_builder_file, (void*)&lc);
}

#if CF_WITH_DIR
static int try_builder_dir(void* ctx, const cf_allocator* a, cf_u64* digest,
                           cf_error* err) {
    cf_builder* b = cf_build(a);

    (void)ctx;
    *digest = 0;
    if (b == NULL) {
        return no_room(err, "a builder");
    }
    (void)cf_builder_dir(b, "dir", CF_RANK_USER_FILE, DIR, "json", 0);
    return finish_create(b, digest, err);
}

static void test_builder_dir(void) {
    if (cf_codec_by_name("json") == NULL || !cf_platform_has_dir_list()) {
        return;
    }
    check(spill(DIR "/10-a.json", "{\"a\": 1}") &&
              spill(DIR "/20-b.json", "{\"b\": 2}"),
          "the drop-in files are written");
    (void)cf_oom_sweep("a drop-in directory", try_builder_dir, NULL);
}
#endif /* CF_WITH_DIR */

#if CF_WITH_DOTENV
static int try_builder_dotenv(void* ctx, const cf_allocator* a, cf_u64* digest,
                              cf_error* err) {
    cf_builder* b = cf_build(a);

    (void)ctx;
    *digest = 0;
    if (b == NULL) {
        return no_room(err, "a builder");
    }
    (void)cf_builder_dotenv(b, "env", CF_RANK_ENV, "cf_oom_paths.env", "APP_", 0);
    return finish_create(b, digest, err);
}

static void test_builder_dotenv(void) {
    check(spill("cf_oom_paths.env", "APP_SERVER__PORT=8080\nAPP_NAME=x\n"),
          "the .env file is written");
    (void)cf_oom_sweep("a .env file", try_builder_dotenv, NULL);
}
#endif /* CF_WITH_DOTENV */

/* ---------------------------------------------------------------- *
 * Derive: strong, the parent is as it was
 * ---------------------------------------------------------------- */

static int try_derive(void* ctx, const cf_allocator* a, cf_u64* digest, cf_error* err) {
    static const cf_pair change[] = {{"server.port", "9000"}};
    int                  in_arena = *(int*)ctx;
    cf_config*           base;
    cf_config*           next;
    cf_arena*            ar = NULL;
    cf_u64               before;
    int                  rc;

    *digest = 0;
    cf_oom_pause(a);
    base = base_config(a, err);
    if (base == NULL) {
        check(0, "the configuration to derive from is built");
        return CF_OOM_BAD;
    }
    before = cf_digest(base);
    if (in_arena) {
        ar = cf_arena_new(a, 0);
        if (ar == NULL) {
            cf_config_free(base);
            check(0, "the caller's arena is made");
            return CF_OOM_BAD;
        }
    }
    cf_oom_resume(a);

    next = in_arena ? cf_derive_in(base, ar, "runtime", change, 1, err)
                    : cf_derive(base, "runtime", change, 1, err);
    if (next == NULL) {
        rc = CF_OOM_FAILED;
    } else {
        check(cf_get_int(next, "server.port", 0) == 9000, "the derived value");
        *digest = cf_digest(next);
        cf_config_free(next);
        rc = CF_OOM_DONE;
    }
    check(cf_digest(base) == before, "the parent is as it was");
    check(cf_get_int(base, "server.port", 0) == 8080, "and still reads as it did");
    cf_arena_free(ar);
    cf_config_free(base);
    return rc;
}

static void test_derive(void) {
    int in_arena = 0;

    (void)cf_oom_sweep("cf_derive", try_derive, &in_arena);
    in_arena = 1;
    (void)cf_oom_sweep("cf_derive_in", try_derive, &in_arena);
}

/* ---------------------------------------------------------------- *
 * The image: written, opened, loaded
 * ---------------------------------------------------------------- */

#if CF_WITH_IMAGE
/* Aligned as a linker aligns a const array: the image is read in place
   (test_image.c's g_img). */
static union {
    cf_i64 align;
    char   b[8192];
} g_image;

static int try_image(void* ctx, const cf_allocator* a, cf_u64* digest, cf_error* err) {
    cf_config*  c;
    cf_image    img;
    cf_arena*   ar;
    cf_keyspace ks;
    size_t      n;
    int         rc = CF_OOM_DONE;

    (void)ctx;
    *digest = 0;
    cf_oom_pause(a);
    c = base_config(a, err);
    if (c == NULL) {
        check(0, "the configuration the image is made from is built");
        return CF_OOM_BAD;
    }
    n = cf_image_write(cf_keys(c), NULL, 0, g_image.b, sizeof g_image.b);
    cf_config_free(c);
    if (n == 0 || !cf_image_open(&img, g_image.b, n, err)) {
        check(0, "an image is written and opens");
        return CF_OOM_BAD;
    }
    ar = cf_arena_new(a, 0);
    if (ar == NULL) {
        check(0, "an arena for the image is made");
        return CF_OOM_BAD;
    }
    cf_oom_resume(a);

    if (!cf_ks_init(&ks, ar, 16)) {
        rc = no_room(err, "a key space");
    } else if (!cf_image_load(&img, &ks, CF_RANK_SYS_FILE, err)) {
        rc = CF_OOM_FAILED;
    } else if (!cf_ks_resolve(&ks)) {
        rc = no_room(err, "the resolved key space");
    } else {
        *digest = (cf_u64)cf_ks_count(&ks);
    }
    cf_arena_free(ar);
    return rc;
}

static void test_image(void) {
    (void)cf_oom_sweep("an image loaded", try_image, NULL);
}
#endif /* CF_WITH_IMAGE */

/* ---------------------------------------------------------------- *
 * The live reload's first build, and a commit through it
 * ---------------------------------------------------------------- */

static cf_allocator g_live_alloc;

/* The build function is the caller's, and "no builder" is all it can say
   to the live when cf_build was refused: the library cannot tell that from
   a caller that declined, so the case, which knows, gives the code. */
static int g_build_refused;

static cf_builder* live_build(void* user) {
    cf_builder* b = target_build((const cf_allocator*)user);

    if (b == NULL) {
        g_build_refused = 1;
    }
    return b;
}

static int refused_build(cf_error* err) {
    if (g_build_refused && err->code == CF_E_SOURCE) {
        cf_error_set(err, CF_E_MEMORY, NULL, NULL, "no room for a builder");
    }
    return CF_OOM_FAILED;
}

static int try_live_open(void* ctx, const cf_allocator* a, cf_u64* digest,
                         cf_error* err) {
    cf_live_options o;
    cf_live*        live;
    cf_config*      c;

    (void)ctx;
    *digest = 0;
    strcpy(g_target, "old");
    cf_live_options_init(&o);
    o.mode          = CF_LIVE_MANUAL;
    o.build         = live_build;
    g_live_alloc    = *a;
    o.build_user    = &g_live_alloc;
    o.allocator     = a;
    g_build_refused = 0;
    live            = cf_live_open_with(&o, err);
    if (live == NULL) {
        return refused_build(err);
    }
    c = cf_live_get(live);
    check(c != NULL && cf_live_generation(live) == 1, "a live that opened serves");
    *digest = cf_digest(c);
    cf_config_free(c);
    cf_live_close(live);
    return CF_OOM_DONE;
}

static void test_live_open(void) {
    (void)cf_oom_sweep("a live open", try_live_open, NULL);
}

static int try_live_commit(void* ctx, const cf_allocator* a, cf_u64* digest,
                           cf_error* err) {
    cf_live_options o;
    cf_live*        live;
    cf_config*      c;
    cf_edit*        e;
    cf_u64          before;
    cf_live_result  r;
    int             rc;

    (void)ctx;
    *digest = 0;
    cf_oom_pause(a);
    strcpy(g_target, "old");
    g_target_writes = 0;
    cf_live_options_init(&o);
    o.mode       = CF_LIVE_MANUAL;
    o.build      = live_build;
    g_live_alloc = *a;
    o.build_user = &g_live_alloc;
    o.allocator  = a;
    live         = cf_live_open_with(&o, err);
    if (live == NULL) {
        check(0, "the live the commit goes through opens");
        return CF_OOM_BAD;
    }
    c      = cf_live_get(live);
    before = cf_digest(c);
    e      = cf_edit_begin(c, "store", err);
    cf_config_free(c);
    if (e == NULL || !cf_edit_set_text(e, "level", "new")) {
        cf_edit_abort(e);
        cf_live_close(live);
        check(0, "an edit for the commit is made");
        return CF_OOM_BAD;
    }
    g_build_refused = 0;
    cf_oom_resume(a);

    r = cf_live_commit(live, e, "note", err);
    c = cf_live_get(live);
    if (r == CF_LIVE_FAILED) {
        check(cf_digest(c) == before && cf_live_generation(live) == 1,
              "a failed commit keeps serving the snapshot readers had");
        check(g_target_writes > 0 || strcmp(g_target, "old") == 0,
              "and one that wrote nothing leaves the target as it was");
        rc = refused_build(err);
    } else {
        check(r == CF_LIVE_SWAPPED && strcmp(cf_get_text(c, "level", ""), "new") == 0,
              "a commit that worked is served");
        *digest = cf_digest(c);
        rc      = CF_OOM_DONE;
    }
    cf_config_free(c);
    cf_live_close(live);
    return rc;
}

static void test_live_commit(void) {
    (void)cf_oom_sweep("a live commit", try_live_commit, NULL);
}

/* ---------------------------------------------------------------- *
 * Write: every format the build has
 * ---------------------------------------------------------------- */

#if CF_WITH_EMIT
static int try_emit(void* ctx, const cf_allocator* a, cf_u64* digest, cf_error* err) {
    const char* format = (const char*)ctx;
    static char buf[2048];
    cf_config*  c;
    size_t      n;
    cf_u64      before;
    int         rc;

    *digest = 0;
    cf_oom_pause(a);
    c = base_config(a, err);
    if (c == NULL) {
        check(0, "the configuration to emit is built");
        return CF_OOM_BAD;
    }
    before = cf_digest(c);
    cf_oom_resume(a);

    n = cf_emit(c, format, CF_EMIT_ALL, 0, buf, sizeof buf, err);
    if (n == 0) {
        check(buf[0] == '\0', "a refused emit leaves an empty buffer");
        rc = CF_OOM_FAILED;
    } else {
        *digest = hash_text(buf);
        rc      = CF_OOM_DONE;
    }
    check(cf_digest(c) == before, "the configuration is untouched");
    cf_config_free(c);
    return rc;
}

static void test_emit(void) {
    static char formats[][12] = {"json", "toml", "yaml", "ini", "cbor"};
    size_t      i;

    for (i = 0; i < sizeof formats / sizeof formats[0]; ++i) {
        char what[64];

        if (cf_codec_by_name(formats[i]) == NULL) {
            continue;
        }
        cf_fmt(what, sizeof what, "emit %s", formats[i]);
        (void)cf_oom_sweep(what, try_emit, formats[i]);
    }
}
#endif /* CF_WITH_EMIT */

/* ---------------------------------------------------------------- *
 * The watcher
 * ---------------------------------------------------------------- */

static int try_watcher_open(void* ctx, const cf_allocator* a, cf_u64* digest,
                            cf_error* err) {
    cf_builder* b;
    cf_config*  c;
    cf_watcher* w;
    int         rc;

    (void)ctx;
    *digest = 0;
    cf_oom_pause(a);
    b = cf_build(a);
    if (b == NULL) {
        check(0, "a builder for the watcher's configuration");
        return CF_OOM_BAD;
    }
    (void)cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_oom_paths.json", NULL, 0);
    c = cf_builder_create(b, err);
    if (c == NULL) {
        check(0, "the watcher's configuration is built");
        return CF_OOM_BAD;
    }
    cf_oom_resume(a);

    w = cf_watcher_open(c, a, err);
    if (w != NULL) {
        cf_watcher_close(w);
        rc = CF_OOM_DONE;
    } else if (err->code == CF_E_CAPABILITY) {
        rc = CF_OOM_DONE; /* a platform with no file notification */
    } else {
        rc = CF_OOM_FAILED;
    }
    cf_config_free(c);
    return rc;
}

static void test_watcher_open(void) {
    if (cf_codec_by_name("json") == NULL) {
        return;
    }
    check(spill("cf_oom_paths.json", "{\"server\": {\"port\": 8080}}"),
          "the watched file is written");
    (void)cf_oom_sweep("a watcher", try_watcher_open, NULL);
}

/* ---------------------------------------------------------------- *
 * K-9j: a builder given its source again after a refusal, and a
 * candidate added without its text
 * ---------------------------------------------------------------- */

/* cf_builder_create consumes the builder, refused or not, so "given its
   source again" is a new builder under the real allocator: it has to
   build what a clean run builds, whatever the refused run left behind. */
static int try_builder_again(void* ctx, const cf_allocator* a, cf_u64* digest,
                             cf_error* err) {
    const load_case* lc    = (const load_case*)ctx;
    cf_u64           clean = 0;
    cf_u64           again = 0;
    cf_error         e2    = CF_ERROR_INIT;
    int              rc;

    {
        cf_builder* b = cf_build(cf_default_allocator());
        check(b != NULL, "the clean builder exists");
        if (b == NULL) {
            return CF_OOM_BAD;
        }
        (void)cf_builder_open(b, "open.");
        (void)cf_builder_document(b, "doc", CF_RANK_USER_FILE, lc->text,
                                  strlen(lc->text), lc->format, 0);
        if (finish_create(b, &clean, &e2) != CF_OOM_DONE) {
            check(0, "the clean run builds");
            return CF_OOM_BAD;
        }
    }
    rc = try_builder_document(ctx, a, digest, err);
    if (rc == CF_OOM_FAILED) {
        cf_builder* b = cf_build(cf_default_allocator());

        check(b != NULL, "the builder after a refusal exists");
        if (b == NULL) {
            return CF_OOM_BAD;
        }
        (void)cf_builder_open(b, "open.");
        (void)cf_builder_document(b, "doc", CF_RANK_USER_FILE, lc->text,
                                  strlen(lc->text), lc->format, 0);
        if (finish_create(b, &again, &e2) != CF_OOM_DONE || again != clean) {
            check(0,
                  "after a refused run the same source builds what a clean run builds");
            return CF_OOM_BAD;
        }
    }
    return rc;
}

static void oom_builder_again(void) {
    static load_case cases[] = {
        {"json", "{\"server\": {\"port\": 8080}, \"tags\": [\"a\", \"b\"]}", NULL},
        {"toml", "[server]\nport = 8080\ntags = [\"a\", \"b\"]\n", NULL},
        {"yaml", "server:\n  port: 8080\ntags:\n  - a\n  - b\n", NULL}};
    size_t i;

    for (i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        char what[64];

        if (cf_codec_by_name(cases[i].format) == NULL) {
            continue;
        }
        cf_fmt(what, sizeof what, "a %s document, then again", cases[i].format);
        (void)cf_oom_sweep(what, try_builder_again, (void*)&cases[i]);
    }
}

/* The key space is made first (paused); the candidate's three texts are
   each longer than the arena's first block, so each takes an allocation. */
static int try_add_candidate(void* ctx, const cf_allocator* a, cf_u64* digest,
                             cf_error* err) {
    static char     big[1300];
    cf_arena*       ar;
    cf_keyspace     ks;
    cf_candidate    c;
    cf_value        v;
    const cf_entry* e;
    size_t          before;
    size_t          counted;
    int             rc = CF_OOM_DONE;
    int             ok;

    (void)ctx;
    *digest = 0;
    memset(big, 'x', sizeof big - 1);
    big[sizeof big - 1] = '\0';

    cf_oom_pause(a);
    ar = cf_arena_new(a, 1024);
    if (ar == NULL || !cf_ks_init(&ks, ar, 1)) {
        cf_oom_resume(a);
        if (ar != NULL) {
            cf_arena_free(ar);
        }
        check(0, "the key space is made before the part under test");
        return CF_OOM_BAD;
    }
    cf_value_int(&v, 1);
    check(cf_ks_add(&ks, "z.first", 7, 0, &v, NULL), "a first candidate");
    before = cf_ks_candidate_count(&ks);

    memset(&c, 0, sizeof c);
    cf_value_text(&c.value, big, sizeof big - 1);
    c.source  = 1;
    c.at      = big;
    c.written = big;
    cf_oom_resume(a);

    ok      = cf_ks_add_candidate(&ks, "a.b", 3, &c);
    counted = cf_oom_calls(a);
    cf_oom_pause(a);
    if (!ok) {
        cf_error_set(err, CF_E_MEMORY, NULL, NULL, "no room for a candidate");
        rc = CF_OOM_FAILED;
    }
    check(cf_ks_resolve(&ks), "the key space resolves");
    e = cf_ks_find(&ks, "a.b", 3);
    if (e == NULL) {
        check(ok == 0, "a candidate that was added is found");
        check(cf_ks_candidate_count(&ks) == before,
              "a refused candidate left the key space as it was");
    } else {
        check(e->chain != NULL && e->chain->written != NULL,
              "a candidate that is in the key space has its written text");
        if (e->chain == NULL || e->chain->written == NULL) {
            rc = CF_OOM_BAD;
        }
        *digest = 1;
    }
    cf_arena_free(ar);
    /* The checks above allocated (resolve, the lookup): not the sweep's places. */
    ((cf_oom*)a->ctx)->calls = counted;
    return rc;
}

static void oom_add_candidate(void) {
    (void)cf_oom_sweep("a candidate added", try_add_candidate, NULL);
}

int main(void) {
    test_edit_uses_the_allocator();
    test_edit_commit();
    test_builder_document();
    oom_builder_again();
    oom_add_candidate();
    test_builder_file();
#if CF_WITH_DIR
    test_builder_dir();
#endif
#if CF_WITH_DOTENV
    test_builder_dotenv();
#endif
    test_derive();
#if CF_WITH_IMAGE
    test_image();
#endif
    test_live_open();
    test_live_commit();
#if CF_WITH_EMIT
    test_emit();
#endif
    test_watcher_open();

    return cf_test_done();
}
