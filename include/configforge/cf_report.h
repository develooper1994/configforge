/*
 * cf_report.h - the provenance report.
 *
 * The artifact D-16 specifies. It is the interface between the library
 * and Doctor, between ConfigForge and any foreign-ecosystem adapter,
 * and - through Report Replay - a source format the library itself has
 * to read. Changing it later breaks all three, which is why the schema
 * is versioned from v1 and why the blind and full forms are
 * distinguishable at the top rather than by inspection.
 */

#ifndef CONFIGFORGE_CF_REPORT_H
#define CONFIGFORGE_CF_REPORT_H

#include "configforge/cf_flatten.h"
#include "configforge/cf_define.h"
#include "configforge/cf_source.h"
#include "configforge/cf_explain.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CF_REPORT_SCHEMA 1

typedef enum cf_report_mode {
    /* Values omitted for sensitive keys, replaced by presence, length
       and a digest. Every Doctor rule but the type and range checks
       works on this, so an audit never needs the secrets. */
    CF_REPORT_BLIND = 0,
    /* Every value present. Needed for Report Replay, and therefore a
       credential-bearing artifact: produced deliberately, used
       immediately, deleted. */
    CF_REPORT_FULL
} cf_report_mode;

typedef struct cf_report_info {
    cf_report_mode mode;
    const char*    profile; /* "production"; may be NULL          */
    const char*    application;
    const char*    app_version;
    const char*    generated; /* ISO 8601; the caller supplies it   */
} cf_report_info;

/* Render the report. Measure-or-write: with a NULL buffer nothing is
   stored and the length still accumulates. */
CF_API size_t cf_report_write(const cf_keyspace* ks, const cf_defset* ds,
                              const cf_source_view* sv, const cf_report_info* info,
                              char* buf, size_t cap);

/* ------------------------------------------------------------------ *
 * Report Replay
 *
 * A full report loaded as a source reproduces, on a developer's
 * machine, exactly the configuration a production process was running.
 * Not an approximation, and not a reconstruction from files that may
 * have changed since.
 *
 * A blind report cannot be replayed, because it has no values. That is
 * refused rather than half-applied.
 * ------------------------------------------------------------------ */

CF_API size_t cf_source_replay_state_size(void);

CF_API cf_bool cf_source_replay(cf_source* s, const char* name, int rank,
                                const char* path, void* state, size_t state_size);

CF_API const cf_error* cf_source_replay_error(const cf_source* s);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_REPORT_H */
