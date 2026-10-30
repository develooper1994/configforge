/*
 * cf_iterate.h - walking a prefix.
 *
 * The questions a program actually asks of a configuration are rarely
 * "what is server.port", which a plain get answers. They are "what are
 * the elements of this array" and "which plugins are configured", and
 * both needed the caller to build keys by hand and guess where to stop.
 *
 * Both are a scan from one binary search, because the key space is
 * sorted. Nothing is allocated: the iterator is the caller's.
 */

#ifndef CONFIGFORGE_CF_ITERATE_H
#define CONFIGFORGE_CF_ITERATE_H

#include "configforge/cf_keyspace.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cf_config;

typedef struct cf_iter {
    const struct cf_config* cfg;
    const char*             prefix;
    size_t                  plen;
    size_t                  pos;
    cf_bool                 children;

    /* The current entry, valid after cf_iter_next returns true. */
    const char*     key; /* the whole key                       */
    size_t          keylen;
    const char*     name; /* the part after the prefix          */
    size_t          namelen;
    const cf_value* value; /* NULL for a child that is a table  */

    /* Room a later 1.x may use without the struct growing: the caller
       allocates a cf_iter and the library fills it, so a larger one
       would be written past the end of a 1.0 caller's (D-51). */
    void* reserved[4];
} cf_iter;

/* Every key under `prefix`, in order. A prefix matches at a segment
   boundary only, so "server" does not match "servername" - the bug
   every hand-written prefix check has. An empty or NULL prefix walks
   the whole configuration. */
CF_API cf_bool cf_iter_keys(cf_iter* it, const struct cf_config* c, const char* prefix);

/* Each distinct immediate child of `prefix`, once:
 *
 *     plugins.auth.enabled     -> auth
 *     plugins.auth.timeout
 *     plugins.cache.size       -> cache
 *
 * This is how a program discovers what is configured rather than being
 * told in advance, which is what plugin and per-tenant configuration
 * both need.
 */
CF_API cf_bool cf_iter_children(cf_iter* it, const struct cf_config* c,
                                const char* prefix);

CF_API cf_bool cf_iter_next(cf_iter* it);

/* ------------------------------------------------------------------ *
 * Arrays
 * ------------------------------------------------------------------ */

/* The recorded length, which is not the same as the number of index
   keys present: a high-precedence source that shortens an array leaves
   the longer one's extra elements in the key space, and counting them
   would report the union of two arrays. */
CF_API size_t cf_array_count(const struct cf_config* c, const char* key);

/* Builds "<key>.<index>", so a loop does not have to. Measure-or-write
   like everything else that renders. */
CF_API size_t cf_array_key(char* buf, size_t cap, const char* key, size_t index);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_ITERATE_H */
