/*
 * tlv.c - tag, length, value, by a layout the program gives (D-93). C89.
 *
 * TLV has no one standard, so the program describes its own: how wide a
 * tag and a length are, in which byte order, how the document ends, and
 * what each tag is - its key and its type. cf_tlv_codec checks the layout
 * whole and makes a codec that reads it through decode_self (D-92), the
 * layout its data; the library copies nothing of it.
 *
 * Every value is placed by the byte its tag starts at, "<name>, byte <n>",
 * the place a hex dump of the file shows it. A length is checked against
 * the bytes left before its value is read, so a length of four gigabytes
 * costs nothing; a tag is found in its table by binary search.
 *
 * In the bridge rather than with the other readers: a layout's keys are
 * checked as the key space checks them, which the serialization layer
 * may not reach.
 */

#include "configforge/cf_keyspace.h"
#include "configforge/cf_numeric.h"
#include "configforge/cf_serialize.h"

#include <stddef.h>
#include <string.h>

#if CF_WITH_TLV

/* ------------------------------------------------------------------ *
 * A tag shown in a message
 * ------------------------------------------------------------------ */

/* `v` as two uppercase hexadecimal digits for each of `width` bytes:
   cf_fmt's %x takes an unsigned int, which may be 16 bits, so the digits
   are made here. */
static void tlv_hex(char out[9], cf_u32 v, unsigned width) {
    static const char digits[] = "0123456789ABCDEF";
    unsigned          i, n = (width > 4u) ? 8u : width * 2u;

    for (i = 0; i < n; ++i) {
        out[i] = digits[(v >> (4u * (n - 1u - i))) & 0xFu];
    }
    out[n] = '\0';
}

/* ------------------------------------------------------------------ *
 * The layout's checks
 * ------------------------------------------------------------------ */

/* The type's word, with its article: "an int". */
static const char* tlv_type_word(int type) {
    static const char* const words[] = {"a text", "an int",   "a uint",
                                        "a bool", "a real",   "a bcd",
                                        "a hex",  "a record", "a nest"};
    return (type >= CF_TLV_TEXT && type <= CF_TLV_NEST) ? words[type - 1] : "a type";
}

static const char* tlv_name(const cf_tlv_layout* l) {
    return (l->name != NULL) ? l->name : "";
}

static cf_bool tlv_bad(cf_error* err, const cf_tlv_layout* l, const char* what) {
    return cf_error_set(err, CF_E_DEFINITION, NULL, l->name, "layout '%s': %s",
                        tlv_name(l), what);
}

/* The digits of a tag in its layout: the tag width's for FIXED. */
static unsigned tlv_tag_bytes(const cf_tlv_layout* l, cf_u32 tag) {
    (void)tag;
    return l->tag_width;
}

/* Whether `tag` fits a FIXED layout's tag width. */
static cf_bool tlv_fits(const cf_tlv_layout* l, cf_u32 tag) {
    if (l->kind != CF_TLV_FIXED || l->tag_width >= 4u) {
        return CF_TRUE;
    }
    return ((tag >> (8u * l->tag_width)) == 0) ? CF_TRUE : CF_FALSE;
}

/* A format's name: one segment of a key. */
static cf_bool tlv_name_ok(const char* name) {
    if (name == NULL || strchr(name, '.') != NULL) {
        return CF_FALSE;
    }
    return cf_key_valid(name, strlen(name));
}

/* Whether the width suits the type. */
static cf_bool tlv_width_ok(int type, unsigned w) {
    switch (type) {
    case CF_TLV_INT:
    case CF_TLV_UINT:
        return (w == 0 || w == 1 || w == 2 || w == 4 || w == 8) ? CF_TRUE : CF_FALSE;
    case CF_TLV_BOOL:
        return (w <= 1u) ? CF_TRUE : CF_FALSE;
    case CF_TLV_REAL:
        return (w == 4 || w == 8) ? CF_TRUE : CF_FALSE;
    case CF_TLV_NEST:
        return (w == 0) ? CF_TRUE : CF_FALSE;
    default:
        return CF_TRUE;
    }
}

