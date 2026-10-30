/*
 * cf_doctor.h - ConfigForge Doctor's rules, as a library.
 *
 * The `doctor` program is this library with a command line around it.
 * A program can run the same rules on itself - at startup, before it
 * serves, a service can refuse a production configuration whose
 * password came from the command line - and a test can assert that a
 * configuration has no findings, without starting a process or writing
 * a report to disk.
 *
 *     cf_doctor* d = cf_doctor_new();
 *     cf_doctor_lint(d, cfg, "production", &err);
 *     for (i = 0; i < cf_doctor_count(d); ++i) {
 *         const cf_finding* f = cf_doctor_finding(d, i);
 *         fprintf(stderr, "%s %s %s: %s\n", f->rule, f->level, f->key, f->message);
 *     }
 *     cf_doctor_free(d);
 *
 * Doctor reads provenance reports and never resolves configuration
 * itself (Doctor.md): linting a cf_config writes its report in memory
 * and reads that, so the rules see exactly what `doctor lint` sees.
 * Findings accumulate across calls on one context until it is freed;
 * a context is used by one thread at a time.
 *
 * A separate library, libconfigforge_doctor, static, and not part of
 * the configuration library: a device that reads configuration does not
 * carry the rules that audit it.
 */

#ifndef CONFIGFORGE_CF_DOCTOR_H
#define CONFIGFORGE_CF_DOCTOR_H

#include "configforge/configforge.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Declarations of a static library: plain extern, never the
   configuration library's CF_API, which in a Windows DLL build would
   import them from a DLL they are not in. */
#define CF_DOCTOR_API extern

typedef struct cf_doctor cf_doctor;

/* One finding. It and its strings belong to the context, each finding
   its own, and stay valid until the next call that adds findings or
   frees the context. `at` is where the value was set, "app.ini:14:3",
   or NULL when the report does not say. */
typedef struct cf_finding {
    const char* rule;  /* "CFD010"                          */
    const char* level; /* "error", "warning" or "info"       */
    const char* key;
    const char* at;
    const char* message;
} cf_finding;

/* A context, or NULL when there is no memory for one. */
CF_DOCTOR_API cf_doctor* cf_doctor_new(void);
CF_DOCTOR_API void       cf_doctor_free(cf_doctor* d);

/* A finding already accepted - "CFD001", "server.port" - so it is
   counted and not reported. cf_doctor_baseline reads them from a file,
   one per line, as `doctor --baseline` does; a missing file is an empty
   baseline. */
CF_DOCTOR_API cf_bool cf_doctor_accept(cf_doctor* d, const char* rule, const char* key);
CF_DOCTOR_API cf_bool cf_doctor_baseline(cf_doctor* d, const char* path, cf_error* err);

/* Where a project disagrees with a rule's default: `level` is "error",
   "warning", "info" or "off", for every profile or only `profile`.
   cf_doctor_severity_file reads a `doctor --severity` file. Neither
   takes effect for findings already recorded. */
CF_DOCTOR_API cf_bool cf_doctor_severity(cf_doctor* d, const char* rule,
                                         const char* level, const char* profile,
                                         cf_error* err);
CF_DOCTOR_API cf_bool cf_doctor_severity_file(cf_doctor* d, const char* path,
                                              cf_error* err);

/* Lint one configuration. A cf_config is reported in memory, with every
   value, so the type and range rules can run; `profile` is the one the
   report states, which some severities depend on, and may be NULL. A
   report - text, or a file - is linted as `doctor lint` lints it, with
   the profile it carries. False with `err` set when the input cannot be
   read; true otherwise, whatever was found. */
CF_DOCTOR_API cf_bool cf_doctor_lint(cf_doctor* d, const cf_config* c,
                                     const char* profile, cf_error* err);
CF_DOCTOR_API cf_bool cf_doctor_lint_report(cf_doctor* d, const char* json, size_t n,
                                            const char* name, cf_error* err);
CF_DOCTOR_API cf_bool cf_doctor_lint_file(cf_doctor* d, const char* path,
                                          cf_error* err);

/* Compare two: what one has and the other lacks, values from sources
   of different kinds, overrides in one and not the other, and every
   value that differs - `doctor diff`. */
CF_DOCTOR_API cf_bool cf_doctor_diff(cf_doctor* d, const cf_config* a,
                                     const cf_config* b, cf_error* err);
CF_DOCTOR_API cf_bool cf_doctor_diff_report(cf_doctor* d, const char* a, size_t an,
                                            const char* aname, const char* b, size_t bn,
                                            const char* bname, cf_error* err);
CF_DOCTOR_API cf_bool cf_doctor_diff_files(cf_doctor* d, const char* a, const char* b,
                                           cf_error* err);

/* The findings so far, in the order they were found. */
CF_DOCTOR_API size_t            cf_doctor_count(const cf_doctor* d);
CF_DOCTOR_API const cf_finding* cf_doctor_finding(cf_doctor* d, size_t i);

/* How many at a level - "error", "warning", "info" - or "accepted", for
   the ones the baseline accepted. */
CF_DOCTOR_API size_t cf_doctor_count_at(const cf_doctor* d, const char* level);

/* Baseline entries that matched nothing: a fix worth noticing, or a
   rule that quietly stopped working. */
CF_DOCTOR_API size_t  cf_doctor_stale_count(const cf_doctor* d);
CF_DOCTOR_API cf_bool cf_doctor_stale(const cf_doctor* d, size_t i, const char** rule,
                                      const char** key);

/* ------------------------------------------------------------------ *
 * Doctor as the gate a reload passes through
 *
 * The preflight a service runs at startup, run again on every snapshot
 * cf_live is about to hand out, so an edit an hour later is refused as
 * the same mistake would have been at startup - before a reader sees
 * it, with the running configuration left serving:
 *
 *     static cf_doctor_gate gate = {"production", "error", NULL, NULL};
 *     cf_live_options       o    = CF_LIVE_OPTIONS_INIT;
 *     o.mode       = CF_LIVE_AUTO;
 *     o.build      = build;      o.build_user = &app;
 *     o.check      = cf_doctor_live_check;
 *     o.check_user = &gate;
 *     cf_live* live = cf_live_open_with(&o, &err);
 * ------------------------------------------------------------------ */

typedef struct cf_doctor_gate {
    const char* profile;  /* linted as; NULL for none                 */
    const char* fail_on;  /* "error" (NULL means it) or "warning"      */
    const char* baseline; /* a baseline file, as --baseline; or NULL  */
    const char* severity; /* a severity file, as --severity; or NULL  */
} cf_doctor_gate;

/* A cf_live_check_fn: `user` is a cf_doctor_gate, or NULL for the
   defaults. Refuses `next` when a finding reaches `fail_on`, the first
   such finding in `err` - "CFD007 error db.password: ..." with where it
   was set as the error's source. */
CF_DOCTOR_API cf_bool cf_doctor_live_check(void* user, const cf_config* running,
                                           const cf_config* next, cf_error* err);

/* The rule catalogue (Doctor.md): each rule's id and what it finds. */
CF_DOCTOR_API size_t      cf_doctor_rule_count(void);
CF_DOCTOR_API const char* cf_doctor_rule_id(size_t i);
CF_DOCTOR_API const char* cf_doctor_rule_text(const char* id);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_DOCTOR_H */
