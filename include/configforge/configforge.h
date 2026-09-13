/*
 * configforge.h - the public interface.
 *
 * One include. Everything below is reachable from here, and nothing
 * above Tier 0 has to be learned to use Tier 0.
 *
 *     cf_error    err = CF_ERROR_INIT;
 *     cf_config*  cfg;
 *     cf_i64      port;
 *
 *     cfg = cf_app("myapp", argc, argv, &err);
 *     if (cfg == NULL) { fputs(err.message, stderr); return 1; }
 *     port = cf_get_int(cfg, "server.port", 8080);
 *     cf_config_free(cfg);
 */

#ifndef CONFIGFORGE_H
#define CONFIGFORGE_H

#include "configforge/cf_port.h"
#include "configforge/cf_version.h"
#include "configforge/cf_platform.h"
#include "configforge/cf_error.h"
#include "configforge/cf_value.h"
#include "configforge/cf_alloc.h"
#include "configforge/cf_keyspace.h"
#include "configforge/cf_entry.h"
#include "configforge/cf_source.h"
#include "configforge/cf_define.h"
#include "configforge/cf_serialize.h"
#include "configforge/cf_flatten.h"
#include "configforge/cf_image.h"
#include "configforge/cf_explain.h"
#include "configforge/cf_report.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cf_builder cf_builder;
typedef struct cf_config  cf_config;

/* ------------------------------------------------------------------ *
 * Tier 0
 *
 * The conventional sources and the conventional precedence for a named
 * application:
 *
 *     command line  >  environment (NAME_)  >  built-in default
 *
 * File sources join that chain when the serialization layer lands;
 * their rank is already reserved, so adding them will not renumber
 * anything.
 * ------------------------------------------------------------------ */

cf_config* cf_app(const char* name, int argc, char* const* argv, cf_error* err);

/* ------------------------------------------------------------------ *
 * Tier 1 and 2
 *
 * The builder owns an arena; the configuration inherits it. A builder
 * is consumed by cf_builder_create whether that succeeds or fails, so
 * there is exactly one way to release it and no path that leaks.
 * Abandoning one without creating is cf_builder_abort.
 *
 * The per-call error is deferred rather than returned: a definition
 * block reads as a block, and the first failure is reported once by
 * cf_builder_create. Adding a check after every line would be the
 * ceremony this design exists to remove.
 * ------------------------------------------------------------------ */

cf_builder* cf_build(const cf_allocator* alloc);
cf_builder* cf_app_builder(const char* name, int argc, char* const* argv);
void        cf_builder_abort(cf_builder* b);

cf_bool cf_builder_memory(cf_builder* b, const char* name, int rank,
                          const cf_pair* pairs, size_t count);
cf_bool cf_builder_env(cf_builder* b, const char* name, int rank, const char* prefix,
                       char* const* environment);
cf_bool cf_builder_cli(cf_builder* b, const char* name, int rank, int argc,
                       char* const* argv);

/* The format is taken from the extension unless one is named. A file
   whose format this build left out is an error that says which option
   to turn on; CF_FILE_OPTIONAL makes a *missing* file acceptable, and
   never makes an unreadable one acceptable. */
cf_bool cf_builder_file(cf_builder* b, const char* name, int rank, const char* path,
                        const char* format, unsigned flags);

/* A .env file, read with the environment's key mapping: a .env file is
   an environment written down, so the escape and the natural spelling
   behave identically in both. */
cf_bool cf_builder_dotenv(cf_builder* b, const char* name, int rank, const char* path,
                          const char* prefix, unsigned flags);

/* A drop-in directory: every file in byte order, all at one rank, so
   10-base losing to 90-site is precedence rather than a merge rule and
   explain() names the file that won. */
cf_bool cf_builder_dir(cf_builder* b, const char* name, int rank, const char* path,
                       const char* format, unsigned flags);

/* Register a source the caller built itself. The source object is
   copied, so the caller's may be on the stack; any state it points at
   must outlive the configuration. */
cf_bool cf_builder_source(cf_builder* b, const cf_source* s);

