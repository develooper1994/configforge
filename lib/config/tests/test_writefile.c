/*
 * test_writefile.c - the file layer (D-224, D-127): a file replaced whole
 * or not at all, by a temporary in its directory, and a writer that
 * buffers and syncs as its options say.
 *
 * Every step that can fail is failed in turn (cf_platform_write_file_with)
 * and the old file must be found intact, with no temporary left.
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200112L /* chmod, stat */
#endif

#include <configforge/cf_platform.h>
#include <configforge/cf_file.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if CF_PLATFORM_POSIX
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include "cf_test.h"

#define BIG (3u * 1024u * 1024u)

static char g_big[BIG + 8];
static char g_back[BIG + 8];

/* What the file holds, up to cap bytes: its length, or (size_t)-1 when it
   cannot be opened. */
static size_t slurp(const char* path, char* buf, size_t cap) {
    FILE*  f = fopen(path, "rb");
    size_t n;

    if (f == NULL) {
        return (size_t)-1;
    }
    n = fread(buf, 1, cap, f);
    fclose(f);
    return n;
}

/* The file holds exactly this text. */
static int holds(const char* path, const char* text) {
    static char buf[256];
    size_t      n = slurp(path, buf, sizeof buf - 1);

    if (n == (size_t)-1) {
        return 0;
    }
    buf[n] = '\0';
    return strcmp(buf, text) == 0;
}

static int exists(const char* path) {
    FILE* f = fopen(path, "rb");

    if (f == NULL) {
        return 0;
    }
    fclose(f);
    return 1;
}

/* The retry hook: the reader that holds the target lets go. */
static unsigned g_closed = 0;
static void     closer(void* user) {
    FILE** fp = (FILE**)user;

    ++g_closed;
    if (*fp != NULL) {
        fclose(*fp);
        *fp = NULL;
    }
}

/* ---------------------------------------------------------------- */

static void test_replace(void) {
    cf_error err = CF_ERROR_INIT;
    size_t   i, n;

    (void)remove("cf_wf.txt");

    /* (a) a new file, byte for byte */
    check(cf_file_replace("cf_wf.txt", "hello\n", 6, NULL, &err),
          "a new file is written");
    check(holds("cf_wf.txt", "hello\n"), "the new file reads back");
    check(!exists(".cf_wf.txt.cf-tmp-0"), "no temporary after a success");

    /* (b) an existing file replaced, by sizes that make partial writes */
    check(cf_file_replace("cf_wf.txt", "", 0, NULL, &err),
          "an empty replacement is fine");
    check(slurp("cf_wf.txt", g_back, sizeof g_back) == 0, "the file is empty");
    check(cf_file_replace("cf_wf.txt", "x", 1, NULL, &err), "one byte");
    check(holds("cf_wf.txt", "x"), "one byte reads back");
    for (i = 0; i < BIG; ++i) {
        g_big[i] = (char)('a' + (int)(i % 23u));
    }
    check(cf_file_replace("cf_wf.txt", g_big, BIG, NULL, &err), "3 MiB");
    n = slurp("cf_wf.txt", g_back, sizeof g_back);
    check(n == BIG, "3 MiB: the length");
    check(n == BIG && memcmp(g_big, g_back, BIG) == 0, "3 MiB: the bytes");
    check(cf_file_replace("cf_wf.txt", "old", 3, NULL, &err), "back to a short file");
    check(holds("cf_wf.txt", "old"), "a shorter file replaces a longer");

#if CF_PLATFORM_POSIX
    /* (c) the mode of the original is kept */
    {
        struct stat st;

        check(chmod("cf_wf.txt", 0640) == 0, "chmod 0640");
        check(cf_file_replace("cf_wf.txt", "new", 3, NULL, &err),
              "replace a 0640 file");
        check(stat("cf_wf.txt", &st) == 0 && (st.st_mode & 07777) == 0640,
              "the mode is still 0640");
    }
#endif

    /* (f) what is refused before anything is made */
    check(!cf_file_replace(".", "x", 1, NULL, &err), "a directory is refused");
    has(err.message, "is not a regular file", "a directory: why");
    check(!cf_file_replace(NULL, "x", 1, NULL, &err), "no path");
    has(err.message, "no file to write", "no path: why");
    check(!cf_file_replace("cf_wf.txt", NULL, 3, NULL, &err), "no data");
    has(err.message, "no file to write", "no data: why");

#if CF_PLATFORM_POSIX
    /* (g) a symbolic link is refused, and its target is unchanged */
    (void)remove("cf_wf.lnk");
    (void)spill("cf_wf.txt", "old");
    if (system("ln -s cf_wf.txt cf_wf.lnk") == 0) {
        check(!cf_file_replace("cf_wf.lnk", "new", 3, NULL, &err), "a symbolic link");
        has(err.message, "is a symbolic link", "a symbolic link: why");
        check(holds("cf_wf.txt", "old"), "the target of the link is unchanged");
        (void)remove("cf_wf.lnk");
    }
#endif
    (void)remove("cf_wf.txt");
}

