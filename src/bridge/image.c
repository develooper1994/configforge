/*
 * image.c - the binary snapshot image. C89.
 *
 * See docs/ImageFormat.md. The short version: the key space is already
 * a sorted array plus a string arena, so the image is a picture of that
 * and loading it is validation followed by pointer arithmetic.
 */

#include "configforge/cf_image.h"

#include <string.h>

#if CF_WITH_IMAGE

#define HDR_SIZE 64
#define IDX_SIZE 16
#define VAL_SIZE 16
#define CHAIN_SIZE 24

/* ------------------------------------------------------------------ *
 * Little-endian accessors
 *
 * Written byte at a time rather than by casting a pointer. Casting
 * would be faster and would also assume both the host's byte order and
 * that the buffer is aligned for the type, neither of which the format
 * is allowed to assume.
 * ------------------------------------------------------------------ */

static void put_u32(char* p, cf_u32 v) {
    p[0] = (char)(v & 0xFFu);
    p[1] = (char)((v >> 8) & 0xFFu);
    p[2] = (char)((v >> 16) & 0xFFu);
    p[3] = (char)((v >> 24) & 0xFFu);
}

static void put_u64(char* p, cf_u64 v) {
    put_u32(p, (cf_u32)(v & (cf_u64)0xFFFFFFFF));
    put_u32(p + 4, (cf_u32)(v >> 32));
}

static cf_u32 get_u32(const char* p) {
    return (cf_u32)(unsigned char)p[0] | ((cf_u32)(unsigned char)p[1] << 8) |
           ((cf_u32)(unsigned char)p[2] << 16) | ((cf_u32)(unsigned char)p[3] << 24);
}

static cf_u64 get_u64(const char* p) {
    return (cf_u64)get_u32(p) | ((cf_u64)get_u32(p + 4) << 32);
}

static cf_u32 get_u16(const char* p) {
    return (cf_u32)(unsigned char)p[0] | ((cf_u32)(unsigned char)p[1] << 8);
}

static void put_u16(char* p, cf_u32 v) {
    p[0] = (char)(v & 0xFFu);
    p[1] = (char)((v >> 8) & 0xFFu);
}

/* A double travels as its bit pattern. memcpy rather than a union or a
   cast: it is the one spelling that is defined on every compiler and
   optimises to the same instruction on all of them. */
#if CF_WITH_REAL
static cf_u64 f64_bits(double d) {
    cf_u64 v = 0;
    memcpy(&v, &d, (sizeof d < sizeof v) ? sizeof d : sizeof v);
    return v;
}

static double bits_f64(cf_u64 v) {
    double d = 0.0;
    memcpy(&d, &v, (sizeof d < sizeof v) ? sizeof d : sizeof v);
    return d;
}
#endif

static size_t align8(size_t n) {
    size_t r = n % 8u;
    return (r == 0) ? n : n + (8u - r);
}

/* ================================================================== *
 * Writing
 * ================================================================== */

typedef struct img_plan {
    size_t count;
    size_t chain_count;
    size_t index_off;
    size_t value_off;
    size_t chain_off;
    size_t string_off;
    size_t string_len;
    size_t total;
} img_plan;

/* The string blob is built by appending, and the same key or value text
   appearing twice is stored twice. Deduplicating would shrink the image
   and would need a hash table at write time; configuration key spaces
   are hundreds of entries, so the table would cost more than it saves.
   Recorded so the next person does not wonder. */
static size_t plan_strings(const cf_keyspace* ks, unsigned flags) {
    size_t n = 0;
    size_t i;

    for (i = 0; i < cf_ks_count(ks); ++i) {
        const cf_entry*     e = cf_ks_at(ks, i);
        const cf_candidate* c;

        if (e->selected == NULL && (flags & CF_IMAGE_PROVENANCE) == 0) {
            continue;
        }
        n += e->keylen + 1;
        if (e->selected != NULL && e->selected->value.type == CF_TYPE_TEXT) {
            n += e->selected->value.u.t.n + 1;
        }
        if ((flags & CF_IMAGE_PROVENANCE) == 0) {
            continue;
        }
        for (c = e->chain; c != NULL; c = c->next) {
            if (c->value.type == CF_TYPE_TEXT) {
                n += c->value.u.t.n + 1;
            }
            if (c->at != NULL) {
                n += strlen(c->at) + 1;
            }
            if (c->reason != NULL) {
                n += strlen(c->reason) + 1;
            }
        }
    }
    return n;
}