cf_bool cf_builder_define(cf_builder* b, const cf_def* d);
cf_bool cf_builder_bool(cf_builder* b, const char* key, cf_bool dflt, unsigned flags);
cf_bool cf_builder_int(cf_builder* b, const char* key, cf_i64 dflt, cf_i64 min,
                       cf_i64 max, unsigned flags);
cf_bool cf_builder_text(cf_builder* b, const char* key, const char* dflt,
                        unsigned flags);
#if CF_WITH_REAL
cf_bool cf_builder_real(cf_builder* b, const char* key, double dflt, double min,
                        double max, unsigned flags);
#endif

void cf_builder_unknown(cf_builder* b, cf_unknown_policy policy);

cf_config* cf_builder_create(cf_builder* b, cf_error* err);

/* ------------------------------------------------------------------ *
 * Reading
 *
 * These do not fail. Everything that can go wrong was detected while
 * the configuration was being built and reported once, there.
 *
 * A read of the wrong type returns the fallback rather than coercing:
 * with a definition present that is a programming error, and coercing
 * would hide it. cf_try_* distinguishes absent from wrong-type for
 * callers who want to know, and cf_type_of answers directly.
 * ------------------------------------------------------------------ */

cf_bool     cf_get_bool(const cf_config* c, const char* key, cf_bool fallback);
cf_i64      cf_get_int(const cf_config* c, const char* key, cf_i64 fallback);
const char* cf_get_text(const cf_config* c, const char* key, const char* fallback);
#if CF_WITH_REAL
double cf_get_real(const cf_config* c, const char* key, double fallback);
#endif

/* Pointer and length, for text that may contain anything. The stored
   value is NUL-terminated as well, so cf_get_text stays free. */
const char* cf_get_text_n(const cf_config* c, const char* key, size_t* len,
                          const char* fallback);

/* CF_OK, CF_E_KEY when absent, CF_E_CONVERT on a type mismatch. */
cf_code cf_try_bool(const cf_config* c, const char* key, cf_bool* out);
cf_code cf_try_int(const cf_config* c, const char* key, cf_i64* out);
cf_code cf_try_text(const cf_config* c, const char* key, const char** out, size_t* len);
#if CF_WITH_REAL
cf_code cf_try_real(const cf_config* c, const char* key, double* out);
#endif

/* CF_TYPE_NULL when the key is absent. */
cf_type cf_type_of(const cf_config* c, const char* key);
cf_bool cf_has(const cf_config* c, const char* key);

/* ------------------------------------------------------------------ *
 * Diagnostics
 *
 * No setup, no flag, no build option. The provenance chain is what
 * get() reads, so printing it costs nothing that was not already paid.
 * ------------------------------------------------------------------ */

size_t cf_explain(const cf_config* c, const char* key, char* buf, size_t cap);
size_t cf_inspect(const cf_config* c, char* buf, size_t cap);

/* Order-independent over the resolved configuration, identical on
   every platform and in both portability modes. */
cf_u64 cf_digest(const cf_config* c);

/* The provenance report: the whole key space with every candidate,
   serialised. Doctor compares two of them; the library can read one
   back with cf_builder_replay. Measure-or-write.

   A full report contains every value and is therefore a
   credential-bearing artifact. The blind form keeps presence, origin,
   length and a digest, which is everything an audit needs and nothing
   a leak would want. */
size_t cf_report(const cf_config* c, const cf_report_info* info, char* buf, size_t cap);

/* Load a full report as a source. Reproduces exactly the configuration
   a process was running, rather than a reconstruction from files that
   may have changed since. */
cf_bool cf_builder_replay(cf_builder* b, const char* name, int rank, const char* path);

size_t                 cf_count(const cf_config* c);
const cf_apply_report* cf_apply_stats(const cf_config* c);
const cf_keyspace*     cf_keys(const cf_config* c);
const cf_source_view*  cf_sources(const cf_config* c);
const cf_defset*       cf_defs(const cf_config* c);

void cf_config_free(cf_config* c);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_H */
