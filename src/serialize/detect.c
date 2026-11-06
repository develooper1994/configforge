/*
 * detect.c - the format of a document from its name, then its content
 * (D-73). C89.
 *
 * The name's format is taken as it stands: it is not read to be proved,
 * so no file that was read before is read differently. Only when the
 * name gives none does the content decide, and then by reading: every
 * candidate codec reads the bytes into a sink that keeps a hash of the
 * events, and one that accepts them and yields a keyed value is a
 * candidate. A look at the first bytes only orders the candidates, so a
 * wrong first guess costs one more reading and is never trusted.
 */

#include "configforge/cf_serialize.h"

#include <string.h>

/* The builder's own codecs counted, at most. */
#define DETECT_OWN_MAX 16
#define DETECT_ORDER_MAX (DETECT_OWN_MAX + 8)

/* ------------------------------------------------------------------ *
 * The sink that keeps a hash and nothing else
 * ------------------------------------------------------------------ */

typedef struct trial {
    unsigned long h1;
    unsigned long h2;
    size_t        keyed; /* scalars and arrays with a name of their own */
    cf_arena*     ar;
    char*         text;
    size_t        text_cap;
} trial;

static void trial_mix(trial* t, const char* p, size_t n) {
    size_t i;

    for (i = 0; i < n; ++i) {
        unsigned long c = (unsigned long)(unsigned char)p[i];
        t->h1           = ((t->h1 ^ c) * 16777619UL) & 0xFFFFFFFFUL;
        t->h2           = ((t->h2 + c + 1UL) * 2654435761UL) & 0xFFFFFFFFUL;
    }
}

static void trial_event(trial* t, char kind, const char* name, size_t n) {
    trial_mix(t, &kind, 1);
    if (name != NULL) {
        trial_mix(t, name, n);
    }
    trial_mix(t, "", 1); /* a NUL ends the name */
}

static cf_bool trial_table(void* ctx, const char* name, size_t n) {
    trial_event((trial*)ctx, 'T', name, n);
    return CF_TRUE;
}

static cf_bool trial_array(void* ctx, const char* name, size_t n) {
    trial* t = (trial*)ctx;

    trial_event(t, 'A', name, n);
    if (name != NULL) {
        /* `ports = [80, 443]` is a keyed value, though its elements are
           not named. */
        ++t->keyed;
    }
    return CF_TRUE;
}

/* The value as the text it would be keyed by: a format that types it and
   one that does not read the same document the same way. */
static cf_bool trial_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                            cf_bool textual, const cf_loc* at) {
    trial* t = (trial*)ctx;
    char   tmp[CF_VALUE_TEXT_MAX];

    (void)textual;
    (void)at;
    trial_event(t, 'S', name, n);
    if (v->type == CF_TYPE_TEXT) {
        trial_mix(t, v->u.t.p, v->u.t.n);
    } else {
        trial_mix(t, tmp, cf_value_to_text(v, tmp, sizeof tmp));
    }
    if (name != NULL) {
        ++t->keyed;
    }
    return CF_TRUE;
}

static cf_bool trial_end(void* ctx) {
    trial_event((trial*)ctx, 'E', NULL, 0);
    return CF_TRUE;
}

static char* trial_reserve_text(void* ctx, size_t n) {
    trial* t = (trial*)ctx;

    if (n > t->text_cap) {
        size_t cap = (t->text_cap == 0) ? 256 : t->text_cap;
        char*  text;

        while (cap < n) {
            if (cap > (size_t)-1 / 2) {
                cap = n;
                break;
            }
            cap *= 2;
        }
        text = (t->ar != NULL) ? (char*)cf_arena_alloc(t->ar, cap) : NULL;
        if (text == NULL) {
            return NULL;
        }
        t->text     = text;
        t->text_cap = cap;
    }
    return t->text;
}

static void* trial_reserve_work(void* ctx, size_t n) {
    trial* t = (trial*)ctx;
    return (t->ar != NULL) ? cf_arena_alloc(t->ar, n) : NULL;
}

/* Whether `c` reads the bytes into at least one keyed value; the events'
   hash in h1 and h2 when it does. */
static cf_bool trial_read(const cf_codec* c, const cf_blob* in, cf_arena* ar,
                          unsigned long* h1, unsigned long* h2) {
    trial       t;
    cf_doc_sink sink;
    cf_error    err = CF_ERROR_INIT;

    memset(&t, 0, sizeof t);
    t.h1 = 2166136261UL;
    t.h2 = 40503UL;
    t.ar = ar;
    memset(&sink, 0, sizeof sink);
    sink.size         = sizeof sink;
    sink.begin_table  = trial_table;
    sink.begin_array  = trial_array;
    sink.scalar       = trial_scalar;
    sink.end          = trial_end;
    sink.ctx          = &t;
    sink.reserve_text = trial_reserve_text;
    sink.reserve_work = trial_reserve_work;
    if (!c->decode(in, &sink, &err) || t.keyed == 0) {
        return CF_FALSE;
    }
    *h1 = t.h1;
    *h2 = t.h2;
    return CF_TRUE;
}

