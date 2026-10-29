/*
 * gate.c - Doctor as the check a reload passes through (cf_live).
 *
 * A preflight at startup refuses a production configuration whose
 * password came from the command line; the same mistake made by an
 * edit an hour later should be refused the same way, before a reader
 * sees it. cf_doctor_live_check is a cf_live_check_fn that lints each
 * snapshot cf_live is about to hand out. C89.
 */

#include "configforge/cf_doctor.h"

#include <string.h>

/* Whether a finding at `level` stops the snapshot, under `fail_on`. */
static cf_bool gate_stops(const char* level, const char* fail_on) {
    if (strcmp(level, "error") == 0) {
        return CF_TRUE;
    }
    return strcmp(fail_on, "warning") == 0 && strcmp(level, "warning") == 0;
}

/* `user` stays a plain pointer: this is a cf_live_check_fn, whose type
   cppcheck cannot see from here. */
/* cppcheck-suppress constParameterPointer */
cf_bool cf_doctor_live_check(void* user, const cf_config* running,
                             const cf_config* next, cf_error* err) {
    const cf_doctor_gate* g = (const cf_doctor_gate*)user;
    const char* fail_on     = (g != NULL && g->fail_on != NULL) ? g->fail_on : "error";
    cf_doctor*  d;
    size_t      i;
    cf_bool     ok = CF_TRUE;

    (void)running;
    if (strcmp(fail_on, "error") != 0 && strcmp(fail_on, "warning") != 0) {
        return cf_error_set(err, CF_E_POLICY, NULL, NULL,
                            "the gate fails on \"%s\"; it fails on error or warning",
                            fail_on);
    }
    d = cf_doctor_new();
    if (d == NULL) {
        return cf_error_set(err, CF_E_MEMORY, NULL, NULL, "no room for Doctor");
    }
    if ((g != NULL && g->baseline != NULL &&
         !cf_doctor_baseline(d, g->baseline, err)) ||
        (g != NULL && g->severity != NULL &&
         !cf_doctor_severity_file(d, g->severity, err)) ||
        !cf_doctor_lint(d, next, (g != NULL) ? g->profile : NULL, err)) {
        cf_doctor_free(d);
        return CF_FALSE;
    }
    /* The first finding that stops it is the reason given; the rest are
       the same edit's, and the operator rereads it anyway. */
    for (i = 0; ok && i < cf_doctor_count(d); ++i) {
        const cf_finding* f = cf_doctor_finding(d, i);
        if (gate_stops(f->level, fail_on)) {
            const char* key = (f->key != NULL) ? f->key : "-";
            ok = cf_error_set(err, CF_E_POLICY, f->key, f->at, "%s %s %s: %s", f->rule,
                              f->level, key, f->message);
        }
    }
    cf_doctor_free(d);
    return ok;
}
