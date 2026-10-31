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
#include "configforge/cf_emit.h"
#include "configforge/cf_image.h"
#include "configforge/cf_explain.h"
#include "configforge/cf_iterate.h"
#include "configforge/cf_report.h"
#include "configforge/cf_live.h"
#include "configforge/cf_log.h"

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

CF_NODISCARD CF_API cf_config* cf_app(const char* name, int argc, char* const* argv,
                                      cf_error* err);

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

CF_API cf_builder* cf_build(const cf_allocator* alloc);

/* Build inside a buffer the caller owns.
 *
 * This is the freestanding path, and it was missing: cf_arena_fixed
 * existed and nothing in the builder accepted an arena, so the claim
 * that a target with no heap can hand over a buffer had no way to be
 * true. Found by writing the test for it.
 *
 * The arena is borrowed, not owned: cf_config_free releases the
 * configuration's hold on it and never frees the caller's buffer.
 * Everything the configuration points at lives in that buffer, so it
 * has to outlive the configuration - which on the target this exists
 * for is a static array, and outlives everything. */
CF_API cf_builder* cf_build_in(cf_arena* arena);
CF_API cf_builder* cf_app_builder(const char* name, int argc, char* const* argv);
CF_API void        cf_builder_abort(cf_builder* b);

CF_API cf_bool cf_builder_memory(cf_builder* b, const char* name, int rank,
                                 const cf_pair* pairs, size_t count);
CF_API cf_bool cf_builder_env(cf_builder* b, const char* name, int rank,
                              const char* prefix, char* const* environment);
CF_API cf_bool cf_builder_cli(cf_builder* b, const char* name, int rank, int argc,
                              char* const* argv);

/* The format is taken from the extension unless one is named. A file
   whose format this build left out is an error that says which option
   to turn on; CF_OPTIONAL makes a *missing* file acceptable, and
   never makes an unreadable one acceptable. */
CF_API cf_bool cf_builder_file(cf_builder* b, const char* name, int rank,
                               const char* path, const char* format, unsigned flags);

/* A file read by a codec the caller supplies, for a format this library
   does not have; see cf_source_file_codec. The codec must outlive the
   configuration. */
struct cf_codec;
CF_API cf_bool cf_builder_file_codec(cf_builder* b, const char* name, int rank,
                                     const char* path, const struct cf_codec* codec,
                                     unsigned flags);

/* A .env file, read with the environment's key mapping: a .env file is
   an environment written down, so the escape and the natural spelling
   behave identically in both. */
CF_API cf_bool cf_builder_dotenv(cf_builder* b, const char* name, int rank,
                                 const char* path, const char* prefix, unsigned flags);

/* A drop-in directory: every file in byte order, all at one rank, so
   10-base losing to 90-site is precedence rather than a merge rule and
   explain() names the file that won. */
CF_API cf_bool cf_builder_dir(cf_builder* b, const char* name, int rank,
                              const char* path, const char* format, unsigned flags);

/* A directory of secrets, one file a key, as containers mount them and
   systemd passes credentials (cf_source_secrets says how names map). */
CF_API cf_bool cf_builder_secrets(cf_builder* b, const char* name, int rank,
                                  const char* path, const char* prefix, unsigned flags);

/* A key of the Windows Registry and its subkeys (cf_source_registry
   says how names and types map). Windows only; elsewhere it fails by
   name, CF_OPTIONAL or not. */
CF_API cf_bool cf_builder_registry(cf_builder* b, const char* name, int rank,
                                   const char* root, unsigned flags);

/* A SQLite store (CF_WITH_SQLITE): typed values, each with what its
   writer said about it. cf_write writes one, when it is granted. */
CF_API cf_bool cf_builder_store(cf_builder* b, const char* name, int rank,
                                const char* path, unsigned flags);

/* Allow cf_write to write the source of this name - the program's
   permission, as against the source's capability. Every source is read
   only until granted. A grant to a source that cannot be written, or to
   no source of that name, fails cf_builder_create, before anything is
   read. A derived configuration keeps the grants of the one it came
   from. */
CF_API cf_bool cf_builder_writable(cf_builder* b, const char* source_name);

