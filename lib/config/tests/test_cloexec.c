/*
 * test_cloexec.c - every descriptor the library keeps open is close-on-exec
 * (POSIX) and not inheritable (Windows), D-190.
 *
 * One binary that is its own child: with `--fds <file>` it lists, into the
 * file, the numbers of the descriptors 3 to 255 that are open, and returns.
 * The parent holds a file opened by cf_platform_fopen, a watcher, a file
 * writer and a sidecar lock, runs the child, and checks that none of them
 * is in its list. A control opened with plain fopen is, so the list works.
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200112L /* fileno, fcntl, system */
#endif

#include <configforge/cf_file.h>
#include <configforge/cf_lock.h>
#include <configforge/cf_platform.h>
#include <configforge/configforge.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define fileno _fileno
#include <crtdbg.h>
#include <io.h>
#include <process.h>
#include <stdlib.h>
#else
#include <fcntl.h>
#endif

#include "cf_test.h"

#define CX_MAXFD 256

#ifdef _WIN32
static void cx_ignore(const wchar_t* e, const wchar_t* f, const wchar_t* g, unsigned l,
                      uintptr_t r) {
    (void)e;
    (void)f;
    (void)g;
    (void)l;
    (void)r;
}
#endif

static int fd_open(int fd) {
#ifdef _WIN32
    return _get_osfhandle(fd) != -1;
#else
    return fcntl(fd, F_GETFD) != -1;
#endif
}

/* The open descriptors 3 to 255, one per line, into `file`. */
static int list_fds(const char* file) {
    FILE* f;
    int   fd;

#ifdef _WIN32
    (void)_set_invalid_parameter_handler(cx_ignore);
    (void)_CrtSetReportMode(_CRT_ASSERT, 0);
#endif
    f = fopen(file, "w");
    if (f == NULL) {
        return 0;
    }
    for (fd = 3; fd < CX_MAXFD; ++fd) {
        if (fd_open(fd) && fd != fileno(f)) {
            fprintf(f, "%d\n", fd);
        }
    }
    fclose(f);
    return 1;
}

/* Whether `fd` is a line of `file`. */
static int listed(const char* file, int fd) {
    FILE* f = fopen(file, "r");
    int   n;
    int   found = 0;

    if (f == NULL) {
        return 0;
    }
    while (fscanf(f, "%d", &n) == 1) {
        if (n == fd) {
            found = 1;
        }
    }
    fclose(f);
    return found;
}

static int run_child(const char* self) {
#ifdef _WIN32
    return (int)_spawnl(_P_WAIT, self, self, "--fds", "cf_fds.txt", (char*)NULL);
#else
    char cmd[1024];

    if (strlen(self) > 900) {
        return -1;
    }
    sprintf(cmd, "\"%s\" --fds cf_fds.txt", self);
    return system(cmd);
#endif
}

static void test_children(const char* self) {
    cf_error        err = CF_ERROR_INIT;
    FILE*           held;
    FILE*           control;
    cf_file_writer* w;
    cf_lock         lk = {-1, NULL};
    cf_watcher*     wt = NULL;
    cf_config*      c  = NULL;
    int             ids[4];
    int             nid = 0;
    int             i;
    int             rc;

    /* A platform with no files to write (none) has no descriptors the
       library could mark: it answers the writer with CF_E_CAPABILITY. */
    w = cf_file_writer_open("cf_cx_w.txt", NULL, &err);
    if (w == NULL && err.code == CF_E_CAPABILITY) {
        puts("skipped: this platform has no file writer");
        return;
    }
    if (w != NULL) {
        (void)cf_file_writer_close(w, &err);
    }
    /* What the test was started with (a harness may pass descriptors of its
       own): not the library's, and the child may have them. */
    check(list_fds("cf_fds_base.txt"), "the parent lists its starting descriptors");
    check(spill("cf_cx.ini", "[server]\nport = 3000\n"), "wrote a file");
    held = (FILE*)cf_platform_fopen("cf_cx.ini", "rb");
    check(held != NULL, "cf_platform_fopen opens it");
    if (held == NULL) {
        return;
    }
    ids[nid++] = fileno(held);

#if CF_WITH_INI
    {
        cf_builder* b = cf_build(NULL);

        cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_cx.ini", NULL, 0);
        cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
        c = cf_builder_create(b, &err);
    }
    if (c != NULL) {
        wt = cf_watcher_open(c, NULL, &err);
    }
#endif
    if (wt != NULL && cf_watcher_fd(wt) >= 0) {
        ids[nid++] = cf_watcher_fd(wt);
    } else {
        printf("skipped: no watcher descriptor\n");
    }

    w = cf_file_writer_open("cf_cx_w.txt", NULL, &err);
    check(w != NULL, "the writer opens");
    if (cf_lock_sidecar()->take(cf_lock_sidecar(), &lk, "cf_cx.ini", 0, &err)) {
#ifndef _WIN32
        if (lk.handle >= 0 && lk.handle < CX_MAXFD) {
            ids[nid++] = (int)lk.handle;
        }
#endif
    } else {
        printf("skipped: no sidecar lock\n");
    }
    /* The writer's descriptor is whatever the list shows beyond the rest:
       the control is the proof that the list works, and the writer's own
       number is every number the child lists that the parent does too
       (below), so the writer needs no fileno of its own. */
    control = fopen("cf_cx.ini", "rb");
    check(control != NULL, "the control opens");
    if (control == NULL) {
        return;
    }

    check(list_fds("cf_fds_parent.txt"), "the parent lists its descriptors");
    rc = run_child(self);
    check(rc == 0, "the child ran");
    check(listed("cf_fds.txt", fileno(control)),
          "the child's list is empty: the control descriptor was not seen");
    for (i = 0; i < nid; ++i) {
        check(!listed("cf_fds.txt", ids[i]),
              "the child does not hold a library descriptor");
    }
    /* Every descriptor the parent opened since it started, but the control, is
       the library's (the writer's too): the child may hold only the control. */
#if defined(CF_TEST_LIBKQUEUE) && CF_TEST_LIBKQUEUE
    /* libkqueue, the Linux stand-in for kqueue, opens descriptors of its own
       that are not close-on-exec: the sweep would test the stand-in. The
       watcher's own descriptor is checked above; a real kqueue has no
       others. */
    puts("skipped: libkqueue's own descriptors are not close-on-exec");
#else
    for (i = 3; i < CX_MAXFD; ++i) {
        if (i != fileno(control) && !listed("cf_fds_base.txt", i) &&
            listed("cf_fds_parent.txt", i)) {
            check(!listed("cf_fds.txt", i), "no other parent descriptor is inherited");
        }
    }
#endif

    fclose(control);
    if (w != NULL) {
        (void)cf_file_writer_close(w, &err);
    }
    cf_lock_sidecar()->give(cf_lock_sidecar(), &lk);
    if (wt != NULL) {
        cf_watcher_close(wt);
    }
    if (c != NULL) {
        cf_config_free(c);
    }
    fclose(held);
}

int main(int argc, char** argv) {
    if (argc >= 3 && strcmp(argv[1], "--fds") == 0) {
        return list_fds(argv[2]) ? 0 : 1;
    }
    test_children(argv[0]);
    return cf_test_done();
}