/* One entry of a table; `prev` is the one before it, or NULL. */
static cf_bool tlv_check_tag(const cf_tlv_layout* l, const cf_tlv_tag* t,
                             const cf_tlv_tag* prev, cf_error* err) {
    char hex[9];
    char before[9];

    tlv_hex(hex, t->tag, tlv_tag_bytes(l, t->tag));
    if (prev != NULL && t->tag <= prev->tag) {
        tlv_hex(before, prev->tag, tlv_tag_bytes(l, prev->tag));
        return cf_error_set(
            err, CF_E_DEFINITION, NULL, l->name,
            "layout '%s': 0x%s is listed after 0x%s: list a table's tags "
            "in ascending order, each once",
            tlv_name(l), hex, before);
    }
    if (!tlv_fits(l, t->tag)) {
        tlv_hex(hex, t->tag, 4u);
        return cf_error_set(err, CF_E_DEFINITION, NULL, l->name,
                            "layout '%s': tag 0x%s is wider than a tag", tlv_name(l),
                            hex);
    }
    if ((l->flags & CF_TLV_END) != 0 && t->tag == l->end_tag) {
        return cf_error_set(err, CF_E_DEFINITION, NULL, l->name,
                            "layout '%s': tag 0x%s is the end tag", tlv_name(l), hex);
    }
    if (t->more != NULL) {
        return cf_error_set(
            err, CF_E_DEFINITION, NULL, l->name,
            "layout '%s': tag 0x%s has `more` set, which this library does "
            "not know",
            tlv_name(l), hex);
    }
    if ((t->flags & ~(unsigned)(CF_TLV_SKIP | CF_TLV_LIST)) != 0) {
        return cf_error_set(
            err, CF_E_DEFINITION, NULL, l->name,
            "layout '%s': tag 0x%s has flags %u that this library does not "
            "know",
            tlv_name(l), hex, t->flags);
    }
    if (t->type < CF_TLV_TEXT || t->type > CF_TLV_NEST) {
        return cf_error_set(err, CF_E_DEFINITION, NULL, l->name,
                            "layout '%s': tag 0x%s has type %d, not a CF_TLV_ type",
                            tlv_name(l), hex, t->type);
    }
    if (t->key == NULL) {
        if (t->type != CF_TLV_RECORD && (t->flags & CF_TLV_SKIP) == 0) {
            return cf_error_set(err, CF_E_DEFINITION, NULL, l->name,
                                "layout '%s': tag 0x%s has no key", tlv_name(l), hex);
        }
    } else if (!cf_key_valid(t->key, strlen(t->key))) {
        return cf_error_set(err, CF_E_DEFINITION, NULL, l->name,
                            "layout '%s': tag 0x%s: '%s' is not a valid key",
                            tlv_name(l), hex, t->key);
    }
    if (!tlv_width_ok(t->type, t->width)) {
        return cf_error_set(err, CF_E_DEFINITION, NULL, l->name,
                            "layout '%s': tag 0x%s: %s of %u bytes", tlv_name(l), hex,
                            tlv_type_word(t->type), t->width);
    }
    if (t->order != 0 && t->order != CF_TLV_BIG && t->order != CF_TLV_LITTLE) {
        return cf_error_set(err, CF_E_DEFINITION, NULL, l->name,
                            "layout '%s': tag 0x%s: say the byte order: CF_TLV_BIG or "
                            "CF_TLV_LITTLE, or 0 for the layout's",
                            tlv_name(l), hex);
    }
    if (t->type == CF_TLV_NEST) {
        if ((t->inner == NULL) != (t->ninner == 0)) {
            return cf_error_set(err, CF_E_DEFINITION, NULL, l->name,
                                "layout '%s': tag 0x%s: a nest's tags and their count "
                                "disagree",
                                tlv_name(l), hex);
        }
    } else if (t->inner != NULL || t->ninner != 0) {
        return cf_error_set(err, CF_E_DEFINITION, NULL, l->name,
                            "layout '%s': tag 0x%s: only a nest has tags inside",
                            tlv_name(l), hex);
    }
#if !CF_WITH_REAL
    if (t->type == CF_TLV_REAL) {
        return cf_error_set(err, CF_E_DEFINITION, NULL, l->name,
                            "layout '%s': tag 0x%s is a real, and this build has no "
                            "floating point support",
                            tlv_name(l), hex);
    }
#endif
    if (t->type == CF_TLV_NEST || t->type == CF_TLV_BCD || t->type == CF_TLV_HEX) {
        return cf_error_set(err, CF_E_DEFINITION, NULL, l->name,
                            "layout '%s': tag 0x%s: %s is not read yet", tlv_name(l),
                            hex, tlv_type_word(t->type));
    }
    if ((t->flags & CF_TLV_LIST) != 0) {
        return cf_error_set(err, CF_E_DEFINITION, NULL, l->name,
                            "layout '%s': tag 0x%s: a list is not read yet",
                            tlv_name(l), hex);
    }
    return CF_TRUE;
}