/* Register a source the caller built itself. The source object is
   copied, so the caller's may be on the stack; any state it points at
   must outlive the configuration. */
CF_API cf_bool cf_builder_source(cf_builder* b, const cf_source* s);

CF_API cf_bool cf_builder_define(cf_builder* b, const cf_def* d);

/* Exempt a prefix from unknown-key rejection, for a key space the
   program cannot know in advance: a plugin's settings are not in the
   contract because the plugin is not either. */
CF_API cf_bool cf_builder_open(cf_builder* b, const char* prefix);
CF_API cf_bool cf_builder_bool(cf_builder* b, const char* key, cf_bool dflt,
                               unsigned flags);
CF_API cf_bool cf_builder_int(cf_builder* b, const char* key, cf_i64 dflt, cf_i64 min,
                              cf_i64 max, unsigned flags);
CF_API cf_bool cf_builder_text(cf_builder* b, const char* key, const char* dflt,
                               unsigned flags);
#if CF_WITH_REAL
CF_API cf_bool cf_builder_real(cf_builder* b, const char* key, double dflt, double min,
                               double max, unsigned flags);
#endif

CF_API void cf_builder_unknown(cf_builder* b, cf_unknown_policy policy);

/* Where what deserves a log line and is not an error goes (cf_log.h):
   unknown keys under CF_UNKNOWN_WARN, deprecated keys in use, values
   refused and fallen back from. Events below `min` are not made. A
   configuration derived from this one logs to the same place. */
CF_API void cf_builder_log(cf_builder* b, cf_log_fn fn, void* user, cf_log_level min);

CF_NODISCARD CF_API cf_config* cf_builder_create(cf_builder* b, cf_error* err);

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

CF_API cf_bool     cf_get_bool(const cf_config* c, const char* key, cf_bool fallback);
CF_API cf_i64      cf_get_int(const cf_config* c, const char* key, cf_i64 fallback);
CF_API const char* cf_get_text(const cf_config* c, const char* key,
                               const char* fallback);
#if CF_WITH_REAL
CF_API double cf_get_real(const cf_config* c, const char* key, double fallback);
#endif

/* Pointer and length, for text that may contain anything. The stored
   value is NUL-terminated as well, so cf_get_text stays free. */
CF_API const char* cf_get_text_n(const cf_config* c, const char* key, size_t* len,
                                 const char* fallback);

/* CF_OK, CF_E_KEY when absent, CF_E_CONVERT on a type mismatch. */
CF_NODISCARD CF_API cf_code cf_try_bool(const cf_config* c, const char* key,
                                        cf_bool* out);
CF_NODISCARD CF_API cf_code cf_try_int(const cf_config* c, const char* key,
                                       cf_i64* out);
CF_NODISCARD CF_API cf_code cf_try_text(const cf_config* c, const char* key,
                                        const char** out, size_t* len);
#if CF_WITH_REAL
CF_NODISCARD CF_API cf_code cf_try_real(const cf_config* c, const char* key,
                                        double* out);
#endif

/* C11: one cf_get, typed by where the value goes (D-53).

       int  port;   if (cf_get(cfg, "server.port", &port) != CF_OK) ...
       bool tls;    cf_get(cfg, "tls.enabled", &tls);
       const char* host;  cf_get(cfg, "server.host", &host);

   int, long, long long, bool, double and const char*. CF_OK, CF_E_KEY
   when absent, CF_E_CONVERT for another type or for an integer the
   target cannot hold - never a truncated one. In C89 the cf_try_
   functions above say the same. */
#if CF_HAVE_C11 && !defined(__cplusplus)
/* An integer the target cannot hold does not survive the round trip:
   implementation-defined, never undefined, and no <limits.h> in a
   public header. */