static void plan(const cf_keyspace* ks, unsigned flags, img_plan* p) {
    size_t i;

    p->count       = 0;
    p->chain_count = 0;
    for (i = 0; i < cf_ks_count(ks); ++i) {
        const cf_entry*     e = cf_ks_at(ks, i);
        const cf_candidate* c;
        if (e->selected == NULL && (flags & CF_IMAGE_PROVENANCE) == 0) {
            continue;
        }
        ++p->count;
        if ((flags & CF_IMAGE_PROVENANCE) != 0) {
            for (c = e->chain; c != NULL; c = c->next) {
                ++p->chain_count;
            }
        }
    }
    p->index_off  = HDR_SIZE;
    p->value_off  = align8(p->index_off + p->count * IDX_SIZE);
    p->chain_off  = align8(p->value_off + p->count * VAL_SIZE);
    p->string_off = align8(p->chain_off + p->chain_count * CHAIN_SIZE);
    p->string_len = plan_strings(ks, flags);
    p->total      = align8(p->string_off + p->string_len);
}

typedef struct img_w {
    char*           buf;
    size_t          cap;
    size_t          len;     /* what would have been written */
    size_t          strings; /* next free offset in the blob */
    const img_plan* p;
} img_w;

static void w_at(img_w* w, size_t off, const char* p, size_t n) {
    size_t i;
    w->len = (off + n > w->len) ? off + n : w->len;
    if (w->buf == NULL) {
        return;
    }
    for (i = 0; i < n && off + i < w->cap; ++i) {
        w->buf[off + i] = p[i];
    }
}

static void w_u32(img_w* w, size_t off, cf_u32 v) {
    char t[4];
    put_u32(t, v);
    w_at(w, off, t, 4);
}

static void w_u64(img_w* w, size_t off, cf_u64 v) {
    char t[8];
    put_u64(t, v);
    w_at(w, off, t, 8);
}

/* Appends text plus its terminator; returns the offset from the start
   of the image. */
static cf_u32 w_text(img_w* w, const char* p, size_t n) {
    size_t            off = w->p->string_off + w->strings;
    static const char nul = '\0';

    w_at(w, off, p, n);
    w_at(w, off + n, &nul, 1);
    w->strings += n + 1;
    return (cf_u32)off;
}