/* A table and every table inside it, to CF_MAX_DEPTH levels. */
static cf_bool tlv_check_table(const cf_tlv_layout* l, const cf_tlv_tag* tags, size_t n,
                               int depth, cf_error* err) {
    size_t i;

    if (depth > CF_MAX_DEPTH) {
        return cf_error_set(err, CF_E_DEFINITION, NULL, l->name,
                            "layout '%s': tables nest deeper than %d levels",
                            tlv_name(l), (int)CF_MAX_DEPTH);
    }
    for (i = 0; i < n; ++i) {
        if (!tlv_check_tag(l, &tags[i], (i > 0) ? &tags[i - 1] : NULL, err)) {
            return CF_FALSE;
        }
        if (tags[i].type == CF_TLV_NEST) {
            if (!tlv_check_table(l, tags[i].inner, tags[i].ninner, depth + 1, err)) {
                return CF_FALSE;
            }
        }
    }
    return CF_TRUE;
}

/* The layout's own fields, then its tables. */
static cf_bool tlv_check(const cf_tlv_layout* l, cf_error* err) {
    unsigned known = CF_TLV_END | CF_TLV_END_LENGTH | CF_TLV_PADDING;
    char     hex[9];

    if (!tlv_name_ok(l->name)) {
        return cf_error_set(
            err, CF_E_DEFINITION, NULL, l->name,
            "layout '%s': '%s' is not a format's name: lowercase letters, "
            "digits, '_' and '-'",
            tlv_name(l), tlv_name(l));
    }
    if (l->kind != CF_TLV_FIXED && l->kind != CF_TLV_BER) {
        return cf_error_set(
            err, CF_E_DEFINITION, NULL, l->name,
            "layout '%s': kind %d is neither CF_TLV_FIXED nor CF_TLV_BER", tlv_name(l),
            l->kind);
    }
    if (l->kind == CF_TLV_FIXED) {
        if (l->tag_width != 1 && l->tag_width != 2 && l->tag_width != 4) {
            return cf_error_set(err, CF_E_DEFINITION, NULL, l->name,
                                "layout '%s': a tag is 1, 2 or 4 bytes wide, not %u",
                                tlv_name(l), l->tag_width);
        }
        if (l->length_width != 1 && l->length_width != 2 && l->length_width != 4) {
            return cf_error_set(err, CF_E_DEFINITION, NULL, l->name,
                                "layout '%s': a length is 1, 2 or 4 bytes wide, not %u",
                                tlv_name(l), l->length_width);
        }
        if (l->order != CF_TLV_BIG && l->order != CF_TLV_LITTLE) {
            return tlv_bad(err, l, "say the byte order: CF_TLV_BIG or CF_TLV_LITTLE");
        }
    }
    if ((l->flags & ~known) != 0) {
        return cf_error_set(err, CF_E_DEFINITION, NULL, l->name,
                            "layout '%s': flags %u that this library does not know",
                            tlv_name(l), l->flags);
    }
    if ((l->flags & CF_TLV_END_LENGTH) != 0 && (l->flags & CF_TLV_END) == 0) {
        return tlv_bad(err, l, "CF_TLV_END_LENGTH is for an end tag: set CF_TLV_END");
    }
    if ((l->flags & CF_TLV_PADDING) != 0 && l->kind != CF_TLV_BER) {
        return tlv_bad(err, l, "CF_TLV_PADDING is BER-TLV's");
    }
    if ((l->flags & CF_TLV_END) != 0 && !tlv_fits(l, l->end_tag)) {
        tlv_hex(hex, l->end_tag, 4u);
        return cf_error_set(err, CF_E_DEFINITION, NULL, l->name,
                            "layout '%s': the end tag 0x%s is wider than a tag",
                            tlv_name(l), hex);
    }
    if (l->unknown < CF_TLV_UNKNOWN_REFUSE || l->unknown > CF_TLV_UNKNOWN_KEEP) {
        return cf_error_set(err, CF_E_DEFINITION, NULL, l->name,
                            "layout '%s': unknown is %d, not a CF_TLV_UNKNOWN_ value",
                            tlv_name(l), l->unknown);
    }
    if ((l->tags == NULL) != (l->ntags == 0)) {
        return tlv_bad(err, l, "the tags and their count disagree");
    }
    if (!tlv_check_table(l, l->tags, l->ntags, 1, err)) {
        return CF_FALSE;
    }
    if (l->kind == CF_TLV_BER) {
        return tlv_bad(err, l, "BER-TLV is not read yet");
    }
    if (l->unknown != CF_TLV_UNKNOWN_REFUSE) {
        return tlv_bad(err, l, "an unknown tag skipped or kept is not read yet");
    }
    return CF_TRUE;
}