CF_INLINE cf_code cf_get_int_(const cf_config* c, const char* k, int* out) {
    cf_i64  v;
    cf_code r = cf_try_int(c, k, &v);
    if (r == CF_OK && (cf_i64)(int)v != v) {
        return CF_E_CONVERT;
    }
    if (r == CF_OK) {
        *out = (int)v;
    }
    return r;
}
CF_INLINE cf_code cf_get_long_(const cf_config* c, const char* k, long* out) {
    cf_i64  v;
    cf_code r = cf_try_int(c, k, &v);
    if (r == CF_OK && (cf_i64)(long)v != v) {
        return CF_E_CONVERT;
    }
    if (r == CF_OK) {
        *out = (long)v;
    }
    return r;
}
CF_INLINE cf_code cf_get_llong_(const cf_config* c, const char* k, long long* out) {
    cf_i64  v;
    cf_code r = cf_try_int(c, k, &v);
    if (r == CF_OK) {
        *out = (long long)v;
    }
    return r;
}
CF_INLINE cf_code cf_get_bool_(const cf_config* c, const char* k, _Bool* out) {
    cf_bool v;
    cf_code r = cf_try_bool(c, k, &v);
    if (r == CF_OK) {
        *out = (v != 0);
    }
    return r;
}
CF_INLINE cf_code cf_get_text_(const cf_config* c, const char* k, const char** out) {
    size_t n;
    return cf_try_text(c, k, out, &n);
}
#if CF_WITH_REAL
#define CF_GET_REAL_ , double* : cf_try_real
#else
#define CF_GET_REAL_
#endif
#define cf_get(c, key, out)                                                            \
    _Generic((out),                                                                    \
        int*: cf_get_int_,                                                             \
        long*: cf_get_long_,                                                           \
        long long*: cf_get_llong_,                                                     \
        _Bool*: cf_get_bool_,                                                          \
        const char**: cf_get_text_ CF_GET_REAL_)((c), (key), (out))
#endif

/* CF_TYPE_NULL when the key is absent. */
CF_API cf_type cf_type_of(const cf_config* c, const char* key);
CF_API cf_bool cf_has(const cf_config* c, const char* key);

/* ------------------------------------------------------------------ *
 * Diagnostics
 *
 * No setup, no flag, no build option. The provenance chain is what
 * get() reads, so printing it costs nothing that was not already paid.
 *
 * A key defined CF_SENSITIVE has its chain shown in full and its values
 * shown as "<sensitive, N bytes>": which source won and where it was
 * set is the question, and it is not the secret.
 * ------------------------------------------------------------------ */

CF_API size_t cf_explain(const cf_config* c, const char* key, char* buf, size_t cap);

/* The same chain as JSON, for an administration endpoint or a tool:
   cf_report_key_write says what it holds. CF_REPORT_BLIND is what an
   endpoint wants - presence, origin, length and digest for a sensitive
   key, never its value. */
CF_API size_t cf_explain_json(const cf_config* c, const char* key, cf_report_mode mode,
                              char* buf, size_t cap);
CF_API size_t cf_inspect(const cf_config* c, char* buf, size_t cap);

/* What changed between two configurations: after a reload, the line a
   log should carry instead of two dumps side by side. One pass over two
   sorted key spaces.

   A value that is the same but now comes from somewhere else is a
   change of its own kind: the configuration means the same thing, its
   provenance does not, and the source it now comes from is the one an
   edit would have to touch. */
typedef enum cf_change {
    CF_CHANGE_ADDED,   /* a value now, none before                */
    CF_CHANGE_REMOVED, /* a value before, none now                */
    CF_CHANGE_VALUE,   /* a different value                       */
    CF_CHANGE_ORIGIN   /* the same value, from a different place  */
} cf_change;

/* Called once per changed key, in key order. `before` is NULL for an
   added key and `after` for a removed one; each is the winning
   candidate, with its value, source rank and location. Return CF_FALSE
   to stop. The values are not redacted: a caller who prints them
   decides, and cf_diff_text below does. */
typedef cf_bool (*cf_diff_fn)(void* ctx, const char* key, size_t klen, cf_change kind,
                              const cf_candidate* before, const cf_candidate* after);

/* The number of changed keys; `fn` may be NULL to only count them. */
CF_API size_t cf_diff(const cf_config* before, const cf_config* after, cf_diff_fn fn,
                      void* ctx);

/* One line per change, with where each new value came from, and the
   values of CF_SENSITIVE keys redacted as explain() redacts them:

     server.port: 8080 -> 9443 (file app.ini:2:8)
     log.level: added, "debug" (cli)
     cache.size: 64, now from file app.ini:7:14, was from default
     db.password: changed, <sensitive> (env)

   Measure-or-write, like every renderer here. */
