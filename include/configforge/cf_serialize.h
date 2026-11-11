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

/* How many bytes a reader skips at the start of a document: 3 when it
   begins with a UTF-8 byte order mark (EF BB BF), else 0. A mark is not
   part of the text, and only the first bytes are one. */
CF_API size_t cf_blob_skip_bom(const cf_blob* b);

/* Reads a whole file into the arena. CF_E_SOURCE when it cannot be
   opened or read; the caller decides whether that is fatal. */
CF_API cf_bool cf_blob_file(cf_blob* b, cf_arena* ar, const char* path, cf_error* err);

/* Where a file's bytes go for one reading (D-61): an arena of their own,
   from `ar`'s allocator, so that they are gone once the reader is done -
   the key space keeps copies of the values - or `ar` itself when it is
   fixed and has none to lend. NULL when there is no room for one.
   cf_blob_arena_done frees it, zeroed, when it was its own. */
CF_API cf_arena* cf_blob_arena(cf_arena* ar);
CF_API void      cf_blob_arena_done(cf_arena* bytes, cf_arena* ar);

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

/* JSON, written by hand where a document is not the key space's (a
   report, a diff): text as a JSON string, escaped; a value as JSON. */
CF_API void cf_sink_json_string(cf_sink* out, const char* p, size_t n);
CF_API void cf_sink_json_value(cf_sink* out, const cf_value* v);

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

/* Zero-initialize a reader's sink before filling its callbacks and
   `size`. A later member is called only when `size` reaches past it.
   Encoders fill only the 1.0 prefix: an older caller may have supplied
   that smaller object as their output. */
typedef struct cf_doc_sink {
    size_t size;
    cf_bool (*begin_table)(void* ctx, const char* name, size_t n);
    cf_bool (*begin_array)(void* ctx, const char* name, size_t n);
    cf_bool (*scalar)(void* ctx, const char* name, size_t n, const cf_value* v,
                      cf_bool textual, const cf_loc* at);
    cf_bool (*end)(void* ctx);
    void* ctx;

    /* Optional reader workspace, including room for a trailing NUL.
       The buffer must survive until scalar returns; the next call may
       reuse it. NULL reports exhaustion. Without this callback, the
       built-in JSON/TOML readers refuse strings over 255 bytes. The
       configuration loader supplies it from the caller's arena. */
    char* (*reserve_text)(void* ctx, size_t n);

    /* Optional reader bookkeeping: n bytes, aligned for any scalar, that
       stay valid until decode returns. Each call is new memory; a reader
       that outgrows a block asks for a larger one and copies. NULL
       reports exhaustion. The TOML reader keeps what the document has
       defined in it, to refuse what TOML forbids defining twice; without
       this callback it does not check, and a later key wins as in 1.1.
       The configuration loader supplies it from the caller's arena. (1.2) */
    void* (*reserve_work)(void* ctx, size_t n);
} cf_doc_sink;

/* ------------------------------------------------------------------ *
 * Codecs
 * ------------------------------------------------------------------ */

enum {
    CF_CODEC_READ   = 1u << 0,
    CF_CODEC_WRITE  = 1u << 1,
    CF_CODEC_TYPED  = 1u << 2, /* carries types; otherwise all text  */
    CF_CODEC_ARRAYS = 1u << 3,
    /* Arrays of values only - not of tables, not of arrays, not empty:
       INI's name[] lines (D-60). Implied by CF_CODEC_ARRAYS. (1.2) */
    CF_CODEC_SCALAR_ARRAYS = 1u << 4
};

/* `size` is sizeof(cf_codec) where the codec was compiled (D-51). */
typedef struct cf_codec {
    size_t      size;
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

/* The most encoder state cf_emit can hold, on its own frame. It follows
   the limits, as an encoder's state does - a key-sized path and a few
   bytes for each level of nesting - and every built-in encoder asserts
   at compile time that it fits, so the bound cannot drift from them. */
#define CF_ENCODER_STATE_MAX (CF_KEY_MAX + (CF_MAX_DEPTH + 2) * 8 + 64)

/* Compiled-in codecs only. NULL when the format exists but this build
   left it out, which the caller must report rather than ignore: a
   configuration system whose purpose is explaining where values come
   from must not have a build option that makes a file invisible. */
CF_API const cf_codec* cf_codec_by_name(const char* name);
CF_API const cf_codec* cf_codec_by_path(const char* path);

/* Among a caller's codecs (cf_builder_codec): the one called `name`
   when it is not NULL, else the one whose extensions match `path`;
   NULL for none. The built-in codecs are not looked at. */
CF_API const cf_codec* cf_codec_among(const cf_codec* const* list, size_t n,
                                      const char* name, const char* path);

/* Names a format this library knows about whether or not it was
   compiled in, so the error can say which option to turn on. */
CF_API const char* cf_format_of_path(const char* path);
CF_API const char* cf_format_option(const char* format);

CF_API size_t          cf_codec_count(void);
CF_API const cf_codec* cf_codec_at(size_t i);

/* ------------------------------------------------------------------ *
 * The format from the name, then the content (D-73)
 *
 * Several simple strategies in turn, the first that answers deciding, and
 * each says why it did: the format a program names; the name's extension;
 * and, when the name gives none, the content. The content is never taken
 * on a guess at a few bytes: a look at the first bytes only orders the
 * candidates, and the decision is by reading - each codec reads the bytes
 * into a sink that keeps a hash of the events, and one that accepts them
 * and yields at least one keyed value is a candidate. A strict format
 * (the builder's own, JSON, JSON5, TOML, XML) is taken at once; a loose
 * one (YAML, INI) only when no other loose format reads the bytes into
 * different events, else the bytes are refused as ambiguous. Properties,
 * whose grammar accepts nearly any text, is never chosen from content.
 * ------------------------------------------------------------------ */

typedef enum cf_detect_how {
    CF_DETECT_NONE  = 0,     /* no format: empty, unreadable or ambiguous */
    CF_DETECT_NAMED = 1,     /* the program named it (set by the callers  */
                             /* that take a name; cf_codec_detect has none) */
    CF_DETECT_EXTENSION = 2, /* the name's extension gave it              */
    CF_DETECT_CONTENT   = 3  /* the content did                           */
} cf_detect_how;

/* What cf_codec_detect says besides the codec. When the bytes are refused
   as ambiguous, `how` is CF_DETECT_NONE and `first` and `second` are the
   two loose formats that read them differently; else both are NULL. */
typedef struct cf_detect {
    cf_detect_how   how;
    const cf_codec* first;
    const cf_codec* second;
} cf_detect;

/* The codec for `in`, or NULL. `own` (n of them) are a builder's own
   codecs, looked at first, as for extensions; `name`, when not NULL, is a
   file name to take the extension's format from - that format is returned
   as it stands, without reading, so no document read before is read
   differently; with no name, or none whose extension a codec claims, the
   content decides. `ar` is the trial readers' work space, and is not
   reset: pass one that can be thrown away (cf_blob_arena), or NULL for one
   made and released inside the call. `out`, when not NULL, says how the
   answer was reached. */
CF_API const cf_codec* cf_codec_detect(const cf_codec* const* own, size_t n,
                                       const cf_blob* in, const char* name,
                                       cf_arena* ar, cf_detect* out);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_SERIALIZE_H */