/* ------------------------------------------------------------------ *
 * Reading
 * ------------------------------------------------------------------ */

typedef struct tlv_scan {
    const unsigned char* base;
    const cf_tlv_layout* l;
    const char*          name;
    const cf_doc_sink*   out;
    cf_error*            err;
    char                 where[CF_PATH_MAX + 32];
} tlv_scan;

/* "<name>, byte <n>: <what>" */
static cf_bool tlv_fail(tlv_scan* s, size_t off, const char* what) {
    return cf_error_set(s->err, CF_E_SOURCE, NULL, s->name, "%s, byte %zu: %s", s->name,
                        off, what);
}

/* `n` bytes as one unsigned number in `order`. */
static cf_u64 tlv_number(const unsigned char* p, size_t n, int order) {
    cf_u64 v = 0;
    size_t i;

    for (i = 0; i < n; ++i) {
        v = (v << 8) | (cf_u64)p[(order == CF_TLV_LITTLE) ? n - 1u - i : i];
    }
    return v;
}

/* The tag at `*at`, advancing past it. */
static cf_bool tlv_tag_read(tlv_scan* s, size_t* at, size_t end, cf_u32* tag) {
    size_t w = s->l->tag_width;

    if (end - *at < w) {
        return cf_error_set(s->err, CF_E_SOURCE, NULL, s->name,
                            "%s, byte %zu: a tag cut short: %zu bytes are left",
                            s->name, *at, end - *at);
    }
    *tag = (cf_u32)tlv_number(s->base + *at, w, s->l->order);
    *at += w;
    return CF_TRUE;
}

/* The length at `*at`, advancing past it; `start` is the element's. */
static cf_bool tlv_len_read(tlv_scan* s, size_t start, size_t* at, size_t end,
                            cf_u64* len) {
    size_t w = s->l->length_width;

    if (end - *at < w) {
        return cf_error_set(s->err, CF_E_SOURCE, NULL, s->name,
                            "%s, byte %zu: a length cut short: %zu bytes are left",
                            s->name, start, end - *at);
    }
    *len = tlv_number(s->base + *at, w, s->l->order);
    *at += w;
    return CF_TRUE;
}

/* The entry for `tag` in an ascending table, or NULL. */
static const cf_tlv_tag* tlv_find(const cf_tlv_tag* tags, size_t n, cf_u32 tag) {
    size_t lo = 0, hi = n;

    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2u;
        if (tags[mid].tag == tag) {
            return &tags[mid];
        }
        if (tags[mid].tag < tag) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }
    return NULL;
}

