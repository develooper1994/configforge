/*
 * test_oom.c - fail the Nth allocation, for every N.
 *
 * The starvation test reaches whatever failure paths happen to sit at
 * the boundary of a buffer size. This reaches all of them: an allocator
 * that succeeds N times and then refuses forever, swept from zero
 * upward until nothing fails any more. Every allocation the library
 * makes is a point where a caller's memory ran out, and this walks each
 * one in turn.
 *
 * The contract is the same as everywhere else that takes input it does
 * not control:
 *
 *   1. it works, and the result is correct
 *   2. it fails, says why, and leaves nothing usable behind
 *
 * Never a crash, never a leak, never a configuration that was built
 * from a partial one and reads as if it were whole.
 *
 * This is the test the cf_allocator interface exists for. A library
 * that called malloc directly could not be swept like this without a
 * linker trick, which is one of the reasons everything here goes
 * through the interface - and why gate.symbols checks that it does.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cf_test.h"

#include "cf_oom.h" /* the allocator and the sweep, shared (K-9b) */

/* ---------------------------------------------------------------- *
 * The run under test
 * ---------------------------------------------------------------- */

static char  a0[]     = "prog";
static char  a1[]     = "--server.port=9000";
static char  a2[]     = "--server.host=10.0.0.1";
static char  a3[]     = "--tls.enabled=true";
static char* g_argv[] = {a0, a1, a2, a3, NULL};

static char  e0[]    = "MYAPP_DB__PASSWORD=secret";
static char  e1[]    = "MYAPP_LOG__LEVEL=debug";
static char* g_env[] = {e0, e1, NULL};

/* `extra` is how many additional keys a memory source supplies.
   A small configuration takes five blocks from the allocator, which is
   five injection points; a few hundred keys take dozens, and every one
   is a place the machine could have filled up.

   Driving it with the configuration's size rather than the arena's
   block size is deliberate. The arena floors a block at a kilobyte for
   a good reason - below that each block spends most of itself on its
   own header - and lowering a floor so a test can reach further is
   bending the library to suit the test. */
static int try_build(void* ctx, const cf_allocator* alloc, cf_u64* digest,
                     cf_error* err) {
    size_t      extra = *(size_t*)ctx;
    cf_builder* b;
    cf_config*  c;
    static char scratch[2048];

    *digest = 0;
    b       = cf_build(alloc);
    if (b == NULL) {
        /* No builder to hold a reason: the refusal is the NULL itself. */
        cf_error_set(err, CF_E_MEMORY, NULL, NULL, "no room for a builder");
        return CF_OOM_FAILED;
    }
    cf_builder_cli(b, "cli", CF_RANK_CLI, 4, g_argv);
    cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", g_env);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "0.0.0.0", 0);
    cf_builder_text(b, "log.level", "info", 0);
    cf_builder_bool(b, "tls.enabled", CF_FALSE, 0);
    cf_builder_text(b, "db.password", NULL, CF_SENSITIVE);
    if (extra > 0) {
        /* Static, because the pairs have to outlive the builder and
           because a few hundred of them is more stack than the budget
           allows. */
        static cf_pair pairs[512];
        static char    keys[512][16];
        size_t         i;
        if (extra > 512) {
            extra = 512;
        }
        for (i = 0; i < extra; ++i) {
            cf_fmt(keys[i], sizeof keys[i], "bulk.k%u", (unsigned)i);
            pairs[i].key   = keys[i];
            pairs[i].value = "1";
        }
        cf_builder_memory(b, "bulk", CF_RANK_SYS_FILE, pairs, extra);
    }

    c = cf_builder_create(b, err);
    if (c == NULL) {
        return CF_OOM_FAILED;
    }

    /* It said it worked, so everything it promised holds. A snapshot
       assembled from a key space that lost half its candidates would
       show up here rather than at construction. */
    check(cf_count(c) == 5 + extra, "every key survived, none dropped");
    check(cf_get_int(c, "server.port", 0) == 9000, "the command line");
    check(strcmp(cf_get_text(c, "server.host", ""), "10.0.0.1") == 0, "and its text");
    check(strcmp(cf_get_text(c, "db.password", ""), "secret") == 0, "the environment");
    check(strcmp(cf_get_text(c, "log.level", ""), "debug") == 0,
          "all of the environment, not the first variable of it");
    check(cf_get_bool(c, "tls.enabled", CF_FALSE), "the boolean");

    /* Reading is supposed to be infallible once construction returned,
       including the parts that render. */
    (void)cf_explain(c, "server.port", scratch, sizeof scratch);
    (void)cf_inspect(c, scratch, sizeof scratch);
    (void)cf_emit(c, "json", CF_EMIT_ALL, 0, scratch, sizeof scratch, err);

    *digest = cf_digest(c);
    cf_config_free(c);
    return CF_OOM_DONE;
}

/* ---------------------------------------------------------------- */

