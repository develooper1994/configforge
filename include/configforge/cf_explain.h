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

#include "configforge/cf_define.h"
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
CF_API size_t cf_ks_explain(const cf_keyspace* ks, const cf_source_view* sv,
                            const char* key, size_t keylen, char* buf, size_t cap);

/* Render every key. Sources that never win appear in the trailing
   summary, which is what makes a dead source visible. */
CF_API size_t cf_ks_inspect(const cf_keyspace* ks, const cf_source_view* sv, char* buf,
                            size_t cap);

/* The same two, with every value of a key that `ds` marks CF_SENSITIVE
   shown as "<sensitive>", and as "<sensitive, N bytes>" when it is text:
   what a blind report gives, and nothing more. The chain - which source
   won, which lost, and where each was set - is shown in full, because
   that is the question and it is not the secret.

   cf_explain and cf_inspect pass the configuration's own definitions. A
   diagnostic is where a secret is most often pasted: into a ticket, a
   chat, a log somebody else reads. The two above are the raw renderers
   for a key space with no definitions. */
CF_API size_t cf_ks_explain_redacted(const cf_keyspace* ks, const cf_source_view* sv,
                                     const cf_defset* ds, const char* key,
                                     size_t keylen, char* buf, size_t cap);
CF_API size_t cf_ks_inspect_redacted(const cf_keyspace* ks, const cf_source_view* sv,
                                     const cf_defset* ds, char* buf, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_EXPLAIN_H */