CF_API size_t cf_diff_text(const cf_config* before, const cf_config* after, char* buf,
                           size_t cap);

/* Order-independent over the resolved configuration, identical on
   every platform and in both portability modes. */
CF_API cf_u64 cf_digest(const cf_config* c);

/* The provenance report: the whole key space with every candidate,
   serialised. Doctor compares two of them; the library can read one
   back with cf_builder_replay. Measure-or-write.

   A full report contains every value and is therefore a
   credential-bearing artifact. The blind form keeps presence, origin,
   length and a digest, which is everything an audit needs and nothing
   a leak would want. */
CF_API size_t cf_report(const cf_config* c, const cf_report_info* info, char* buf,
                        size_t cap);

/* The configuration's contract as a JSON Schema, closed when it rejects
   unknown keys (CF_UNKNOWN_REJECT). See cf_schema_write. */
CF_API size_t cf_schema(const cf_config* c, const char* title, char* buf, size_t cap);

/* Load a full report as a source. Reproduces exactly the configuration
   a process was running, rather than a reconstruction from files that
   may have changed since. `flags` is a source's: CF_OPTIONAL, or 0. */
CF_API cf_bool cf_builder_replay(cf_builder* b, const char* name, int rank,
                                 const char* path, unsigned flags);

CF_API size_t                 cf_count(const cf_config* c);
CF_API const cf_apply_report* cf_apply_stats(const cf_config* c);
CF_API const cf_keyspace*     cf_keys(const cf_config* c);
CF_API const cf_source_view*  cf_sources(const cf_config* c);
CF_API const cf_defset*       cf_defs(const cf_config* c);

/* ------------------------------------------------------------------ *
 * Ownership and threads
 *
 * The contract, decision D-14:
 *
 *   Reads are safe concurrently. Building and releasing are the
 *   caller's to synchronise.
 *
 * Concurrent reads are the entire point of an immutable snapshot;
 * locking the read path would contradict the performance model and buy
 * nothing, because there is nothing to protect. An unstated threading
 * contract is assumed to be full internal locking by users and nothing
 * at all by implementers, which is why this is written down rather than
 * left to be inferred.
 *
 * A snapshot is reference counted, so a reader can keep using one while
 * another is built. The count is atomic; everything it protects is
 * immutable.
 * ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ *
 * Changing a value
 *
 * A snapshot is immutable, so setting one means deriving another. The
 * original stays valid and unchanged, which is the point: a reader
 * holding it keeps seeing a configuration that does not move underneath
 * it, and a thread reading while another sets needs no lock because
 * nothing is being written.
 *
 * The derived configuration is independent - it copies what it needs -
 * so neither has to outlive the other. That costs a pass over the key
 * space, which is microseconds for a configuration and buys the caller
 * one less lifetime to reason about.
 *
 * Provenance survives. The new value arrives from a source of its own,
 * above the command line because the user just did it deliberately, and
 * the value it displaced is still in the chain. explain() afterwards
 * says "set at run time, and before that it came from the file", which
 * is the question people actually ask after changing a setting.
 * ------------------------------------------------------------------ */

CF_NODISCARD CF_API cf_config* cf_derive(const cf_config* base, const char* source_name,
                                         const cf_pair* pairs, size_t count,
                                         cf_error* err);

/* The same, inside an arena the caller owns. A configuration built with
   cf_build_in has no allocator to borrow, so it must use this one. */
CF_API cf_config* cf_derive_in(const cf_config* base, cf_arena* arena,
                               const char* source_name, const cf_pair* pairs,
                               size_t count, cf_error* err);

/* ------------------------------------------------------------------ *
 * Keeping a change
 *
 * Deriving changes a snapshot; nothing outlives the process. To keep a
 * change, write it to a source - which asks three questions first, and
 * any "no" refuses every item before one is written:
 *
 *   capability     can that source be written at all? Only a store can
 *                  (Design.md, section 39).
 *   permission     did the program grant it (cf_builder_writable)?
 *   authorization  would the contract accept each value from that
 *                  source? The read side's own rules decide - type,
 *                  range, allowed spellings, unknown keys, and a key's
 *                  policy (D-33): a key that may only be set from the
 *                  environment is not written to a store, because
 *                  writing past an operator's lock would lift it. A
 *                  command (CF_TRANSIENT) is never written.
 *
 * `meta` - who, when, why - goes with each value, and is what explain()
 * shows as where it was set once the store is read again. The
 * configuration does not change: it is a snapshot. Rebuild, or derive,
 * to see the write.
 * ------------------------------------------------------------------ */