size_t cf_image_write(const cf_keyspace* ks, const cf_source_view* sv, unsigned flags,
                      char* buf, size_t cap) {
    img_plan p;
    img_w    w;
    size_t   i, ki = 0, ci = 0;

    (void)sv;
    if (ks == NULL) {
        return 0;
    }
    plan(ks, flags, &p);

    w.buf     = buf;
    w.cap     = (buf != NULL) ? cap : 0;
    w.len     = 0;
    w.strings = 0;
    w.p       = &p;

    w_at(&w, 0, "CFGB", 4);
    {
        char t[2];
        put_u16(t, CF_IMAGE_MAJOR);
        w_at(&w, 4, t, 2);
        put_u16(t, CF_IMAGE_MINOR);
        w_at(&w, 6, t, 2);
    }
    w_u32(&w, 8, (cf_u32)flags);
    w_u32(&w, 12, (cf_u32)p.count);
    w_u32(&w, 16, (cf_u32)p.index_off);
    w_u32(&w, 20, (cf_u32)p.value_off);
    w_u32(&w, 24, (cf_u32)p.string_off);
    w_u32(&w, 28, (cf_u32)p.string_len);
    w_u32(&w, 32, (cf_u32)((p.chain_count > 0) ? p.chain_off : 0));
    w_u32(&w, 36, (cf_u32)p.chain_count);
    w_u64(&w, 40, cf_ks_digest(ks));
    w_u32(&w, 48, (cf_u32)p.total);
    {
        char z[12];
        memset(z, 0, sizeof z);
        w_at(&w, 52, z, sizeof z);
    }

    for (i = 0; i < cf_ks_count(ks); ++i) {
        const cf_entry*     e = cf_ks_at(ks, i);
        const cf_candidate* c;
        size_t              idx         = p.index_off + ki * IDX_SIZE;
        size_t              val         = p.value_off + ki * VAL_SIZE;
        cf_u32              first_chain = 0xFFFFFFFFu;

        if (e->selected == NULL && (flags & CF_IMAGE_PROVENANCE) == 0) {
            continue;
        }
        w_u32(&w, idx, w_text(&w, e->key, e->keylen));
        w_u32(&w, idx + 4, (cf_u32)e->keylen);
        w_u32(&w, idx + 8, (cf_u32)ki);

        if (e->selected != NULL) {
            const cf_value* v = &e->selected->value;
            w_at(&w, val, (const char*)"", 0);
            {
                char t = (char)v->type;
                w_at(&w, val, &t, 1);
            }
            {
                char z[3];
                memset(z, 0, 3);
                w_at(&w, val + 1, z, 3);
            }
            switch (v->type) {
            case CF_TYPE_TEXT:
                w_u32(&w, val + 4, (cf_u32)v->u.t.n);
                w_u64(&w, val + 8, (cf_u64)w_text(&w, v->u.t.p, v->u.t.n));
                break;
            case CF_TYPE_INT:
                w_u64(&w, val + 8, (cf_u64)v->u.i);
                break;
            case CF_TYPE_BOOL:
                w_u64(&w, val + 8, v->u.b ? (cf_u64)1 : (cf_u64)0);
                break;
            case CF_TYPE_REAL:
#if CF_WITH_REAL
                w_u64(&w, val + 8, f64_bits(v->u.r));
#endif
                break;
            case CF_TYPE_NULL:
                break;
            }
        } else {
            char t = (char)CF_TYPE_NULL;
            w_at(&w, val, &t, 1);
        }

        if ((flags & CF_IMAGE_PROVENANCE) != 0) {
            for (c = e->chain; c != NULL; c = c->next) {
                size_t ch = p.chain_off + ci * CHAIN_SIZE;
                if (first_chain == 0xFFFFFFFFu) {
                    first_chain = (cf_u32)ci;
                }
                w_u32(&w, ch, (cf_u32)c->source);
                w_u32(&w, ch + 4, (cf_u32)c->outcome);
                {
                    char t = (char)c->value.type;
                    w_at(&w, ch + 8, &t, 1);
                }
                if (c->value.type == CF_TYPE_TEXT) {
                    w_u32(&w, ch + 12, w_text(&w, c->value.u.t.p, c->value.u.t.n));
                } else if (c->value.type == CF_TYPE_INT) {
                    w_u64(&w, ch + 16, (cf_u64)c->value.u.i);
                } else if (c->value.type == CF_TYPE_BOOL) {
                    w_u64(&w, ch + 16, c->value.u.b ? (cf_u64)1 : (cf_u64)0);
                }
#if CF_WITH_REAL
                else if (c->value.type == CF_TYPE_REAL) {
                    w_u64(&w, ch + 16, f64_bits(c->value.u.r));
                }
#endif
                if (c->at != NULL) {
                    w_u32(&w, ch + 20, w_text(&w, c->at, strlen(c->at)));
                } else {
                    w_u32(&w, ch + 20, 0xFFFFFFFFu);
                }
                ++ci;
            }
        }
        w_u32(&w, idx + 12, first_chain);
        ++ki;
    }

    if (w.len < p.total) {
        char z[8];
        memset(z, 0, sizeof z);
        w_at(&w, p.total - 1, z, 1);
    }
    return w.len;
}

/* ================================================================== *
 * Reading
 * ================================================================== */

static cf_bool bad(cf_error* err, const char* what) {
    return cf_error_set(err, CF_E_SOURCE, NULL, "image", "%s", what);
}

/* Does a region of `len` bytes at `off` lie inside `limit`? */
static cf_bool region_fits(cf_u32 off, size_t len, size_t limit) {
    return ((size_t)off <= limit && len <= limit - (size_t)off) ? CF_TRUE : CF_FALSE;
}