/* One scalar, placed at its tag. */
static cf_bool tlv_emit(tlv_scan* s, size_t start, const char* key, const cf_value* v,
                        cf_bool textual) {
    cf_loc at;

    cf_fmt(s->where, sizeof s->where, "%s, byte %zu", s->name, start);
    at.file = s->where;
    at.line = 0;
    at.col  = 0;
    if (!s->out->scalar(s->out->ctx, key, strlen(key), v, textual, &at)) {
        return cf_error_set(s->err, CF_E_KEY, NULL, s->name,
                            "%s, byte %zu: '%s' is not a valid key", s->name, start,
                            key);
    }
    return CF_TRUE;
}

static cf_bool tlv_text(tlv_scan* s, const cf_tlv_tag* e, const char* hex, size_t start,
                        const unsigned char* p, size_t n) {
    cf_value v;

    while (n > 0 && p[n - 1u] == 0) {
        --n;
    }
    if (memchr(p, 0, n) != NULL) {
        return cf_error_set(s->err, CF_E_SOURCE, NULL, s->name,
                            "%s, byte %zu: tag 0x%s is text with a NUL inside", s->name,
                            start, hex);
    }
    if (!cf_text_is_utf8((const char*)p, n)) {
        return cf_error_set(s->err, CF_E_SOURCE, NULL, s->name,
                            "%s, byte %zu: tag 0x%s is text that is not UTF-8", s->name,
                            start, hex);
    }
    cf_value_text(&v, (const char*)p, n);
    return tlv_emit(s, start, e->key, &v, CF_FALSE);
}

static cf_bool tlv_integer(tlv_scan* s, const cf_tlv_tag* e, const char* hex,
                           size_t start, const unsigned char* p, size_t n) {
    int      order = (e->order != 0) ? e->order : s->l->order;
    cf_u64   u;
    cf_value v;

    if (n < 1u || n > 8u) {
        return cf_error_set(
            s->err, CF_E_SOURCE, NULL, s->name,
            "%s, byte %zu: tag 0x%s is %U bytes, and an integer is 1 to 8", s->name,
            start, hex, (cf_u64)n);
    }
    u = tlv_number(p, n, order);
    if (e->type == CF_TLV_INT) {
        if (n < 8u && (u >> (8u * n - 1u)) != 0) {
            u |= ~(cf_u64)0 << (8u * n); /* sign-extended from the top bit */
        }
        cf_value_int(&v, (cf_i64)u);
    } else {
        if (u > (cf_u64)CF_I64_MAX) {
            return cf_error_set(
                s->err, CF_E_SOURCE, NULL, s->name,
                "%s, byte %zu: tag 0x%s is %U, past the largest integer a "
                "configuration holds",
                s->name, start, hex, u);
        }
        cf_value_int(&v, (cf_i64)u);
    }
    return tlv_emit(s, start, e->key, &v, CF_FALSE);
}

#if CF_WITH_REAL
/* A single or a double by its bit pattern; Infinity and NaN are text, as
   CBOR's are. */
static cf_bool tlv_real(tlv_scan* s, const cf_tlv_tag* e, size_t start,
                        const unsigned char* p, size_t n) {
    int    order = (e->order != 0) ? e->order : s->l->order;
    cf_u64 bits  = tlv_number(p, n, order);
    double d = (n == 4u) ? cf_f64_from_f32_bits((cf_u32)bits) : cf_f64_from_bits(bits);
    cf_value v;

    bits = cf_f64_bits(d);
    if (((bits >> 52) & 0x7FFu) == 0x7FFu) {
        const char* t = "nan";

        if ((bits & (((cf_u64)1 << 52) - 1u)) == 0) {
            t = ((bits >> 63) != 0) ? "-inf" : "inf";
        }
        cf_value_text(&v, t, strlen(t));
        return tlv_emit(s, start, e->key, &v, CF_TRUE);
    }
    cf_value_real(&v, d);
    return tlv_emit(s, start, e->key, &v, CF_FALSE);
}
#endif