CF_NODISCARD CF_API cf_bool cf_write(const cf_config* c, const char* target,
                                     const cf_write_item* items, size_t count,
                                     const char* meta, cf_error* err);

/* ------------------------------------------------------------------ *
 * Change notification
 *
 * Pollable, never a callback: a library that owns a thread and calls
 * you back has made a threading decision that belongs to you.
 *
 *     if (cf_changed(cfg)) {
 *         cf_config* next = rebuild();
 *         cf_config_free(cfg);
 *         cfg = next;
 *     }
 *
 * The old snapshot stays valid while anything holds it, so reloading
 * changes no contract: cf_get_text's pointer is valid until *that*
 * configuration is released, watched or not.
 *
 * What is compared is modification time and size. That is cheap,
 * portable and pollable, and it misses a file rewritten inside one
 * clock tick at the same length. Treat a true as worth acting on, not
 * as a guarantee; the exact answer is to rebuild and compare digests,
 * which costs the re-read you were about to do anyway.
 * ------------------------------------------------------------------ */

CF_API cf_bool     cf_changed(const cf_config* c);
CF_API size_t      cf_watch_count(const cf_config* c);
CF_API const char* cf_watch_path(const cf_config* c, size_t i);

/* ------------------------------------------------------------------ *
 * A handle an event loop can wait on
 *
 * For a program that already has a loop and would rather wait than
 * poll. Still not a callback and still no thread: the library hands
 * over something to wait on, and the waiting is the caller's.
 *
 *     cf_watcher* w = cf_watcher_open(cfg, NULL, &err);
 *
 *     add cf_watcher_fd(w) (POSIX) or cf_watcher_handle(w) (Windows)
 *     to the loop, and when it is ready:
 *
 *     cf_watcher_drain(w);
 *     if (cf_changed(cfg)) {
 *         rebuild and swap, then close w and open one on the new
 *         configuration, whose files may not be the same ones
 *     }
 *
 * With no loop at all, cf_watcher_wait(w, timeout_ms) waits and drains
 * in one call, on whichever thread the caller gives it.
 *
 * A wake-up means "ask now", never "it changed". What is watched is
 * each file's directory - editors save by renaming a new file over the
 * old one - so a neighbour can wake it, and cf_changed is the answer.
 * Drain before asking: the other order can consume a wake-up for a
 * change that arrived after the question.
 *
 * The watcher copies what it needs, so the configuration it was opened
 * on may be released first. It is the caller's to use from one thread
 * at a time; the configuration's own threading contract is unchanged.
 *
 * Linux (inotify), macOS and the BSDs (kqueue), and Windows. Elsewhere
 * cf_watcher_open fails with CF_E_CAPABILITY and says so, and
 * cf_changed works as before.
 * ------------------------------------------------------------------ */

typedef struct cf_platform_wake cf_watcher;

/* `a` NULL means the default allocator. Fails when the configuration
   read no file, since there would be nothing to wake it. */
CF_API cf_watcher* cf_watcher_open(const cf_config* c, const cf_allocator* a,
                                   cf_error* err);

CF_API int   cf_watcher_fd(const cf_watcher* w); /* -1 where it is not a descriptor */
CF_API void* cf_watcher_handle(const cf_watcher* w); /* NULL where it is not a HANDLE */
CF_API cf_bool cf_watcher_drain(cf_watcher* w);

/* 1 woken (and drained), 0 timed out, -1 failed. Negative: no limit. */
CF_API int  cf_watcher_wait(cf_watcher* w, long timeout_ms);
CF_API void cf_watcher_close(cf_watcher* w);

CF_API void cf_config_ref(cf_config* c);
CF_API void cf_config_free(cf_config* c);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_H */
