/*
 * test_oom_file.c - the file layer when memory runs out (K-9b, D-237).
 *
 * cf_file_writer_open takes the writer, and the stream's buffer when the
 * options ask for one, from options->allocator. Each case here refuses the
 * 1st, 2nd, ... nth allocation in turn (tests/support/cf_oom.h): the open
 * says CF_E_MEMORY, no writer exists, nothing is outstanding.
 *
 * cf_file_replace takes no allocator and allocates nothing: its temporary
 * is named on the stack. lib/config/tests/oom_paths.txt says so.
 */

#include <configforge/cf_alloc.h>
#include <configforge/cf_error.h>
#include <configforge/cf_file.h>
#include <configforge/cf_port.h>

#include <stdio.h>
#include <string.h>

#include "cf_test.h"

#include "cf_oom.h"

#define OUT "cf_oom_file.out"

static cf_u64 hash_file(const char* path) {
    FILE*  f = fopen(path, "rb");
    cf_u64 h = 5381u;
    int    c;

    if (f == NULL) {
        return 0;
    }
    while ((c = fgetc(f)) != EOF) {
        h = (h * 131u + (unsigned char)c) & 0xFFFFFFFFu;
    }
    fclose(f);
    return h;
}

/* A writer with its own buffer, so that the open takes two blocks. */
static int try_file_writer(void* ctx, const cf_allocator* a, cf_u64* digest,
                           cf_error* err) {
    cf_file_options o;
    cf_file_writer* w;

    (void)ctx;
    *digest = 0;
    remove(OUT);
    cf_file_options_init(&o);
    o.buffering   = CF_BUF_FULL;
    o.buffer_size = 4096u;
    o.allocator   = a;

    w = cf_file_writer_open(OUT, &o, err);
    if (w == NULL) {
        return CF_OOM_FAILED;
    }
    if (!cf_file_writer_write(w, "hello\n", 6u, err)) {
        check(0, "a writer that opened writes");
        (void)cf_file_writer_close(w, err);
        return CF_OOM_BAD;
    }
    if (!cf_file_writer_close(w, err)) {
        check(0, "a writer that wrote closes");
        return CF_OOM_BAD;
    }
    *digest = hash_file(OUT);
    return CF_OOM_DONE;
}

static void test_file_writer(void) {
    cf_error        err = CF_ERROR_INIT;
    cf_file_writer* w   = cf_file_writer_open(OUT, NULL, &err);
    size_t          calls;

    /* A platform with no files to write (none) answers CF_E_CAPABILITY. */
    if (w == NULL && err.code == CF_E_CAPABILITY) {
        puts("skipped: this platform has no file writer");
        return;
    }
    if (w != NULL) {
        (void)cf_file_writer_close(w, &err);
    }
    calls = cf_oom_sweep("a file writer with a buffer", try_file_writer, NULL);

    check(calls >= 2u, "the open takes the writer and the buffer from the allocator");
    remove(OUT);
}

int main(void) {
    test_file_writer();

    return cf_test_done();
}
