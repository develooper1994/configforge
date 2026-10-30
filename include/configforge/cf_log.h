/*
 * cf_log.h - what the library has to say that is not an error.
 *
 * An error stops a configuration and comes back in a cf_error. Some
 * things deserve a line in the program's log without stopping anything:
 * a key nobody defined, under CF_UNKNOWN_WARN; a deprecated key still in
 * use; a value the contract refused and fell back from; each reload, and
 * the keys waiting for a restart. Each is an event with a level, a
 * stable name a log pipeline can match on, and the key and source it is
 * about, handed to a function the program gives:
 *
 *     static void to_syslog(void* user, const cf_log_event* e) {
 *         syslog(e->level >= CF_LOG_WARN ? LOG_WARNING : LOG_INFO,
 *                "configforge %s %s: %s", e->event, e->key ? e->key : "-",
 *                e->message);
 *     }
 *     cf_builder_log(b, to_syslog, NULL, CF_LOG_INFO);
 *
 * The library never writes to stderr or anywhere else by itself. An
 * event's strings belong to the library and are valid during the call.
 * The function is called on the thread that creates the configuration,
 * or that drives a cf_live's reloads.
 */

#ifndef CONFIGFORGE_CF_LOG_H
#define CONFIGFORGE_CF_LOG_H

#include "configforge/cf_port.h"
#include "configforge/cf_error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum cf_log_level {
    CF_LOG_DEBUG = 10,
    CF_LOG_INFO  = 20,
    CF_LOG_WARN  = 30,
    CF_LOG_ERROR = 40
} cf_log_level;

/* One event. `size` is sizeof(cf_log_event) in the library that made it:
   a later 1.x adds a member after `message`, and a program reads it only
   where `size` reaches it (D-51). */
typedef struct cf_log_event {
    size_t       size;
    cf_log_level level;
    const char*  event;  /* stable: "unknown-key", "deprecated-key",
                            "fallback", "reload", "reload-pending",
                            "reload-failed", "restart-pending"        */
    cf_code     code;    /* CF_OK, or the error a failure carried     */
    const char* key;     /* the key it is about, or NULL              */
    const char* source;  /* the source, or where the value was set    */
    const char* message; /* one sentence for a person                 */
} cf_log_event;

typedef void (*cf_log_fn)(void* user, const cf_log_event* e);

/* The name of a level: "debug", "info", "warn", "error". */
CF_API const char* cf_log_level_name(cf_log_level level);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_LOG_H */
