/*
 * cf_flatten.h - document events to and from the flat key space.
 *
 * The bridge between the serialization layer and the configuration
 * model. It lives on this side of the boundary on purpose: the
 * serialization layer may use the leaf types and may not touch a key
 * space, which is what keeps extracting it later a directory move.
 */

#ifndef CONFIGFORGE_CF_FLATTEN_H
#define CONFIGFORGE_CF_FLATTEN_H

#include "configforge/cf_keyspace.h"
#include "configforge/cf_serialize.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Both CF_MAX_DEPTH, a build limit (cf_build_options.h): one nesting
   limit for every decoder and both directions, so a document that one
   part of the library accepts no other part refuses. */
#define CF_FLATTEN_MAX_DEPTH CF_MAX_DEPTH
#define CF_EMIT_MAX_DEPTH CF_MAX_DEPTH

/* Eight bytes a level rather than twenty-four: a length within a key
   fits sixteen bits (CF_KEY_MAX is at most 65535), and no configuration
   array has four billion elements. CF_MAX_DEPTH levels of this sit in
   the frame of every source that reads a document. */
typedef struct fl_frame {
    cf_u32 index;
    cf_u16 saved_len;
    cf_u8  is_array;
} fl_frame;

typedef struct cf_flatten {
    cf_keyspace* ks;
    int          rank;

    char   path[CF_KEY_MAX];
    size_t plen;

    fl_frame stack[CF_FLATTEN_MAX_DEPTH];
    int      depth;

    cf_bool     failed;   /* the key space refused a candidate     */
    cf_bool     overflow; /* a key or the nesting grew too large   */
    cf_doc_sink sink;
} cf_flatten;

CF_API cf_bool            cf_flatten_init(cf_flatten* f, cf_keyspace* ks, int rank);
CF_API const cf_doc_sink* cf_flatten_sink(cf_flatten* f);

/* Walk a resolved key space, emitting the document it describes.
   Structure is reconstructed from the sorted keys in one pass: two
   adjacent keys share a prefix exactly as deep as the tables that
   enclose them both, so the sort is already the tree. */
CF_API cf_bool cf_doc_emit(const cf_keyspace* ks, const cf_doc_sink* out,
                           cf_error* err);

/* The same walk, with a say in what is included.
 *
 * An array is all or nothing. Emitting only the members that pass would
 * produce a shorter array than the one that was read, which is a
 * different configuration rather than a subset of one - so a group with
 * any surviving member keeps all of them. */
typedef cf_bool (*cf_emit_pred)(const void* ctx, const cf_entry* e);

CF_API cf_bool cf_doc_emit_where(const cf_keyspace* ks, const cf_doc_sink* out,
                                 cf_emit_pred pred, const void* ctx, cf_error* err);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_FLATTEN_H */