/* (d), (e): every step failed in turn */
static void test_steps(void) {
    cf_error err = CF_ERROR_INIT;
    unsigned step, steps = cf_platform_write_steps();
    char     want[64];

    for (step = 1; step <= steps; ++step) {
        sprintf(want, "injected failure at step %u", step);

        (void)spill("cf_wf.txt", "old");
        check(cf_platform_write_file_with("cf_wf.txt", "new", 3, NULL, step, NULL, NULL,
                                          &err) == 0,
              "a failed step returns 0");
        has(err.message, want, "the step that failed is named");
        check(err.code == CF_E_SOURCE, "a failed step is CF_E_SOURCE");
        check(holds("cf_wf.txt", "old"), "the old file is intact");
        check(!exists(".cf_wf.txt.cf-tmp-0"), "no temporary is left");

        (void)remove("cf_wf_new.txt");
        check(cf_platform_write_file_with("cf_wf_new.txt", "new", 3, NULL, step, NULL,
                                          NULL, &err) == 0,
              "a failed step with no original returns 0");
        has(err.message, want, "no original: the step is named");
        check(!exists("cf_wf_new.txt"), "no original: still no file");
        check(!exists(".cf_wf_new.txt.cf-tmp-0"), "no original: no temporary");
    }
    check(cf_platform_write_file_with("cf_wf.txt", "new", 3, NULL, 0, NULL, NULL,
                                      &err) == 1,
          "no injected failure: the write is done");
    check(holds("cf_wf.txt", "new"), "and it is the new content");

    /* a temporary a crash left is not read, removed or reused */
    (void)spill(".cf_wf.txt.cf-tmp-0", "crash");
    check(cf_file_replace("cf_wf.txt", "again", 5, NULL, &err),
          "a taken temporary name");
    check(holds("cf_wf.txt", "again"), "the next name was used");
    check(holds(".cf_wf.txt.cf-tmp-0", "crash"),
          "the other process's temporary is kept");
    (void)remove(".cf_wf.txt.cf-tmp-0");
    (void)remove("cf_wf.txt");
}

/* (h) the rename waits for a reader on Windows, and at once elsewhere */
static void test_retry(void) {
    cf_error        err = CF_ERROR_INIT;
    cf_file_options o;
    FILE*           fp;

    (void)spill("cf_wf.txt", "old");
    cf_file_options_init(&o);
    o.retry_ms = 1500;
    fp         = (FILE*)cf_platform_fopen("cf_wf.txt", "rb");
    check(fp != NULL, "the reader opened the target");
    g_closed = 0;
    check(cf_platform_write_file_with("cf_wf.txt", "new", 3, &o, 0, closer, &fp,
                                      &err) == 1,
          "the write waits for the reader");
    check(holds("cf_wf.txt", "new"), "after the wait the file is new");
#if CF_PLATFORM_WIN32
    check(g_closed >= 1, "the retry hook was called");
#endif
    if (fp != NULL) {
        fclose(fp);
    }

#if CF_PLATFORM_WIN32
    /* a reader that never lets go: the retry gives up, and the file is as it was.
       (Elsewhere a rename over an open file is fine, so there is nothing to give up.)
     */
    o.retry_ms = 100;
    fp         = (FILE*)cf_platform_fopen("cf_wf.txt", "rb");
    check(fp != NULL, "the second reader opened the target");
    check(cf_platform_write_file_with("cf_wf.txt", "again", 5, &o, 0, NULL, NULL,
                                      &err) == 0,
          "the retry gives up");
    has(err.message, "cannot write", "the retry gave up: why");
    check(holds("cf_wf.txt", "new"), "the file is as it was");
    check(!exists(".cf_wf.txt.cf-tmp-0"), "no temporary after the retry gave up");
    if (fp != NULL) {
        fclose(fp);
    }
#endif
    (void)remove("cf_wf.txt");
}

