/*
 * test_wipe.c - a secret leaves nothing behind when its configuration
 * goes.
 *
 * The allocator below remembers every block it hands out and, when one
 * comes back, searches it for the secret before letting it go. Memory
 * returned to an allocator is memory the next allocation, a core dump or
 * a swap file can show to somebody else, so finding the secret there is
 * the failure - whether it came from a value, a file's text or a key
 * space's scratch.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int  g_fail = 0, g_run = 0;
static void check(int c, const char* w) {
    ++g_run;
    if (!c) {
        ++g_fail;
        printf("FAIL: %s\n", w);
    }
}

static const char g_secret[] = "hunter2-correct-horse";

#define MAX_BLOCKS 256

typedef struct watching {
    void*  p[MAX_BLOCKS];
    size_t n[MAX_BLOCKS];
    int    count;
    int    released;
    int    leaked; /* blocks that still held the secret when released */
} watching;

static void* w_alloc(void* ctx, size_t n) {
    watching* w = (watching*)ctx;
    void*     p = malloc(n);
    if (p != NULL && w->count < MAX_BLOCKS) {
        w->p[w->count] = p;
        w->n[w->count] = n;
        ++w->count;
    }
    return p;
}

static cf_bool holds_secret(const char* p, size_t n) {
    size_t k = sizeof g_secret - 1;
    size_t i;
    for (i = 0; i + k <= n; ++i) {
        if (memcmp(p + i, g_secret, k) == 0) {
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}

static void w_release(void* ctx, void* p) {
    watching* w = (watching*)ctx;
    int       i;
    for (i = 0; i < w->count; ++i) {
        if (w->p[i] == p) {
            if (holds_secret((const char*)p, w->n[i])) {
                ++w->leaked;
            }
            w->p[i] = NULL;
            ++w->released;
            break;
        }
    }
    free(p);
}

static cf_bool spill(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        return CF_FALSE;
    }
    fputs(text, f);
    fclose(f);
    return CF_TRUE;
}

static void test_value_and_file(void) {
    static watching w; /* four kilobytes: over the stack budget */
    cf_allocator    a;
    cf_builder*     b;
    cf_config*      c;
    cf_error        err = CF_ERROR_INIT;
    const char*     got;

    memset(&w, 0, sizeof w);
    a.alloc   = w_alloc;
    a.release = w_release;
    a.ctx     = &w;

    check(spill("cf_wipe.ini", "[db]\npassword = hunter2-correct-horse\n"),
          "wrote a file with a secret in it");
    b = cf_build(&a);
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_wipe.ini", NULL, 0);
    cf_builder_text(b, "db.password", NULL, CF_REQUIRED | CF_SENSITIVE);
    c = cf_builder_create(b, &err);
    check(c != NULL, "builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        remove("cf_wipe.ini");
        return;
    }
    /* The secret is in the library's memory, not only in the file: the
       test would prove nothing if it never got there. */
    got = cf_get_text(c, "db.password", "");
    check(strcmp(got, g_secret) == 0, "the configuration holds the secret");
    check(w.count > 0, "and its memory came from the watched allocator");

    cf_config_free(c);
    check(w.released > 0, "freeing released the memory");
    check(w.leaked == 0, "and none of it still held the secret");
    remove("cf_wipe.ini");
}

/* A build that fails after the secret was read: the builder is consumed
   by cf_builder_create either way, and the failure path is the one
   nobody looks at. Not an abandoned builder - nothing is read until
   create, so one of those never held the secret, and a test of it
   passed whether or not anything was wiped. */
static void test_failed_build(void) {
    static watching w;
    cf_allocator    a;
    cf_builder*     b;
    cf_config*      c;
    cf_error        err = CF_ERROR_INIT;

    memset(&w, 0, sizeof w);
    a.alloc   = w_alloc;
    a.release = w_release;
    a.ctx     = &w;

    check(spill("cf_wipe2.ini", "[api]\ntoken = hunter2-correct-horse\n"),
          "wrote a file with a secret in it");
    b = cf_build(&a);
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_wipe2.ini", NULL, 0);
    cf_builder_text(b, "api.token", NULL, CF_SENSITIVE);
    cf_builder_text(b, "api.url", NULL, CF_REQUIRED);
    c = cf_builder_create(b, &err);
    check(c == NULL, "a missing required key fails the build");
    check(w.released > 0, "the failed build released its memory");
    check(w.leaked == 0, "wiped first");
    cf_config_free(c);
    remove("cf_wipe2.ini");
}

static void test_fixed_arena(void) {
    static char buf[16384];
    cf_arena*   ar = cf_arena_fixed(buf, sizeof buf);
    char*       s  = cf_arena_dup(ar, g_secret, sizeof g_secret - 1);

    check(s != NULL && holds_secret(buf, sizeof buf), "a fixed arena holds the secret");
    cf_arena_wipe(ar);
    check(!holds_secret(buf, sizeof buf), "cf_arena_wipe leaves the buffer clean");
}

int main(void) {
    test_value_and_file();
    test_failed_build();
    test_fixed_arena();
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