/* One element whose entry is `e`: its value is `n` bytes at `p`. */
static cf_bool tlv_element(tlv_scan* s, const cf_tlv_tag* e, cf_u32 tag, size_t start,
                           const unsigned char* p, size_t n) {
    char     hex[9];
    cf_value v;

    tlv_hex(hex, tag, tlv_tag_bytes(s->l, tag));
    if (e == NULL) {
        return cf_error_set(s->err, CF_E_SOURCE, NULL, s->name,
                            "%s, byte %zu: tag 0x%s is not in the layout '%s'", s->name,
                            start, hex, tlv_name(s->l));
    }
    if ((e->flags & CF_TLV_SKIP) != 0) {
        return CF_TRUE; /* the layout knows it, and chose not to read it */
    }
    if (e->type == CF_TLV_RECORD) {
        return cf_error_set(s->err, CF_E_SOURCE, NULL, s->name,
                            "%s, byte %zu: tag 0x%s is a record of %U bytes, which the "
                            "layout does not read",
                            s->name, start, hex, (cf_u64)n);
    }
    if (e->width != 0 && n != e->width) {
        return cf_error_set(s->err, CF_E_SOURCE, NULL, s->name,
                            "%s, byte %zu: tag 0x%s is %U bytes, and its entry says %u",
                            s->name, start, hex, (cf_u64)n, e->width);
    }
    switch (e->type) {
    case CF_TLV_TEXT:
        return tlv_text(s, e, hex, start, p, n);
    case CF_TLV_INT:
    case CF_TLV_UINT:
        return tlv_integer(s, e, hex, start, p, n);
    case CF_TLV_BOOL:
        if (n != 1u) {
            return cf_error_set(s->err, CF_E_SOURCE, NULL, s->name,
                                "%s, byte %zu: tag 0x%s is %U bytes, and a bool is one",
                                s->name, start, hex, (cf_u64)n);
        }
        if (p[0] > 1u) {
            return cf_error_set(s->err, CF_E_SOURCE, NULL, s->name,
                                "%s, byte %zu: tag 0x%s holds %u, and a bool is 0 or 1",
                                s->name, start, hex, (unsigned)p[0]);
        }
        cf_value_bool(&v, (p[0] != 0) ? CF_TRUE : CF_FALSE);
        return tlv_emit(s, start, e->key, &v, CF_FALSE);
#if CF_WITH_REAL
    case CF_TLV_REAL:
        return tlv_real(s, e, start, p, n);
#endif
    default:
        break;
    }
    return tlv_fail(s, start, "a type this reader does not read");
}

/* The elements from `at` to `end` of one table; the top level ends at the
   end tag when the layout has one. */
static cf_bool tlv_seq(tlv_scan* s, size_t at, size_t end, const cf_tlv_tag* tags,
                       size_t n, cf_bool top) {
    const cf_tlv_layout* l = s->l;
    char                 hex[9];

    while (at < end) {
        size_t start = at;
        cf_u32 tag   = 0;
        cf_u64 len   = 0;

        if (!tlv_tag_read(s, &at, end, &tag)) {
            return CF_FALSE;
        }
        if (top && (l->flags & CF_TLV_END) != 0 && tag == l->end_tag) {
            if ((l->flags & CF_TLV_END_LENGTH) != 0) {
                if (!tlv_len_read(s, start, &at, end, &len)) {
                    return CF_FALSE;
                }
                if (len != 0) {
                    return cf_error_set(
                        s->err, CF_E_SOURCE, NULL, s->name,
                        "%s, byte %zu: the end tag has a length of %U; it "
                        "must be 0",
                        s->name, start, len);
                }
            }
            if (at != end) {
                return cf_error_set(s->err, CF_E_SOURCE, NULL, s->name,
                                    "%s, byte %zu: %zu bytes after the end tag",
                                    s->name, at, end - at);
            }
            return CF_TRUE;
        }
        if (!tlv_len_read(s, start, &at, end, &len)) {
            return CF_FALSE;
        }
        if (len > (cf_u64)(end - at)) {
            tlv_hex(hex, tag, tlv_tag_bytes(l, tag));
            return cf_error_set(
                s->err, CF_E_SOURCE, NULL, s->name,
                "%s, byte %zu: tag 0x%s says %U bytes, and %zu are left", s->name,
                start, hex, len, end - at);
        }
        if (!tlv_element(s, tlv_find(tags, n, tag), tag, start, s->base + at,
                         (size_t)len)) {
            return CF_FALSE;
        }
        at += (size_t)len;
    }
    if (top && (l->flags & CF_TLV_END) != 0) {
        tlv_hex(hex, l->end_tag, l->tag_width);
        return cf_error_set(
            s->err, CF_E_SOURCE, NULL, s->name,
            "%s: the document ends at byte %zu without the end tag 0x%s", s->name, end,
            hex);
    }
    return CF_TRUE;
}