/* (i) the options */
static void test_options(void) {
    cf_error        err = CF_ERROR_INIT;
    cf_file_options o;
    cf_file_writer* w;

    memset(&o, 0x5a, sizeof o);
    cf_file_options_init(&o);
    check(o.size == sizeof o, "options: size");
    check(o.buffering == CF_BUF_PLATFORM, "options: buffering");
    check(o.buffer_size == 0, "options: buffer_size");
    check(o.sync == CF_SYNC_DEFAULT, "options: sync");
    check(o.sync_bytes == 1048576UL, "options: sync_bytes");
    check(o.sync_ms == 1000UL, "options: sync_ms");
    check(o.retry_ms == 2000UL, "options: retry_ms");
    check(o.append == CF_FALSE, "options: append");
    cf_file_options_init(NULL);

    o.size = 1;
    w      = cf_file_writer_open("cf_wf_w.txt", &o, &err);
    check(w == NULL, "a size of 1 is refused");
    has(err.message, "of an unknown size", "an unknown size: why");
    check(!cf_file_replace("cf_wf_w.txt", "x", 1, &o, &err), "replace refuses it too");
    has(err.message, "of an unknown size", "replace, an unknown size: why");
    check(!exists("cf_wf_w.txt"), "nothing was made");
}

/* (j) the writer */
static void test_writer(void) {
    cf_error                 err = CF_ERROR_INIT;
    cf_file_options          o;
    cf_file_writer*          w;
    unsigned long            syncs;
    unsigned long            t0;
    int                      i;
    static const cf_buf_mode modes[3] = {CF_BUF_NONE, CF_BUF_LINE, CF_BUF_FULL};

    /* the default policy does not sync */
    w = cf_file_writer_open("cf_wf_w.txt", NULL, &err);
    check(w != NULL, "writer: open with NULL options");
    if (w == NULL) {
        return;
    }
    check(cf_file_writer_write(w, "one\n", 4, &err), "writer: write 1");
    check(cf_file_writer_write(w, "two\n", 4, &err), "writer: write 2");
    check(cf_file_writer_write(w, "three\n", 6, &err), "writer: write 3");
    check(cf_file_writer_flush(w, &err), "writer: flush");
    check(cf_platform_writer_syncs(w) == 0, "writer: the default syncs nothing");
    check(cf_platform_writer_close_counted(w, &syncs, &err), "writer: close");
    check(syncs == 0, "writer: the default does not sync at close either");
    check(holds("cf_wf_w.txt", "one\ntwo\nthree\n"), "writer: the bytes");

    /* append keeps what is there */
    cf_file_options_init(&o);
    o.append = CF_TRUE;
    w        = cf_file_writer_open("cf_wf_w.txt", &o, &err);
    check(w != NULL, "writer: open to append");
    if (w != NULL) {
        check(cf_file_writer_write(w, "four\n", 5, &err), "writer: append");
        check(cf_file_writer_close(w, &err), "writer: close after append");
    }
    check(holds("cf_wf_w.txt", "one\ntwo\nthree\nfour\n"),
          "writer: append keeps the old bytes");

    /* without append the file starts again */
    w = cf_file_writer_open("cf_wf_w.txt", NULL, &err);
    check(w != NULL, "writer: reopen");
    if (w != NULL) {
        check(cf_file_writer_close(w, &err), "writer: close an empty file");
    }
    check(holds("cf_wf_w.txt", ""), "writer: truncated by default");

    /* COMMIT: once, at close */
    cf_file_options_init(&o);
    o.sync = CF_SYNC_COMMIT;
    w      = cf_file_writer_open("cf_wf_w.txt", &o, &err);
    check(w != NULL, "writer: open COMMIT");
    if (w != NULL) {
        check(cf_file_writer_write(w, "a", 1, &err), "writer: COMMIT write 1");
        check(cf_file_writer_write(w, "b", 1, &err), "writer: COMMIT write 2");
        check(cf_platform_writer_syncs(w) == 0,
              "writer: COMMIT syncs nothing before the close");
        check(cf_platform_writer_close_counted(w, &syncs, &err),
              "writer: COMMIT close");
        check(syncs == 1, "writer: COMMIT syncs once, at close");
    }

    /* EACH: after every write */
    cf_file_options_init(&o);
    o.sync = CF_SYNC_EACH;
    w      = cf_file_writer_open("cf_wf_w.txt", &o, &err);
    check(w != NULL, "writer: open EACH");
    if (w != NULL) {
        check(cf_file_writer_write(w, "a", 1, &err), "writer: EACH write 1");
        check(cf_file_writer_write(w, "b", 1, &err), "writer: EACH write 2");
        check(cf_file_writer_write(w, "c", 1, &err), "writer: EACH write 3");
        check(cf_platform_writer_syncs(w) == 3, "writer: EACH syncs after every write");
        check(cf_file_writer_close(w, &err), "writer: EACH close");
    }

    /* EVERY, by bytes */
    cf_file_options_init(&o);
    o.sync       = CF_SYNC_EVERY;
    o.sync_bytes = 4;
    o.sync_ms    = 0;
    w            = cf_file_writer_open("cf_wf_w.txt", &o, &err);
    check(w != NULL, "writer: open EVERY bytes");
    if (w != NULL) {
        check(cf_file_writer_write(w, "aaa", 3, &err), "writer: EVERY write 1");
        check(cf_platform_writer_syncs(w) == 0, "writer: EVERY: 3 bytes is not enough");
        check(cf_file_writer_write(w, "bbb", 3, &err), "writer: EVERY write 2");
        check(cf_platform_writer_syncs(w) == 1, "writer: EVERY: 6 bytes passed 4");
        check(cf_file_writer_write(w, "c", 1, &err), "writer: EVERY write 3");
        check(cf_platform_writer_syncs(w) == 1,
              "writer: EVERY: the count started again");
        check(cf_file_writer_close(w, &err), "writer: EVERY close");
    }

    /* EVERY, by time (a clock this platform may lack) */
    cf_file_options_init(&o);
    o.sync       = CF_SYNC_EVERY;
    o.sync_bytes = 0;
    o.sync_ms    = 100;
    w            = cf_file_writer_open("cf_wf_w.txt", &o, &err);
    if (cf_platform_now_ms() == (unsigned long)-1) {
        check(w == NULL, "writer: no clock, no sync_ms");
        has(err.message, "no clock for sync_ms", "writer: no clock: why");
    } else {
        check(w != NULL, "writer: open EVERY time");
        if (w != NULL) {
            check(cf_file_writer_write(w, "a", 1, &err), "writer: EVERY time write 1");
            check(cf_platform_writer_syncs(w) == 0, "writer: EVERY time: too soon");
            t0 = cf_platform_now_ms();
            while (cf_platform_now_ms() - t0 < 250UL) {
                /* wait: a loop on the clock */
            }
            check(cf_file_writer_write(w, "b", 1, &err), "writer: EVERY time write 2");
            check(cf_platform_writer_syncs(w) == 1,
                  "writer: EVERY time: the wait passed 100 ms");
            check(cf_file_writer_close(w, &err), "writer: EVERY time close");
        }
    }

    /* every buffering writes the same bytes */
    for (i = 0; i < 3; ++i) {
        cf_file_options_init(&o);
        o.buffering   = modes[i];
        o.buffer_size = (modes[i] == CF_BUF_FULL) ? 4096 : 0;
        w             = cf_file_writer_open("cf_wf_w.txt", &o, &err);
        check(w != NULL, "writer: open with a buffering");
        if (w != NULL) {
            check(cf_file_writer_write(w, "line one\n", 9, &err),
                  "writer: buffered write 1");
            check(cf_file_writer_write(w, "line two\n", 9, &err),
                  "writer: buffered write 2");
            check(cf_file_writer_flush(w, &err), "writer: buffered flush");
            check(cf_file_writer_sync(w, &err), "writer: buffered sync");
            check(cf_file_writer_close(w, &err), "writer: buffered close");
        }
        check(holds("cf_wf_w.txt", "line one\nline two\n"),
              "writer: the bytes, whatever the buffering");
    }
    (void)remove("cf_wf_w.txt");

    /* (k) */
    check(cf_file_writer_close(NULL, &err), "writer: close(NULL) is fine");
}

int main(void) {
    cf_error err = CF_ERROR_INIT;

    if (cf_platform_write_steps() == 0) {
        printf("skipped: no files to write\n");
        check(!cf_file_replace("cf_wf.txt", "x", 1, NULL, &err), "none: replace fails");
        check(err.code == CF_E_CAPABILITY, "none: CF_E_CAPABILITY");
        return cf_test_done();
    }
    /* what an earlier run that failed may have left */
    (void)remove(".cf_wf.txt.cf-tmp-0");
    (void)remove(".cf_wf_new.txt.cf-tmp-0");
    test_replace();
    test_steps();
    test_retry();
    test_options();
    test_writer();
    return cf_test_done();
}