/* A bigger configuration must take strictly more allocations. Two
   sweeps that agree exactly are two sweeps of the same thing, and a
   test that cannot tell whether it tested anything is worse than no
   test - which this caught: an earlier version passed block sizes to a
   function that had started taking key counts, and both were clamped to
   the same number. Counting distinct values was too weak to see it,
   because the first sweep differed and the other two did not. */
static size_t g_last_count  = 0;
static int    g_not_growing = 0;

static void note_count(size_t n) {
    if (g_last_count != 0 && n <= g_last_count) {
        ++g_not_growing;
    }
    g_last_count = n;
}

static void sweep(size_t extra, const char* what) {
    /* Every allocation the library makes, refused in turn. */
    note_count(cf_oom_sweep(what, try_build, &extra));
}

/* The same sweep for the reading side, where a caller supplies the
   allocator for something built after construction. An arena that exists
   may still refuse everything after its first block, which is the path a
   source takes when the machine fills up while it is reading. */
static int try_keyspace(void* ctx, const cf_allocator* alloc, cf_u64* digest,
                        cf_error* err) {
    cf_arena*   ar;
    cf_keyspace ks;
    cf_value    v;
    size_t      i;
    int         rc = CF_OOM_DONE;

    (void)ctx;
    *digest = 0;
    ar      = cf_arena_new(alloc, 0);
    if (ar == NULL) {
        cf_error_set(err, CF_E_MEMORY, NULL, NULL, "no room for an arena");
        return CF_OOM_FAILED;
    }
    if (!cf_ks_init(&ks, ar, 64)) {
        cf_error_set(err, CF_E_MEMORY, NULL, NULL, "no room for a key space");
        cf_arena_free(ar);
        return CF_OOM_FAILED;
    }
    cf_value_int(&v, 1);
    for (i = 0; i < 500; ++i) {
        char   key[32];
        size_t n = cf_fmt(key, sizeof key, "k.%zu", i);
        if (!cf_ks_add(&ks, key, n, 0, &v, NULL)) {
            /* refused, which is the correct answer */
            cf_error_set(err, CF_E_MEMORY, NULL, NULL, "no room for key %u",
                         (unsigned)i);
            rc = CF_OOM_FAILED;
            break;
        }
    }
    if (rc == CF_OOM_DONE && !cf_ks_resolve(&ks)) {
        cf_error_set(err, CF_E_MEMORY, NULL, NULL, "no room to resolve the key space");
        rc = CF_OOM_FAILED;
    }
    if (rc == CF_OOM_DONE) {
        *digest = (cf_u64)cf_ks_count(&ks);
    }
    cf_arena_free(ar);
    return rc;
}

static void test_sweep_with_sources(void) {
    (void)cf_oom_sweep("a key space of five hundred keys", try_keyspace, NULL);
}

struct oom_list_input {
    const cf_allocator* allocator;
    const cf_pair*      pairs;
    size_t              count;
    cf_bool             writable;
};

static unsigned g_oom_writes;

static cf_bool oom_store_load(const cf_source* s, cf_keyspace* ks) {
    return cf_ks_add_text(ks, "tags.count", 10, s->rank, "1", 1, "test store") &&
           cf_ks_add_text(ks, "tags.0", 6, s->rank, "stored", 6, "test store");
}

static cf_bool oom_store_write(const cf_source* s, const cf_write_item* items,
                               size_t count, const char* meta, cf_error* err) {
    (void)s;
    (void)meta;
    (void)err;
    ++g_oom_writes;
    check(count == 1 && strcmp(items[0].key, "tags.0") == 0 &&
              items[0].op == CF_WRITE_SET && items[0].value.type == CF_TYPE_TEXT &&
              items[0].value.u.t.n == 3 && memcmp(items[0].value.u.t.p, "new", 3) == 0,
          "the write callback receives the complete source-local assignment");
    return CF_TRUE;
}

static const cf_source_vtable oom_store_vt = {
    sizeof(cf_source_vtable), "test-store", oom_store_load, NULL, NULL,
    oom_store_write};

/* The build function is the caller's, and "no builder" is all it can
   say to cf_live_reload when cf_build was refused: the library cannot
   tell that from a caller that declined. The case knows. */
static int g_build_refused;

static cf_builder* oom_list_build(void* user) {
    struct oom_list_input* input = (struct oom_list_input*)user;
    cf_builder*            b     = cf_build(input->allocator);
    cf_def                 d;
    cf_value               v;
    if (b == NULL) {
        g_build_refused = 1;
        return NULL;
    }
    cf_def_init(&d, CF_TYPE_TEXT, "tags");
    d.list_mode   = CF_LIST_APPEND;
    d.flags       = CF_RESTART;
    d.has_default = CF_TRUE;
    cf_value_text(&v, "default", 7);
    d.list_defaults      = &v;
    d.list_default_count = 1;
    cf_builder_define(b, &d);
    cf_builder_text(b, "level", "old", 0);
    if (input->writable) {
        cf_source s;
        memset(&s, 0, sizeof s);
        s.vt   = &oom_store_vt;
        s.name = "store";
        s.rank = CF_RANK_USER_FILE;
        cf_builder_source(b, &s);
        cf_builder_writable(b, "store");
    } else {
        cf_builder_memory(b, "mem", CF_RANK_USER_FILE, input->pairs, input->count);
    }
    return b;
}