static cf_bool tlv_decode(const cf_codec* self, const cf_blob* in,
                          const cf_doc_sink* out, cf_error* err) {
    tlv_scan s;
    size_t   n = (in->p != NULL) ? in->n : 0;

    s.base = (const unsigned char*)in->p;
    s.l    = (const cf_tlv_layout*)self->data;
    s.name = (in->name != NULL) ? in->name : "<tlv>";
    s.out  = out;
    s.err  = err;
    if (n == 0) {
        return cf_error_set(err, CF_E_SOURCE, NULL, s.name, "%s: the document is empty",
                            s.name);
    }
    if (!out->begin_table(out->ctx, NULL, 0)) {
        return cf_error_set(err, CF_E_SOURCE, NULL, s.name, "%s: sink refused", s.name);
    }
    if (!tlv_seq(&s, 0, n, s.l->tags, s.l->ntags, CF_TRUE)) {
        return CF_FALSE;
    }
    if (!out->end(out->ctx)) {
        return cf_error_set(err, CF_E_SOURCE, NULL, s.name,
                            "%s: the document could not be recorded", s.name);
    }
    return CF_TRUE;
}

/* A layout with its size: CF_TLV_LAYOUT_INIT's, or a later one's. */
static cf_bool tlv_sized(const cf_tlv_layout* l) {
    if (l == NULL) {
        return CF_FALSE;
    }
    return (l->size >= offsetof(cf_tlv_layout, ntags) + sizeof(size_t)) ? CF_TRUE
                                                                        : CF_FALSE;
}

cf_bool cf_tlv_codec_sized(cf_codec* out, size_t out_size, const cf_tlv_layout* layout,
                           cf_error* err) {
    if (out == NULL || out_size < offsetof(cf_codec, data) + sizeof(const void*)) {
        return cf_error_set(
            err, CF_E_DEFINITION, NULL, (layout != NULL) ? layout->name : NULL,
            "this cf_codec is %zu bytes, from before 1.5, and has no room "
            "for a layout",
            out_size);
    }
    memset(out, 0, out_size);
    if (!tlv_sized(layout)) {
        return cf_error_set(
            err, CF_E_DEFINITION, NULL, (layout != NULL) ? layout->name : NULL,
            "layout '%s': no layout, or one without its size "
            "(CF_TLV_LAYOUT_INIT)",
            (layout != NULL && layout->name != NULL) ? layout->name : "");
    }
    if (!tlv_check(layout, err)) {
        return CF_FALSE;
    }
    out->size        = (out_size < sizeof *out) ? out_size : sizeof *out;
    out->name        = layout->name;
    out->extensions  = layout->extensions;
    out->caps        = CF_CODEC_READ | CF_CODEC_NAMED_ONLY;
    out->decode_self = tlv_decode;
    out->data        = layout;
    return CF_TRUE;
}

#else /* !CF_WITH_TLV */

cf_bool cf_tlv_codec_sized(cf_codec* out, size_t out_size, const cf_tlv_layout* layout,
                           cf_error* err) {
    (void)out;
    (void)out_size;
    if (layout != NULL && layout->name != NULL) {
        return cf_error_set(err, CF_E_CAPABILITY, NULL, layout->name,
                            "'%s' is a TLV layout, and this build was compiled without "
                            "CF_WITH_TLV",
                            layout->name);
    }
    return cf_error_set(
        err, CF_E_CAPABILITY, NULL, NULL,
        "a TLV layout, and this build was compiled without CF_WITH_TLV");
}

#endif /* CF_WITH_TLV */
