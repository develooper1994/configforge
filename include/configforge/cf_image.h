/*
 * cf_image.h - the binary snapshot image.
 *
 * The one thing a binary form does that no text format can: load
 * without parsing and without allocating. That is what a target with
 * no filesystem, no heap and a boot-time budget needs, and it is the
 * artifact the freestanding profile was missing - build-time
 * configuration was described as a source and had nothing to be.
 *
 * Not a codec. The key space is already a sorted array plus a string
 * arena, so the image is a picture of that rather than a document, and
 * routing it through the document event model would mean inventing a
 * shape that happens to match the layout. See decision D-28.
 *
 *     magic     "CFGB", major, minor
 *     layout    header | key index | values | strings
 *     order     little endian, always
 *     integrity the determinism digest, checked on load
 *
 * Optionally the whole candidate chain travels with it, so a device
 * with no filesystem can still answer why a value is what it is.
 */

#ifndef CONFIGFORGE_CF_IMAGE_H
#define CONFIGFORGE_CF_IMAGE_H

#include "configforge/cf_keyspace.h"
#include "configforge/cf_serialize.h"
#include "configforge/cf_source.h"
#include "configforge/cf_explain.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CF_IMAGE_MAGIC_0 'C'
#define CF_IMAGE_MAGIC_1 'F'
#define CF_IMAGE_MAGIC_2 'G'
#define CF_IMAGE_MAGIC_3 'B'

#define CF_IMAGE_MAJOR 1
#define CF_IMAGE_MINOR 0

enum {
    /* Carry every candidate, not just the winners, so explain() still
       works after the image is loaded. Larger, and the only reason the
       image is a ConfigForge artifact rather than a blob. */
    CF_IMAGE_PROVENANCE = 1u << 0
};

/* Write the image. Measure-or-write, like everything else that renders:
   with a NULL buffer nothing is stored and the length still
   accumulates. */
CF_API size_t cf_image_write(const cf_keyspace* ks, const cf_source_view* sv,
                             unsigned flags, char* buf, size_t cap);

/* ------------------------------------------------------------------ *
 * Reading
 *
 * The image is used in place. Nothing is copied, nothing is allocated,
 * and the buffer must outlive the view - which is exactly the point on
 * a target where the buffer is a `const char[]` in flash.
 * ------------------------------------------------------------------ */

typedef struct cf_image {
    const char* base;
    size_t      size;
    cf_u32      count;
    cf_u32      flags;
    cf_u64      digest;
    const char* index; /* key records                 */
    const char* values;
    const char* strings;
    cf_bool     swapped; /* the host reads the other way round */

    /* Room a later 1.x may use without the struct growing: the caller
       allocates a cf_image and cf_image_open fills it (D-51). */
    void* reserved[4];
} cf_image;

/* Validates the header, the bounds and the digest. Refuses rather than
   guesses: a truncated image and a corrupt one look the same from
   inside, and neither should produce a configuration. */
CF_API cf_bool cf_image_open(cf_image* img, const char* buf, size_t n, cf_error* err);

CF_API size_t  cf_image_count(const cf_image* img);
CF_API cf_bool cf_image_at(const cf_image* img, size_t i, const char** key,
                           size_t* klen, cf_value* v);
CF_API cf_bool cf_image_get(const cf_image* img, const char* key, size_t klen,
                            cf_value* v);

/* Load an image into a key space at a rank, which is what a source
   does. Text values point into the image, so it must outlive the
   configuration. */
CF_API cf_bool cf_image_load(const cf_image* img, cf_keyspace* ks, int rank,
                             cf_error* err);

/* ------------------------------------------------------------------ *
 * As a source
 * ------------------------------------------------------------------ */

CF_API size_t cf_source_image_state_size(void);

/* From memory: a build-time image linked into the program. No file, no
   parser, no allocation - the freestanding path. */
CF_API cf_bool cf_source_image_mem(cf_source* s, const char* name, int rank,
                                   const char* buf, size_t n, void* state,
                                   size_t state_size);

/* From a file, for hosted targets. */
CF_API cf_bool cf_source_image_file(cf_source* s, const char* name, int rank,
                                    const char* path, void* state, size_t state_size);

CF_API const cf_error* cf_source_image_error(const cf_source* s);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_IMAGE_H */