static const cf_pair g_reload_before[] = {
    {"tags.count", "1"}, {"tags.0", "$${literal}"}, {"level", "old"}};
static const cf_pair g_reload_after[] = {
    {"tags.count", "2"}, {"tags.0", "new"}, {"tags.1", "extra"}, {"level", "new"}};

/* The configuration is opened with every allocation allowed; only the
   reload is refused. */
static int try_list_reload(void* ctx, const cf_allocator* alloc, cf_u64* digest,
                           cf_error* err) {
    struct oom_list_input input;
    cf_live*              live;
    cf_config*            c;
    cf_live_result        result;
    cf_u64                before;
    int                   rc = CF_OOM_DONE;

    (void)ctx;
    *digest = 0;
    cf_oom_pause(alloc);
    input.allocator = alloc;
    input.pairs     = g_reload_before;
    input.count     = 3;
    input.writable  = CF_FALSE;
    live = cf_live_open(CF_LIVE_MANUAL, oom_list_build, NULL, &input, alloc, err);
    check(live != NULL, "open the list before injecting reload failures");
    if (live == NULL) {
        return CF_OOM_BAD;
    }
    c      = cf_live_get(live);
    before = cf_digest(c);
    cf_config_free(c);
    input.pairs     = g_reload_after;
    input.count     = 4;
    g_build_refused = 0;
    cf_oom_resume(alloc);

    result = cf_live_reload(live, err);
    c      = cf_live_get(live);
    if (result == CF_LIVE_FAILED) {
        if (g_build_refused && err->code == CF_E_SOURCE) {
            cf_error_set(err, CF_E_MEMORY, NULL, NULL, "no room for a builder");
        }
        check(cf_digest(c) == before && cf_live_generation(live) == 1,
              "a failed list reload leaves the running snapshot intact");
        rc = CF_OOM_FAILED;
    } else {
        check(result == CF_LIVE_SWAPPED && cf_get_int(c, "tags.count", -1) == 2 &&
                  strcmp(cf_get_text(c, "tags.1", ""), "${literal}") == 0 &&
                  strcmp(cf_get_text(c, "level", ""), "new") == 0,
              "a successful reload keeps the complete list, never a partial overlay");
        *digest = cf_digest(c);
    }
    cf_config_free(c);
    cf_live_close(live);
    return rc;
}

static void test_list_reload_sweep(void) {
    (void)cf_oom_sweep("a list reload", try_list_reload, NULL);
}

/* The configuration is built with every allocation allowed; only the
   write is refused. */
static int try_list_write(void* ctx, const cf_allocator* alloc, cf_u64* digest,
                          cf_error* err) {
    struct oom_list_input input;
    cf_config*            c;
    cf_write_item         item;
    cf_u64                before;
    int                   rc = CF_OOM_DONE;

    (void)ctx;
    *digest = 0;
    cf_oom_pause(alloc);
    memset(&input, 0, sizeof input);
    input.allocator = alloc;
    input.writable  = CF_TRUE;
    c               = cf_builder_create(oom_list_build(&input), err);
    check(c != NULL, "build the list before injecting write failures");
    if (c == NULL) {
        return CF_OOM_BAD;
    }
    before   = cf_digest(c);
    item.key = "tags.1";
    item.op  = CF_WRITE_SET;
    cf_value_text(&item.value, "new", 3);
    g_oom_writes = 0;
    cf_oom_resume(alloc);

    if (!cf_write(c, "store", &item, 1, NULL, err)) {
        check(g_oom_writes == 0,
              "a failed write never calls the source with partial mapped keys");
        rc = CF_OOM_FAILED;
    } else {
        check(g_oom_writes == 1, "a successful write calls its source exactly once");
    }
    check(cf_digest(c) == before && cf_get_int(c, "tags.count", -1) == 2 &&
              strcmp(cf_get_text(c, "tags.1", ""), "stored") == 0,
          "write authorization and mapping never mutate the read snapshot");
    *digest = cf_digest(c);
    cf_config_free(c);
    return rc;
}

static void test_list_write_sweep(void) {
    (void)cf_oom_sweep("a mapped write", try_list_write, NULL);
}

int main(void) {
    /* Every allocation in a run, refused in turn, at three sizes. The
       count has to grow with the configuration or the sweeps are not
       reaching further than each other. */
    sweep(0, "a small configuration");
    sweep(64, "sixty-four more keys");
    sweep(400, "four hundred more keys");
    /* The widest sweep: enough keys that the arena takes dozens of
       blocks, so dozens of different places in the library get to be
       the one where the machine ran out. */
    sweep(512, "five hundred more keys");
    check(g_not_growing == 0, "a bigger configuration took strictly more allocations");

    test_sweep_with_sources();
    test_list_reload_sweep();
    test_list_write_sweep();

    return cf_test_done();
}