cf_bool cf_image_open(cf_image* img, const char* buf, size_t n, cf_error* err) {
    cf_u32 major, total, string_off, string_len, count;
    size_t addr;

    /* Zeroed whole before anything can fail, so an image that did not
       open is an empty one rather than whatever the stack held - which
       a caller ignoring the result would otherwise iterate. */
    if (img != NULL) {
        memset(img, 0, sizeof *img);
    }
    if (img == NULL || buf == NULL) {
        return bad(err, "no image given");
    }
    /* Alignment first. It is a precondition on the caller's buffer
       rather than a property of the content, and reporting the caller's
       mistake ahead of the content's is more useful than the other way
       round: an image read from an odd address fails the magic check
       too, and "not an image" would send the reader looking at the
       file. Dereferencing an unaligned address is undefined on every
       target that cares and silent on the ones that do not. */
    addr = (size_t)buf;
    if ((addr % 8u) != 0) {
        return bad(err, "image buffer is not 8-byte aligned");
    }
    if (n < HDR_SIZE) {
        return bad(err, "image is shorter than its header");
    }
    if (buf[0] != CF_IMAGE_MAGIC_0 || buf[1] != CF_IMAGE_MAGIC_1 ||
        buf[2] != CF_IMAGE_MAGIC_2 || buf[3] != CF_IMAGE_MAGIC_3) {
        return bad(err, "not a ConfigForge image");
    }
    major = get_u16(buf + 4);
    if (major != CF_IMAGE_MAJOR) {
        return cf_error_set(err, CF_E_SOURCE, NULL, "image",
                            "image format %u, this build reads %d", (unsigned)major,
                            CF_IMAGE_MAJOR);
    }

    total      = get_u32(buf + 48);
    count      = get_u32(buf + 12);
    string_off = get_u32(buf + 24);
    string_len = get_u32(buf + 28);

    if (total > n) {
        return cf_error_set(err, CF_E_SOURCE, NULL, "image",
                            "image claims %u bytes, %u given", (unsigned)total,
                            (unsigned)n);
    }
    /* Every offset is checked before anything is read through it. A
       truncated image and a corrupt one look identical from inside, and
       neither may produce a configuration. */
    {
        /* Every region the header describes has to lie inside the image,
           including the chain: a reader added later would otherwise
           inherit an unchecked offset, which is how this kind of bug
           comes back. Subtraction rather than addition, so nothing can
           wrap where size_t is 32 bits. */
        cf_u32 chain_off   = get_u32(buf + 32);
        cf_u32 chain_count = get_u32(buf + 36);
        size_t limit       = total;

        if (!region_fits(get_u32(buf + 16), (size_t)count * IDX_SIZE, limit) ||
            !region_fits(get_u32(buf + 20), (size_t)count * VAL_SIZE, limit) ||
            !region_fits(string_off, string_len, limit) ||
            !region_fits(chain_off, (size_t)chain_count * CHAIN_SIZE, limit)) {
            return bad(err, "image offsets fall outside the image");
        }
        if (chain_count > 0 && chain_off == 0) {
            return bad(err, "image claims a chain and does not say where");
        }
    }

    img->base    = buf;
    img->size    = total;
    img->count   = count;
    img->flags   = get_u32(buf + 8);
    img->digest  = get_u64(buf + 40);
    img->index   = buf + get_u32(buf + 16);
    img->values  = buf + get_u32(buf + 20);
    img->strings = buf + string_off;
    img->swapped = CF_FALSE;
    return CF_TRUE;
}

size_t cf_image_count(const cf_image* img) {
    return (img != NULL) ? (size_t)img->count : 0;
}

static cf_bool read_value(const cf_image* img, const char* rec, cf_value* v) {
    cf_type t = (cf_type)(unsigned char)rec[0];

    switch (t) {
    case CF_TYPE_NULL:
        cf_value_null(v);
        return CF_TRUE;
    case CF_TYPE_BOOL:
        cf_value_bool(v, (get_u64(rec + 8) != 0) ? CF_TRUE : CF_FALSE);
        return CF_TRUE;
    case CF_TYPE_INT:
        cf_value_int(v, (cf_i64)get_u64(rec + 8));
        return CF_TRUE;
    case CF_TYPE_REAL:
#if CF_WITH_REAL
        cf_value_real(v, bits_f64(get_u64(rec + 8)));
        return CF_TRUE;
#else
        return CF_FALSE;
#endif
    case CF_TYPE_TEXT: {
        cf_u32 off = (cf_u32)get_u64(rec + 8);
        cf_u32 len = get_u32(rec + 4);
        /* Written so it cannot overflow where size_t is 32 bits: two
           u32 values and a one can wrap there, and a wrapped bound
           passes every check it was meant to fail. */
        if ((size_t)off > img->size || (size_t)len + 1 > img->size - (size_t)off) {
            return CF_FALSE;
        }
        cf_value_text(v, img->base + off, len);
        return CF_TRUE;
    }
    }
    return CF_FALSE;
}

