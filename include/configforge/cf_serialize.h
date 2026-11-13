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

/* Whether n bytes are UTF-8 as RFC 3629 has it: no overlong form, no
   surrogate (U+D800 to U+DFFF), nothing past U+10FFFF, no sequence cut
   short. A NUL is a character like any other. (1.5) */
CF_API cf_bool cf_text_is_utf8(const char* p, size_t n);

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
    CF_CODEC_SCALAR_ARRAYS = 1u << 4,
    /* Writes bytes, each text with its length before it: cf_emit cannot
       double a ${ in what it has written, and asks the encoder to
       (cf_encoder_head). Also what a reader of such bytes says it is: the
       absence of this flag is "text". (1.5) */
    CF_CODEC_BINARY = 1u << 5,
    /* Chosen by its name or its extensions, never from content: a layout
       reads bytes no other format would refuse, so a guess at them would
       be taken (D-73, D-92). (1.5) */
    CF_CODEC_NAMED_ONLY = 1u << 6,
    /* Cannot be read without a schema or layout the program gives: the
       bytes do not say what they are (a TLV layout, D-93). A format that
       describes itself lacks it; one that is text lacks CF_CODEC_BINARY.
       (1.5) */
    CF_CODEC_SCHEMA = 1u << 7
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

    /* (1.5, D-92) Reading for a codec made at run time from a
       description - a TLV layout (cf_tlv_codec) - which reaches it
       through `self`: taken in place of `decode` when `size` reaches it
       and it is not NULL. `data` is the description; the library never
       reads it, frees it or copies it. */
    cf_bool (*decode_self)(const struct cf_codec* self, const cf_blob* in,
                           const cf_doc_sink* out, cf_error* err);
    const void* data;
} cf_codec;

/* The most encoder state cf_emit can hold, on its own frame. It follows
   the limits, as an encoder's state does - a key-sized path and a few
   bytes for each level of nesting - and every built-in encoder asserts
   at compile time that it fits, so the bound cannot drift from them. */
#define CF_ENCODER_STATE_MAX (CF_KEY_MAX + (CF_MAX_DEPTH + 2) * 8 + 64)

/* The first member of a binary encoder's state (CF_CODEC_BINARY):
   the encoder function zeroes it, and the caller may then set
   CF_ENCODE_ESCAPE_REFS, which has every ${ in a text value written
   as $${, as cf_emit does for the text formats after writing.
   (1.5, D-91) */
typedef struct cf_encoder_head {
    unsigned flags;
} cf_encoder_head;
enum { CF_ENCODE_ESCAPE_REFS = 1u << 0 };

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

/* Reads `in` through `c`: its decode_self when its size reaches that
   member and it is set, else its decode. False with `err` set when it
   refuses the bytes, or has neither. (1.5) */
CF_API cf_bool cf_codec_decode(const cf_codec* c, const cf_blob* in,
                               const cf_doc_sink* out, cf_error* err);

/* Whether `c` reads: CF_CODEC_READ and a decoder of either kind. (1.5) */
CF_API cf_bool cf_codec_can_read(const cf_codec* c);

/* Names a format this library knows about whether or not it was
   compiled in, so the error can say which option to turn on. */
CF_API const char* cf_format_of_path(const char* path);
CF_API const char* cf_format_option(const char* format);

CF_API size_t          cf_codec_count(void);
CF_API const cf_codec* cf_codec_at(size_t i);

/* The built-in codecs whose caps hold every bit of `all_of` and none of
   `none_of`, in registry order: at most `cap` of them are written to
   `out` (NULL when `cap` is 0), and the number that match in all is
   returned, as the size cf_report needs is. (1.5) */
CF_API size_t cf_codec_select(unsigned all_of, unsigned none_of, const cf_codec** out,
                              size_t cap);

/* What kind of format `c` is, as text for a person, comma-separated in
   this order: "text" or "binary"; "self-describing" or "schema-bound";
   "typed" or "untyped"; "arrays", "arrays of values" or "no arrays";
   "read and written", "read" or "written" ("neither read nor written"
   for a codec that does neither). Returns the length needed, as
   snprintf does; a NULL codec gives 0 and an empty string. (1.5) */
CF_API size_t cf_codec_kind(const cf_codec* c, char* buf, size_t cap);

/* ------------------------------------------------------------------ *
 * Tag, length, value, by a layout the program gives (D-93)
 *
 * TLV has no one standard, so it is a codec the program makes: the
 * layout says how wide a tag and a length are and in which byte
 * order, how the document ends, and what each tag is - its key, its
 * type, and for a nested one the tags inside it. Each value is placed
 * in explain() as "<file>, byte <n>", n the offset of its tag. A tag
 * the layout does not name is refused unless the layout says to skip
 * or keep it. Read only: writing back keeps what a layout skips, and
 * is wave 6's.
 * ------------------------------------------------------------------ */

enum { CF_TLV_FIXED = 0, CF_TLV_BER = 1 }; /* the layout's kind */
enum {
    CF_TLV_BIG    = 1,
    CF_TLV_LITTLE = 2
}; /* byte order; 0 in a
      tag is the layout's */
