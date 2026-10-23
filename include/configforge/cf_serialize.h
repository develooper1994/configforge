/*
 * cf_serialize.h - the serialization layer.
 *
 * Reading a configuration file and writing a provenance report are the
 * same problem seen from two sides: bytes to a key space, and a key
 * space to bytes. They are solved once, here, in a layer that knows
 * nothing about configuration semantics.
 *
 *     bytes  <->  blob / sink          byte source and sink
 *                 codec                format grammar, both directions
 *                 document events      begin_table, begin_array, scalar
 *                 flattener            events <-> flat key space
 *
 * The flattener is deliberately not in this file. This layer may use
 * the leaf types - values, errors, arenas - and may not touch the key
 * space, the definitions or the sources. That boundary is checked in
 * CI so that extracting this layer later stays a directory move.
 */

#ifndef CONFIGFORGE_CF_SERIALIZE_H
#define CONFIGFORGE_CF_SERIALIZE_H

#include "configforge/cf_value.h"
#include "configforge/cf_alloc.h"
#include "configforge/cf_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ *
 * Bytes in
 *
 * A whole document in memory rather than a streaming reader.
 * Configuration files are kilobytes; a streaming parser would be twice
 * the code, would need its own pushback buffer, and would give up both
 * zero-copy string slices and trivially exact locations. The one thing
 * streaming buys - bounded memory over a huge document - is not a
 * property any configuration file needs.
 * ------------------------------------------------------------------ */

typedef struct cf_blob {
    const char* p;
    size_t      n;
    const char* name; /* shown in locations: "app.toml" */
} cf_blob;

CF_API void cf_blob_mem(cf_blob* b, const char* p, size_t n, const char* name);

/* Reads a whole file into the arena. CF_E_SOURCE when it cannot be
   opened or read; the caller decides whether that is fatal. */
CF_API cf_bool cf_blob_file(cf_blob* b, cf_arena* ar, const char* path, cf_error* err);

/* ------------------------------------------------------------------ *
 * Bytes out
 *
 * Measure-or-write, exactly like cf_fmt: with a NULL buffer nothing is
 * stored and the length still accumulates, so a caller sizes a buffer
 * by rendering once into nothing.
 * ------------------------------------------------------------------ */

typedef struct cf_sink {
    char*  buf;
    size_t cap;
    size_t len; /* what would have been written */
} cf_sink;

CF_API void    cf_sink_init(cf_sink* s, char* buf, size_t cap);
CF_API void    cf_sink_write(cf_sink* s, const char* p, size_t n);
CF_API void    cf_sink_putc(cf_sink* s, char c);
CF_API void    cf_sink_puts(cf_sink* s, const char* z);
CF_API void    cf_sink_fmt(cf_sink* s, const char* fmt, ...);
CF_API void    cf_sink_terminate(cf_sink* s);
CF_API cf_bool cf_sink_overflowed(const cf_sink* s);

/* ------------------------------------------------------------------ *
 * Locations
 * ------------------------------------------------------------------ */

typedef struct cf_loc {
    const char*  file;
    unsigned int line;
    unsigned int col;
} cf_loc;

/* "app.toml:14:3", or just the file when there is no position. */
CF_API size_t cf_loc_text(const cf_loc* at, char* buf, size_t cap);

/* ------------------------------------------------------------------ *
 * Document events
 *
 * A parser is an event producer, never a source. It emits these and
 * knows nothing about keys; one shared flattener owns key
 * construction, array counting and location formatting.
 *
 * Without that split the key rules would live in every parser and
 * would drift apart, which is the repetition this project exists to
 * remove - applied to its own internals.
 *
 * `name` is the member name inside a table and NULL inside an array,
 * where position is the name.
 * ------------------------------------------------------------------ */

typedef struct cf_doc_sink {
    cf_bool (*begin_table)(void* ctx, const char* name, size_t n);
    cf_bool (*begin_array)(void* ctx, const char* name, size_t n);
    cf_bool (*scalar)(void* ctx, const char* name, size_t n, const cf_value* v,
                      cf_bool textual, const cf_loc* at);
    cf_bool (*end)(void* ctx);
    void* ctx;
} cf_doc_sink;

/* ------------------------------------------------------------------ *
 * Codecs
 * ------------------------------------------------------------------ */

enum {
    CF_CODEC_READ   = 1u << 0,
    CF_CODEC_WRITE  = 1u << 1,
    CF_CODEC_TYPED  = 1u << 2, /* carries types; otherwise all text  */
    CF_CODEC_ARRAYS = 1u << 3
};

typedef struct cf_codec {
    const char* name;
    const char* extensions; /* ".json;.jsn", lowercase, with dots */
    unsigned    caps;
    cf_bool (*decode)(const cf_blob* in, const cf_doc_sink* out, cf_error* err);

    /* Encoding is the same event interface in reverse: the caller
       walks a document and the codec receives the events as a sink.
       Both the state and the sink are supplied by the caller, so a
       codec holds no state of its own and two encodings can run at
       once. Returning a pointer to a static sink would have been
       shorter and would have been global mutable state. */
    cf_bool (*encoder)(cf_sink* out, void* state, cf_doc_sink* sink);
    size_t encoder_state_size;
} cf_codec;

/* Compiled-in codecs only. NULL when the format exists but this build
   left it out, which the caller must report rather than ignore: a
   configuration system whose purpose is explaining where values come
   from must not have a build option that makes a file invisible. */
CF_API const cf_codec* cf_codec_by_name(const char* name);
CF_API const cf_codec* cf_codec_by_path(const char* path);

/* Names a format this library knows about whether or not it was
   compiled in, so the error can say which option to turn on. */
CF_API const char* cf_format_of_path(const char* path);
CF_API const char* cf_format_option(const char* format);

CF_API size_t          cf_codec_count(void);
CF_API const cf_codec* cf_codec_at(size_t i);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_SERIALIZE_H */
