/*
 * test_fork.c - after fork, the child reads a cf_config built before it
 * (K-9i, docs/Guarantees.md).
 *
 * The parent builds a config, forks, and the child reads every key and
 * exits 0. The library does not detect a child (no pid check, so no
 * system call on every cf_live_* call): cf_live_* in a child is outside
 * the contract, and this test holds the reading half only.
 * POSIX only: elsewhere, and on none, it skips.
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200112L /* fork, waitpid */
#endif

#include <configforge/cf_platform.h>
#include <configforge/configforge.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "cf_test.h"

#ifndef _WIN32

static const cf_pair g_pairs[] = {{"server.port", "9443"},
                                  {"server.host", "web1"},
                                  {"tls.enabled", "true"},
                                  {"log.level", "debug"}};

static cf_builder* build(void* user) {
    cf_builder* b = cf_build(NULL);
    (void)user;
    cf_builder_memory(b, "mem", CF_RANK_USER_FILE, g_pairs, 4);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "localhost", 0);
    cf_builder_bool(b, "tls.enabled", CF_FALSE, 0);
    cf_builder_text(b, "log.level", "info", 0);
    return b;
}

/* The child: every key reads what the parent built. */
static int child_reads(const cf_config* c) {
    int bad = 0;

    bad += cf_get_int(c, "server.port", 0) != 9443;
    bad += strcmp(cf_get_text(c, "server.host", ""), "web1") != 0;
    bad += cf_get_bool(c, "tls.enabled", CF_FALSE) != CF_TRUE;
    bad += strcmp(cf_get_text(c, "log.level", ""), "debug") != 0;
    return bad;
}

static void test_child(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = cf_builder_create(build(NULL), &err);
    pid_t      pid;
    int        status = -1;

    check(c != NULL, "the parent builds");
    if (c == NULL) {
        return;
    }
    (void)fflush(stdout);
    pid = fork();
    if (pid == 0) {
        _exit(child_reads(c) == 0 ? 0 : 2);
    }
    check(pid > 0, "fork");
    if (pid > 0) {
        check(waitpid(pid, &status, 0) == pid, "wait");
        check(WIFEXITED(status) && WEXITSTATUS(status) == 0,
              "the child reads every key as the parent built it");
    }
    cf_config_free(c);
}

#endif

int main(void) {
#ifdef _WIN32
    puts("skipped: no fork on this platform");
#else
    if (strcmp(cf_platform_name(), "posix") != 0) {
        puts("skipped: this platform has no fork");
        return 0;
    }
    test_child();
#endif
    return cf_test_done();
}