/* ------------------------------------------------------------------ *
 * The look at the first bytes
 * ------------------------------------------------------------------ */

/* The built-in formats, in the order a first look puts them. The strict
   ones come before the loose ones in each, so a strict format that reads
   the bytes is taken before a loose one is tried. */
static const char* const look_orders[4][6] = {
    {"json", "json5", "toml", "xml", "yaml", "ini"}, /* { or [ of JSON */
    {"xml", "json", "json5", "toml", "yaml", "ini"}, /* <              */
    {"toml", "json", "json5", "xml", "ini", "yaml"}, /* [section], key = */
    {"json", "json5", "toml", "xml", "yaml", "ini"}  /* key: or anything else */
};

static cf_bool is_space(char c) {
    return (c == ' ' || c == '\t' || c == '\r' || c == '\n') ? CF_TRUE : CF_FALSE;
}

/* Which of look_orders the first bytes suggest. */
static int look_at(const char* p, size_t n) {
    size_t i = 0;

    if (n >= 3 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB &&
        (unsigned char)p[2] == 0xBF) {
        i = 3;
    }
    for (;;) {
        while (i < n && is_space(p[i])) {
            ++i;
        }
        if (i < n && (p[i] == '#' || p[i] == ';')) { /* a comment line */
            while (i < n && p[i] != '\n') {
                ++i;
            }
            continue;
        }
        break;
    }
    if (i >= n) {
        return 3;
    }
    if (p[i] == '{') {
        return 0;
    }
    if (p[i] == '<') {
        return 1;
    }
    if (p[i] == '[') {
        ++i;
        while (i < n && (p[i] == ' ' || p[i] == '\t')) {
            ++i;
        }
        if (i < n && (p[i] == '"' || p[i] == '[' || p[i] == '{' || p[i] == '-' ||
                      p[i] == ']' || (p[i] >= '0' && p[i] <= '9'))) {
            return 0;
        }
        return 2;
    }
    for (; i < n && p[i] != '\n'; ++i) {
        if (p[i] == '=') {
            return 2;
        }
        if (p[i] == ':') {
            return 3;
        }
    }
    return 3;
}

/* YAML and INI accept the most, so they are only taken when nothing
   stricter reads the bytes and they agree with each other. */
static cf_bool is_loose(const cf_codec* c) {
    return (c == cf_codec_by_name("yaml") || c == cf_codec_by_name("ini")) ? CF_TRUE
                                                                           : CF_FALSE;
}

static cf_bool can_read(const cf_codec* c) {
    return (c != NULL && (c->caps & CF_CODEC_READ) != 0 && c->decode != NULL)
               ? CF_TRUE
               : CF_FALSE;
}

const cf_codec* cf_codec_detect(const cf_codec* const* own, size_t nown,
                                const cf_blob* in, const char* name, cf_arena* ar,
                                cf_detect* out) {
    cf_detect       local;
    cf_detect*      d = (out != NULL) ? out : &local;
    const cf_codec* order[DETECT_ORDER_MAX];
    const cf_codec* loose = NULL;
    unsigned long   l1 = 0, l2 = 0;
    size_t          n = 0, i;
    int             look;

    memset(d, 0, sizeof *d);
    if (in == NULL) {
        return NULL;
    }
    if (nown > DETECT_OWN_MAX) {
        nown = DETECT_OWN_MAX;
    }

    /* The name: its extension's format, the builder's own first. */
    if (name != NULL) {
        const cf_codec* c = cf_codec_among(own, nown, NULL, name);
        if (c == NULL) {
            c = cf_codec_by_path(name);
        }
        if (can_read(c)) {
            d->how = CF_DETECT_EXTENSION;
            return c;
        }
    }

    /* The content: the builder's own, then the built-in ones as the first
       bytes order them. Properties never, and so not listed. */
    if (in->p == NULL || in->n == 0) {
        return NULL;
    }
    for (i = 0; own != NULL && i < nown; ++i) {
        if (can_read(own[i])) {
            order[n++] = own[i];
        }
    }
    nown = n;
    look = look_at(in->p, in->n);
    for (i = 0; i < 6; ++i) {
        const cf_codec* c = cf_codec_by_name(look_orders[look][i]);
        if (can_read(c)) {
            order[n++] = c;
        }
    }

    for (i = 0; i < n; ++i) {
        unsigned long h1, h2;

        if (!trial_read(order[i], in, ar, &h1, &h2)) {
            continue;
        }
        if (i < nown || !is_loose(order[i])) {
            d->how = CF_DETECT_CONTENT;
            return order[i];
        }
        if (loose == NULL) {
            loose = order[i];
            l1    = h1;
            l2    = h2;
        } else if (h1 != l1 || h2 != l2) {
            d->first  = loose;
            d->second = order[i];
            return NULL;
        }
    }
    if (loose != NULL) {
        d->how = CF_DETECT_CONTENT;
    }
    return loose;
}