cf_bool cf_image_at(const cf_image* img, size_t i, const char** key, size_t* klen,
                    cf_value* v) {
    const char* rec;
    cf_u32      off, len;

    if (img == NULL || i >= img->count) {
        return CF_FALSE;
    }
    rec = img->index + i * IDX_SIZE;
    off = get_u32(rec);
    len = get_u32(rec + 4);
    if ((size_t)off > img->size || (size_t)len + 1 > img->size - (size_t)off) {
        return CF_FALSE;
    }
    if (key != NULL) {
        *key = img->base + off;
    }
    if (klen != NULL) {
        *klen = len;
    }
    if (v != NULL) {
        /* The value index comes straight out of the file. The header's
           extents were checked when the image opened, and this was not:
           a corrupt index pointed anywhere, which the mutation fuzzer
           found on its first run. An image is untrusted input - every
           number in it is a claim, not a fact. */
        cf_u32 vi = get_u32(rec + 8);
        if (vi >= img->count) {
            return CF_FALSE;
        }
        return read_value(img, img->values + (size_t)vi * VAL_SIZE, v);
    }
    return CF_TRUE;
}

cf_bool cf_image_get(const cf_image* img, const char* key, size_t klen, cf_value* v) {
    size_t lo = 0, hi;

    if (img == NULL || key == NULL) {
        return CF_FALSE;
    }
    hi = img->count;
    while (lo < hi) {
        size_t      mid = lo + (hi - lo) / 2;
        const char* k;
        size_t      n;
        int         r;

        if (!cf_image_at(img, mid, &k, &n, NULL)) {
            return CF_FALSE;
        }
        r = cf_cmp_bytes(k, n, key, klen);
        if (r == 0) {
            return cf_image_at(img, mid, NULL, NULL, v);
        }
        if (r < 0) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return CF_FALSE;
}

/* Where a value was set before it was baked: the location of the
   winning candidate in the chain, when the image carries one. Without
   this the chain travelled and nothing read it - every value said
   "image" - and the reason to carry it, a device with no filesystem
   still able to say why a value is what it is, was a claim. The string
   is in the image, which outlives the configuration as its text values
   do, and every number on the way to it is checked like every other. */
static const char* image_origin(const cf_image* img, size_t i) {
    cf_u32 chain_off   = get_u32(img->base + 32);
    cf_u32 chain_count = get_u32(img->base + 36);
    cf_u32 first, end;
    size_t j, k;

    if (chain_count == 0) {
        return "image";
    }
    first = get_u32(img->index + i * IDX_SIZE + 12);
    if (first == 0xFFFFFFFFu || first >= chain_count) {
        return "image";
    }
    /* A key's candidates run to where the next key's begin. */
    end = chain_count;
    for (j = i + 1; j < img->count; ++j) {
        cf_u32 f = get_u32(img->index + j * IDX_SIZE + 12);
        if (f != 0xFFFFFFFFu) {
            end = f;
            break;
        }
    }
    if (end > chain_count || end < first) {
        return "image";
    }
    for (k = first; k < end; ++k) {
        const char* rec = img->base + chain_off + k * CHAIN_SIZE;
        if (get_u32(rec + 4) == (cf_u32)CF_OUT_SELECTED) {
            cf_u32 at = get_u32(rec + 20);
            if (at != 0 && (size_t)at < img->size &&
                memchr(img->base + at, '\0', img->size - (size_t)at) != NULL) {
                return img->base + at;
            }
            return "image";
        }
    }
    return "image";
}

cf_bool cf_image_load(const cf_image* img, cf_keyspace* ks, int rank, cf_error* err) {
    size_t i;

    if (img == NULL || ks == NULL) {
        return bad(err, "nothing to load");
    }
    for (i = 0; i < img->count; ++i) {
        const char* key;
        size_t      klen;
        cf_value    v;

        if (!cf_image_at(img, i, &key, &klen, &v)) {
            return bad(err, "image entry is malformed");
        }
        if (!cf_ks_add(ks, key, klen, rank, &v, image_origin(img, i))) {
            return cf_error_set(err, CF_E_MEMORY, key, "image", "no room for '%S'", key,
                                klen);
        }
    }
    return CF_TRUE;
}

#else /* !CF_WITH_IMAGE */

/* Compiled without the image. Every function is still here, so a program
   that uses one links, and each says why it did nothing. */
size_t cf_image_write(const cf_keyspace* ks, const cf_source_view* sv, unsigned flags,
                      char* buf, size_t cap) {
    (void)ks;
    (void)sv;
    (void)flags;
    if (buf != NULL && cap > 0) {
        buf[0] = '\0';
    }
    return 0;
}

cf_bool cf_image_open(cf_image* img, const char* buf, size_t n, cf_error* err) {
    (void)buf;
    (void)n;
    if (img != NULL) {
        memset(img, 0, sizeof *img);
    }
    return cf_error_set(err, CF_E_CAPABILITY, NULL, "image",
                        "this build was compiled without CF_WITH_IMAGE");
}

size_t cf_image_count(const cf_image* img) {
    (void)img;
    return 0;
}

cf_bool cf_image_at(const cf_image* img, size_t i, const char** key, size_t* klen,
                    cf_value* v) {
    (void)img;
    (void)i;
    (void)key;
    (void)klen;
    (void)v;
    return CF_FALSE;
}

cf_bool cf_image_get(const cf_image* img, const char* key, size_t klen, cf_value* v) {
    (void)img;
    (void)key;
    (void)klen;
    (void)v;
    return CF_FALSE;
}

cf_bool cf_image_load(const cf_image* img, cf_keyspace* ks, int rank, cf_error* err) {
    (void)img;
    (void)ks;
    (void)rank;
    return cf_error_set(err, CF_E_CAPABILITY, NULL, "image",
                        "this build was compiled without CF_WITH_IMAGE");
}

#endif /* CF_WITH_IMAGE */

/* ================================================================== *
 * As a source
 * ================================================================== */

typedef struct image_state {
    const char* path; /* NULL when the image is already in memory */
    const char* buf;
    size_t      n;
    cf_error    err;
    cf_bool     failed;
} image_state;

static cf_bool image_load_src(const cf_source* s, cf_keyspace* ks) {
    image_state* st = (image_state*)s->ext;
    cf_image     img;

    st->failed = CF_FALSE;
    cf_error_clear(&st->err);

    if (st->path != NULL) {
        cf_blob blob;
        if (!cf_blob_file(&blob, cf_ks_arena(ks), st->path, &st->err)) {
            st->failed = CF_TRUE;
            return CF_FALSE;
        }
        st->buf = blob.p;
        st->n   = blob.n;
    }
    if (!cf_image_open(&img, st->buf, st->n, &st->err)) {
        st->failed = CF_TRUE;
        return CF_FALSE;
    }
    if (!cf_image_load(&img, ks, s->rank, &st->err)) {
        st->failed = CF_TRUE;
        return CF_FALSE;
    }
    return CF_TRUE;
}

static void image_destroy(cf_source* s) {
    (void)s;
}

static const cf_source_vtable cf_image_vt = {"image", image_load_src, image_destroy,
                                             NULL, NULL};

size_t cf_source_image_state_size(void) {
    return sizeof(image_state);
}

static cf_bool image_source_init(cf_source* s, const char* name, int rank, void* state,
                                 size_t state_size) {
    if (s == NULL || state == NULL || state_size < sizeof(image_state)) {
        return CF_FALSE;
    }
    memset(state, 0, sizeof(image_state));
    ((image_state*)state)->err.size = sizeof(cf_error);

    s->vt   = &cf_image_vt;
    s->name = (name != NULL) ? name : "image";
    s->rank = rank;
    s->ext  = state;
    s->ref  = NULL;
    return CF_TRUE;
}

cf_bool cf_source_image_mem(cf_source* s, const char* name, int rank, const char* buf,
                            size_t n, void* state, size_t state_size) {
    if (!image_source_init(s, name, rank, state, state_size)) {
        return CF_FALSE;
    }
    /* Text values point into this buffer rather than being copied, so
       it has to outlive the configuration. On the target this exists
       for, it is a const array in flash and outlives everything. */
    ((image_state*)state)->buf = buf;
    ((image_state*)state)->n   = n;
    return CF_TRUE;
}

cf_bool cf_source_image_file(cf_source* s, const char* name, int rank, const char* path,
                             void* state, size_t state_size) {
    if (!image_source_init(s, name, rank, state, state_size)) {
        return CF_FALSE;
    }
    ((image_state*)state)->path = path;
    return CF_TRUE;
}

const cf_error* cf_source_image_error(const cf_source* s) {
    const image_state* st;

    if (s == NULL || s->vt != &cf_image_vt || s->ext == NULL) {
        return NULL;
    }
    st = (const image_state*)s->ext;
    return st->failed ? &st->err : NULL;
}
