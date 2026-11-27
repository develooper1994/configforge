/*
 * cf_oom.h - an allocator that refuses the Nth allocation, and the sweep
 * that walks N (K-9b, D-237).
 *
 *     #include "cf_test.h"
 *     #include "cf_oom.h"
 *     ...
 *     static int try_thing(void* ctx, const cf_allocator* a, cf_u64* digest,
 *                          cf_error* err) { ... }
 *     ...
 *     (void)cf_oom_sweep("thing", try_thing, &input);
 *
 * One case is one function that does the work through `a` and says how
 * it went: CF_OOM_DONE (it worked; `digest` describes the result),
 * CF_OOM_FAILED (it failed; `err` is set) or CF_OOM_BAD (the case's own
 * check failed, which it has already reported with check()). The sweep
 * runs it once with every allocation allowed, which counts the places
 * memory can run out, and then once for each of those places with that
 * allocation refused and every one after it. Each failure must be
 * CF_E_MEMORY with a message and must leave no block outstanding; the
 * run that reaches success must give the digest of the unlimited run.
 *
 * A case that has to build something before the part under test calls
 * cf_oom_pause(a) first and cf_oom_resume(a) when the part begins: the
 * allocations before are not counted and not refused, the ones after are
 * the sweep's. Every block still has to be released by the end.
 *
 * Static functions in the style of cf_test.h: cf_oom_sweep names the
 * others, so a program that includes the header and sweeps compiles
 * without an unused-function warning. C89 and C++.
 */

#ifndef CONFIGFORGE_TESTS_CF_OOM_H
#define CONFIGFORGE_TESTS_CF_OOM_H

#include <configforge/cf_alloc.h>
#include <configforge/cf_error.h>
#include <configforge/cf_port.h>

#include <stdlib.h>

#include "cf_test.h"

typedef struct cf_oom {
    size_t budget;  /* allocations still allowed before refusing forever */
    size_t calls;   /* how many were asked for since the section began */
    size_t live;    /* blocks outstanding, for leak detection */
    size_t refused; /* how many were refused */
    size_t saved;   /* the budget while cf_oom_pause holds it back */
    int    paused;
} cf_oom;

enum { CF_OOM_DONE = 0, CF_OOM_FAILED = 1, CF_OOM_BAD = 2 };

typedef int (*cf_oom_try)(void* ctx, const cf_allocator* a, cf_u64* digest,
                          cf_error* err);

static void* cf_oom_alloc(void* ctx, size_t n) {
    cf_oom* o = (cf_oom*)ctx;
    void*   p;

    ++o->calls;
    if (o->budget == 0) {
        ++o->refused;
        return NULL;
    }
    if (o->budget != (size_t)-1) {
        --o->budget;
    }
    p = malloc(n);
    if (p != NULL) {
        ++o->live;
    }
    return p;
}

static void cf_oom_release(void* ctx, void* p) {
    cf_oom* o = (cf_oom*)ctx;

    if (p != NULL) {
        --o->live;
        free(p);
    }
}

/* The allocator `a` over `o`, allowing everything. */
static void cf_oom_init(cf_oom* o, cf_allocator* a) {
    o->budget  = (size_t)-1;
    o->calls   = 0;
    o->live    = 0;
    o->refused = 0;
    o->saved   = (size_t)-1;
    o->paused  = 0;
    a->alloc   = cf_oom_alloc;
    a->release = cf_oom_release;
    a->ctx     = o;
}

/* From here to cf_oom_resume nothing is refused and nothing is counted
   toward the budget: what a case builds before the part it tests. */
static void cf_oom_pause(const cf_allocator* a) {
    cf_oom* o = (cf_oom*)a->ctx;

    if (!o->paused) {
        o->saved  = o->budget;
        o->budget = (size_t)-1;
        o->paused = 1;
    }
}

/* The sweep's budget applies again, counted from here. */
static void cf_oom_resume(const cf_allocator* a) {
    cf_oom* o = (cf_oom*)a->ctx;

    if (o->paused) {
        o->budget = o->saved;
        o->paused = 0;
    }
    o->calls   = 0;
    o->refused = 0;
}

/* The allocations the case's own counted section made so far. */
static size_t cf_oom_calls(const cf_allocator* a) {
    return ((const cf_oom*)a->ctx)->calls;
}

/* Walks every allocation of the case in turn. Returns how many there
   are, so that a test can require that a bigger input takes more. */
static size_t cf_oom_sweep(const char* what, cf_oom_try fn, void* ctx) {
    cf_oom       o;
    cf_allocator a;
    cf_u64       reference = 0;
    size_t       calls;
    size_t       budget;
    size_t       first_success = 0;
    size_t       failures      = 0;
    int          found         = 0;
    char         msg[160];

    (void)&cf_oom_pause;
    (void)&cf_oom_resume;
    (void)&cf_oom_calls;

    cf_oom_init(&o, &a);
    {
        cf_error err = CF_ERROR_INIT;
        int      rc  = fn(ctx, &a, &reference, &err);

        cf_fmt(msg, sizeof msg, "%s: works when nothing is refused", what);
        check(rc == CF_OOM_DONE, msg);
        cf_fmt(msg, sizeof msg, "%s: leaves nothing outstanding", what);
        check(o.live == 0, msg);
        calls = o.calls;
        if (rc != CF_OOM_DONE) {
            return 0;
        }
    }

    for (budget = 0; budget <= calls + 1; ++budget) {
        cf_error err    = CF_ERROR_INIT;
        cf_u64   digest = 0;
        int      rc;

        cf_oom_init(&o, &a);
        o.budget = budget;
        rc       = fn(ctx, &a, &digest, &err);

        if (rc == CF_OOM_FAILED) {
            ++failures;
            cf_fmt(msg, sizeof msg,
                   "%s: refusing allocation %u is CF_E_MEMORY, not %s (%s)", what,
                   (unsigned)budget + 1u, cf_code_name((cf_code)err.code), err.message);
            check(err.code == CF_E_MEMORY, msg);
            cf_fmt(msg, sizeof msg, "%s: refusing allocation %u says why", what,
                   (unsigned)budget + 1u);
            check(err.message[0] != '\0', msg);
        } else if (rc == CF_OOM_DONE) {
            if (!found) {
                found         = 1;
                first_success = budget;
            }
            cf_fmt(
                msg, sizeof msg,
                "%s: the result does not depend on how many allocations were allowed "
                "(budget %u)",
                what, (unsigned)budget);
            check(digest == reference, msg);
        } else {
            cf_fmt(msg, sizeof msg, "%s: the case's own check failed at budget %u",
                   what, (unsigned)budget);
            check(0, msg);
        }
        cf_fmt(msg, sizeof msg, "%s: releases every block at budget %u", what,
               (unsigned)budget);
        check(o.live == 0, msg);
    }

    cf_fmt(msg, sizeof msg,
           "%s: success is reached when every counted allocation is allowed", what);
    check(found && first_success == calls, msg);
    cf_fmt(msg, sizeof msg, "%s: a case with allocations has a run that fails", what);
    check(calls == 0 || failures > 0, msg);
    printf("  %s: %u allocations, %u refused runs\n", what, (unsigned)calls,
           (unsigned)failures);
    return calls;
}

#endif /* CONFIGFORGE_TESTS_CF_OOM_H */
