/*
 * cf_explain.h - the feature the library exists for.
 *
 * explain() is not a debugging extra bolted onto a resolver. It is the
 * provenance chain printed, and the chain is what get() reads. That
 * ordering is the reason this works at all: a resolver that selected a
 * winner and discarded the rest could not explain itself afterwards,
 * and retrofitting the chain would mean rewriting the resolver.
 */

#ifndef CONFIGFORGE_CF_EXPLAIN_H
#define CONFIGFORGE_CF_EXPLAIN_H

#include "configforge/cf_source.h"

#ifdef __cplusplus
extern "C" {
#endif

/* More sources than this in one configuration would mean a precedence
   chain nobody could reason about, which is the problem this library
   exists to prevent rather than to scale. */
#define CF_MAX_SOURCES 32

/* Render the chain for one key. The caller supplies the buffer, in
   keeping with the allocation rules; the return value is the length
   that would have been written, so a caller can size a buffer by
   calling once with cap 0.
   Returns 0 when the key is absent from the space. */
/* The low-level renderers. The public API in configforge.h wraps
   these; these take a key space and a source list directly, which is
   what the tests and the Doctor tool need. */
size_t cf_ks_explain(const cf_keyspace* ks, const cf_source_view* sv, const char* key,
                     size_t keylen, char* buf, size_t cap);

/* Render every key. Sources that never win appear in the trailing
   summary, which is what makes a dead source visible. */
size_t cf_ks_inspect(const cf_keyspace* ks, const cf_source_view* sv, char* buf,
                     size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_EXPLAIN_H */