enum {
    CF_TLV_TEXT   = 1, /* UTF-8; trailing NUL bytes dropped              */
    CF_TLV_INT    = 2, /* signed, two's complement, `width` bytes        */
    CF_TLV_UINT   = 3, /* unsigned, `width` bytes                        */
    CF_TLV_BOOL   = 4, /* one byte, 0 or 1                               */
    CF_TLV_REAL   = 5, /* IEEE 754, 4 or 8 bytes                         */
    CF_TLV_BCD    = 6, /* packed decimal digits, high nibble first      */
    CF_TLV_HEX    = 7, /* any bytes, as lowercase hexadecimal text       */
    CF_TLV_RECORD = 8, /* a fixed record: skipped or refused             */
    CF_TLV_NEST   = 9  /* more TLV inside: a table                       */
};
enum { CF_TLV_SKIP = 1u << 0, CF_TLV_LIST = 1u << 1 }; /* a tag's flags */
enum {
    CF_TLV_END        = 1u << 0,
    CF_TLV_END_LENGTH = 1u << 1,
    CF_TLV_PADDING    = 1u << 2
}; /* a layout's */
enum { CF_TLV_UNKNOWN_REFUSE = 0, CF_TLV_UNKNOWN_SKIP = 1, CF_TLV_UNKNOWN_KEEP = 2 };

/* One tag of a layout. Fixed for 1.x, as cf_pair is: it is an array's
   element, and an array cannot grow. `more` is NULL; a later 1.x may
   point it at a sized struct of more, and an earlier library refuses a
   tag whose `more` is set rather than ignore it (D-51). */
typedef struct cf_tlv_tag {
    cf_u32 tag;                     /* CF_TLV_FIXED: its value read in the
                                       layout's order; CF_TLV_BER: its
                                       identifier bytes as one big-endian
                                       number, as EMV writes it: 0x9F02 */
    const char* key;                /* "port", or dotted, "net.port";
                                       NULL for a skipped or refused one */
    int      type;                  /* CF_TLV_TEXT ...                   */
    unsigned width;                 /* bytes: 0 any (TEXT, HEX, BCD, RECORD;
                                       INT and UINT: 1 to 8, the value's
                                       own); else exactly this many      */
    int                      order; /* INT, UINT, REAL: 0 the layout's   */
    unsigned                 flags; /* CF_TLV_SKIP, CF_TLV_LIST          */
    const struct cf_tlv_tag* inner; /* CF_TLV_NEST: the tags inside      */
    size_t                   ninner;
    const void*              more; /* NULL                              */
} cf_tlv_tag;

/* A layout; start from CF_TLV_LAYOUT_INIT. Each table of tags is in
   ascending order of `tag`, which is how it is searched and how a tag
   listed twice is found. */
typedef struct cf_tlv_layout {
    size_t            size;         /* sizeof(cf_tlv_layout)          */
    const char*       name;         /* the format's name: "devcfg"    */
    const char*       extensions;   /* ".dcfg", or NULL: by name only */
    int               kind;         /* CF_TLV_FIXED or CF_TLV_BER     */
    unsigned          tag_width;    /* CF_TLV_FIXED: 1, 2 or 4        */
    unsigned          length_width; /* CF_TLV_FIXED: 1, 2 or 4        */
    int               order;        /* CF_TLV_FIXED: BIG or LITTLE    */
    unsigned          flags;        /* CF_TLV_END, _END_LENGTH, _PADDING */
    cf_u32            end_tag;      /* with CF_TLV_END                */
    int               unknown;      /* CF_TLV_UNKNOWN_REFUSE ...      */
    const cf_tlv_tag* tags;         /* the top level's                */
    size_t            ntags;
} cf_tlv_layout;

#define CF_TLV_LAYOUT_INIT                                                             \
    {sizeof(cf_tlv_layout), NULL, NULL, CF_TLV_FIXED, 0, 0, 0, 0, 0,                   \
     CF_TLV_UNKNOWN_REFUSE, NULL, 0}

/* A codec that reads `layout`, made in `out`: the program owns `out`,
   the layout, and every table and key it points to, and each must
   outlive every configuration that reads with the codec - as any codec
   given to cf_builder_codec must; a `static` one is the usual way.
   The layout is checked whole here, and false with `err` set
   (CF_E_DEFINITION, the layout's name as source) names the first
   mistake; `out` then reads nothing. The codec is named the layout's
   name, takes its extensions, reads only, and is never chosen from
   content (CF_CODEC_NAMED_ONLY). Off in a build without CF_WITH_TLV,
   and says so. (1.5) */
CF_API cf_bool cf_tlv_codec_sized(cf_codec* out, size_t out_size,
                                  const cf_tlv_layout* layout, cf_error* err);
#define cf_tlv_codec(out, layout, err)                                                 \
    cf_tlv_codec_sized((out), sizeof(cf_codec), (layout), (err))

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
 * (the builder's own, CBOR, JSON, JSON5, TOML, XML) is taken at once; a loose
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
