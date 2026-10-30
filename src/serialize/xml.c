/*
 * xml.c - XML, read through expat. C89. Read only.
 *
 * XML has no canonical mapping onto keys, so this reader declares one
 * (D-23) and refuses what falls outside it by name:
 *
 *   <config port="8080">       the root's name is not part of any key;
 *     <server>                 its attributes and children are the top
 *       <host>a.example</host> level: port, server.host
 *     </server>
 *     <peer>x</peer>           repeated siblings are a list - peer.0,
 *     <peer>y</peer>           peer.1 - and must be next to each other
 *   </config>
 *
 *   attributes and children   one namespace: an attribute and a child
 *                             element of one name are an error
 *   text                      an element holding only text is a value,
 *                             its surrounding whitespace trimmed; text
 *                             beside child elements or attributes is
 *                             mixed content, refused
 *   types                     none: every value is text, typed by its
 *                             definition, as an INI value is (D-24)
 *   DOCTYPE                   refused: entities and external references
 *                             are how XML configuration gets attacked,
 *                             and configuration needs neither
 *   namespaces                refused: a key has no room for a prefix
 *
 * "Repeated siblings are a list" needs to know at the first <peer>
 * whether a second follows, which a streaming parser cannot say. So the
 * text is read twice: the first pass records, for every element, whether
 * it opens, continues or closes a list, has children, attributes or text,
 * and checks the rules above; the second emits the events. The record is
 * one byte an element, in expat's own memory - this library allocates
 * nothing outside its arenas, and expat allocates for itself anyway.
 */

#include "configforge/cf_serialize.h"

#include <string.h>

#if CF_WITH_XML

#include <expat.h>

/* What the first pass learns about each element, in document order. */
enum {
    XML_F_MEMBER = 1u << 0, /* one of a run of same-named siblings  */
    XML_F_FIRST  = 1u << 1, /* the first of that run                */
    XML_F_LAST   = 1u << 2, /* the last of that run                 */
    XML_F_KIDS   = 1u << 3, /* has child elements                   */
    XML_F_ATTRS  = 1u << 4, /* has attributes                       */
    XML_F_TEXT   = 1u << 5  /* holds text other than whitespace     */
};

/* A name seen at one level of the first pass: an attribute of the open
   element, or one of its children. Names are kept in one buffer that,
   like the table, is used as a stack. */
typedef struct xml_name {
    size_t        at; /* offset in the names buffer */
    size_t        n;
    unsigned long element; /* the latest element of this name     */
    unsigned char is_attr;
} xml_name;

typedef struct xml_level {
    size_t        names; /* first entry of this level's table   */
    size_t        chars; /* names buffer offset at this level   */
    long          prev;  /* table entry of the previous child   */
    unsigned long element;
} xml_level;

typedef struct xml_scan {
    XML_Parser         parser;
    const char*        file;
    const cf_doc_sink* out;
    cf_error*          err;
    cf_bool            failed;
    int                depth; /* elements open; 0 before the root */

    /* The record: one byte an element. */
    unsigned char* flags;
    size_t         nflags, capflags;

    /* First pass only: the names seen, as a stack. */
    xml_name* table;
    size_t    ntable, captable;
    char*     names;
    size_t    nnames, capnames;
    xml_level level[CF_MAX_DEPTH + 1];

    /* Second pass only. */
    unsigned long next; /* the next element's number    */
    unsigned char lflags[CF_MAX_DEPTH + 1];
    char*         text; /* the value being collected      */
    size_t        ntext, captext;
    char          key[CF_KEY_MAX]; /* its name, NULL for a member */
    size_t        klen;
    cf_bool       has_key;
    cf_loc        at;
} xml_scan;

/* Record the first failure and stop the parser. */
static void xml_fail(xml_scan* s, const char* what, const char* name) {
    if (s->failed) {
        return;
    }
    s->failed = CF_TRUE;
    cf_error_set(s->err, CF_E_SOURCE, NULL, s->file, "%s:%u:%u: %s%s%s%s", s->file,
                 (unsigned int)XML_GetCurrentLineNumber(s->parser),
                 (unsigned int)XML_GetCurrentColumnNumber(s->parser) + 1u, what,
                 (name != NULL) ? " (" : "", (name != NULL) ? name : "",
                 (name != NULL) ? ")" : "");
    XML_StopParser(s->parser, XML_FALSE);
}

/* A buffer of expat's with room for `want` more items of `size` bytes:
   `buf` itself, or a larger one, or NULL with the failure recorded. */
static void* xml_grow(xml_scan* s, void* buf, size_t* cap, size_t used, size_t want,
                      size_t size) {
    size_t n = (*cap == 0) ? 64 : *cap;
    void*  p;

    if (used + want <= *cap) {
        return buf;
    }
    while (n < used + want) {
        n *= 2;
    }
    p = XML_MemRealloc(s->parser, buf, n * size);
    if (p == NULL) {
        xml_fail(s, "no room to read the document", NULL);
        return NULL;
    }
    *cap = n;
    return p;
}

static cf_bool xml_plain_name(xml_scan* s, const XML_Char* name) {
    if (strchr(name, ':') != NULL) {
        xml_fail(s,
                 "namespaces are not in the XML this library reads: a key has no room "
                 "for a prefix",
                 name);
        return CF_FALSE;
    }
    return CF_TRUE;
}

/* ================================================================== *
 * The first pass: the record
 * ================================================================== */

/* A name at the open level, or -1. */
static long xml_find(const xml_scan* s, const char* name, size_t n) {
    size_t i = s->level[s->depth].names;
    for (; i < s->ntable; ++i) {
        const xml_name* e = &s->table[i];
        if (e->n == n && memcmp(s->names + e->at, name, n) == 0) {
            return (long)i;
        }
    }
    return -1;
}

static cf_bool xml_remember(xml_scan* s, const char* name, size_t n, cf_bool is_attr,
                            unsigned long element) {
    xml_name* e;
    void*     t = xml_grow(s, s->table, &s->captable, s->ntable, 1, sizeof(xml_name));
    void*     c;

    if (t == NULL) {
        return CF_FALSE;
    }
    s->table = (xml_name*)t;
    c        = xml_grow(s, s->names, &s->capnames, s->nnames, n, 1);
    if (c == NULL) {
        return CF_FALSE;
    }
    s->names = (char*)c;
    memcpy(s->names + s->nnames, name, n);
    e          = &s->table[s->ntable++];
    e->at      = s->nnames;
    e->n       = n;
    e->element = element;
    e->is_attr = (unsigned char)(is_attr ? 1 : 0);
    s->nnames += n;
    return CF_TRUE;
}

/* The run the previous child was part of is over. */
static void xml_close_run(xml_scan* s, xml_level* l) {
    if (l->prev >= 0) {
        unsigned long e = s->table[l->prev].element;
        if (s->flags[e] & XML_F_MEMBER) {
            s->flags[e] = (unsigned char)(s->flags[e] | XML_F_LAST);
        }
    }
}

static void XMLCALL xml_scan_start(void* ctx, const XML_Char* name,
                                   const XML_Char** attrs) {
    xml_scan*     s = (xml_scan*)ctx;
    size_t        n = strlen(name);
    unsigned long me;
    xml_level*    l;
    size_t        i;

    if (s->failed || !xml_plain_name(s, name)) {
        return;
    }
    if (s->depth >= CF_MAX_DEPTH) {
        xml_fail(s, "nesting is too deep", name);
        return;
    }
    {
        void* f = xml_grow(s, s->flags, &s->capflags, s->nflags, 1, 1);
        if (f == NULL) {
            return;
        }
        s->flags = (unsigned char*)f;
    }
    me           = (unsigned long)s->nflags++;
    s->flags[me] = 0;

    /* As a child of the open element: a list, a collision, or new. */
    if (s->depth > 0) {
        long       found;
        xml_level* parent = &s->level[s->depth];

        s->flags[parent->element] =
            (unsigned char)(s->flags[parent->element] | XML_F_KIDS);
        found = xml_find(s, name, n);
        if (found >= 0 && s->table[found].is_attr) {
            xml_fail(s,
                     "an attribute and a child element have the same name; one key "
                     "cannot be both",
                     name);
            return;
        }
        if (found >= 0 && found != parent->prev) {
            xml_fail(s,
                     "elements of one name are a list, and a list's elements must be "
                     "next to each other",
                     name);
            return;
        }
        if (found >= 0) {
            unsigned long before = s->table[found].element;
            if (!(s->flags[before] & XML_F_MEMBER)) {
                s->flags[before] =
                    (unsigned char)(s->flags[before] | XML_F_MEMBER | XML_F_FIRST);
            }
            s->flags[me]            = (unsigned char)(s->flags[me] | XML_F_MEMBER);
            s->table[found].element = me;
        } else {
            xml_close_run(s, parent);
            if (!xml_remember(s, name, n, CF_FALSE, me)) {
                return;
            }
            found = (long)s->ntable - 1;
        }
        parent->prev = found;
    }

    ++s->depth;
    l          = &s->level[s->depth];
    l->names   = s->ntable;
    l->chars   = s->nnames;
    l->prev    = -1;
    l->element = me;
    for (i = 0; attrs[i] != NULL; i += 2) {
        if (!xml_plain_name(s, attrs[i]) ||
            !xml_remember(s, attrs[i], strlen(attrs[i]), CF_TRUE, me)) {
            return;
        }
        s->flags[me] = (unsigned char)(s->flags[me] | XML_F_ATTRS);
    }
}

static void XMLCALL xml_scan_end(void* ctx, const XML_Char* name) {
    xml_scan*     s = (xml_scan*)ctx;
    xml_level*    l;
    unsigned char f;

    (void)name;
    if (s->failed || s->depth <= 0) {
        return;
    }
    l = &s->level[s->depth];
    xml_close_run(s, l);
    f = s->flags[l->element];
    if ((f & XML_F_TEXT) && (f & (XML_F_KIDS | XML_F_ATTRS))) {
        xml_fail(s,
                 (f & XML_F_KIDS)
                     ? "text beside child elements: an element holds a value "
                       "or settings, not both"
                     : "text in an element with attributes: an element holds "
                       "a value or settings, not both",
                 name);
        return;
    }
    s->ntable = l->names;
    s->nnames = l->chars;
    --s->depth;
}

static void XMLCALL xml_scan_text(void* ctx, const XML_Char* p, int len) {
    xml_scan* s = (xml_scan*)ctx;
    int       i;

    if (s->failed || s->depth <= 0) {
        return;
    }
    for (i = 0; i < len; ++i) {
        if (!cf_is_space((unsigned char)p[i])) {
            unsigned long e = s->level[s->depth].element;
            s->flags[e]     = (unsigned char)(s->flags[e] | XML_F_TEXT);
            return;
        }
    }
}

static void XMLCALL xml_doctype(void* ctx, const XML_Char* name, const XML_Char* sysid,
                                const XML_Char* pubid, int internal) {
    (void)sysid;
    (void)pubid;
    (void)internal;
    xml_fail((xml_scan*)ctx,
             "a DOCTYPE is not read: entities and external references are how XML "
             "configuration is attacked, and configuration needs neither",
             name);
}

/* ================================================================== *
 * The second pass: the events
 * ================================================================== */

static void xml_here(xml_scan* s) {
    s->at.file = s->file;
    s->at.line = (unsigned int)XML_GetCurrentLineNumber(s->parser);
    s->at.col  = (unsigned int)XML_GetCurrentColumnNumber(s->parser) + 1u;
}

static void xml_sink_failed(xml_scan* s, const char* name) {
    xml_fail(s, "not a valid key", name);
}

static void XMLCALL xml_emit_start(void* ctx, const XML_Char* name,
                                   const XML_Char** attrs) {
    xml_scan*     s = (xml_scan*)ctx;
    size_t        n = strlen(name);
    unsigned char f;
    size_t        i;
    cf_bool       ok = CF_TRUE;

    if (s->failed || s->next >= s->nflags) {
        return;
    }
    f = s->flags[s->next++];
    ++s->depth;
    s->lflags[s->depth] = f;

    if (s->depth == 1) {
        if (f & XML_F_TEXT) {
            xml_fail(s, "the root element holds settings, not a value", name);
            return;
        }
        ok = s->out->begin_table(s->out->ctx, NULL, 0);
    } else {
        /* A list opens at its first element; its elements have no name of
           their own, only a place. */
        if ((f & XML_F_FIRST) && !s->out->begin_array(s->out->ctx, name, n)) {
            xml_sink_failed(s, name);
            return;
        }
        if (f & (XML_F_KIDS | XML_F_ATTRS)) {
            ok = (f & XML_F_MEMBER) ? s->out->begin_table(s->out->ctx, NULL, 0)
                                    : s->out->begin_table(s->out->ctx, name, n);
        } else {
            /* A value: collected until the element ends. */
            if (n >= sizeof s->key) {
                xml_fail(s, "the key is too long", name);
                return;
            }
            memcpy(s->key, name, n);
            s->key[n]  = '\0';
            s->klen    = n;
            s->has_key = (f & XML_F_MEMBER) ? CF_FALSE : CF_TRUE;
            s->ntext   = 0;
            xml_here(s);
            return;
        }
    }
    if (!ok) {
        xml_sink_failed(s, name);
        return;
    }
    xml_here(s);
    for (i = 0; attrs[i] != NULL; i += 2) {
        cf_value v;
        cf_value_text(&v, attrs[i + 1], strlen(attrs[i + 1]));
        if (!s->out->scalar(s->out->ctx, attrs[i], strlen(attrs[i]), &v, CF_TRUE,
                            &s->at)) {
            xml_sink_failed(s, attrs[i]);
            return;
        }
    }
}

static void XMLCALL xml_emit_text(void* ctx, const XML_Char* p, int len) {
    xml_scan* s = (xml_scan*)ctx;
    size_t    n = (len > 0) ? (size_t)len : 0u;

    if (s->failed || s->depth <= 1 ||
        (s->lflags[s->depth] & (XML_F_KIDS | XML_F_ATTRS)) != 0) {
        return;
    }
    {
        void* t = xml_grow(s, s->text, &s->captext, s->ntext, n + 1, 1);
        if (t == NULL) {
            return;
        }
        s->text = (char*)t;
    }
    memcpy(s->text + s->ntext, p, n);
    s->ntext += n;
}

static void XMLCALL xml_emit_end(void* ctx, const XML_Char* name) {
    xml_scan*     s = (xml_scan*)ctx;
    unsigned char f;
    cf_bool       ok;

    if (s->failed || s->depth <= 0) {
        return;
    }
    f = s->lflags[s->depth];
    if (s->depth > 1 && !(f & (XML_F_KIDS | XML_F_ATTRS))) {
        const char* p = (s->text != NULL) ? s->text : "";
        size_t      n = s->ntext;
        cf_value    v;
        while (n > 0 && cf_is_space((unsigned char)p[0])) {
            ++p;
            --n;
        }
        while (n > 0 && cf_is_space((unsigned char)p[n - 1])) {
            --n;
        }
        cf_value_text(&v, p, n);
        ok = s->out->scalar(s->out->ctx, s->has_key ? s->key : NULL,
                            s->has_key ? s->klen : 0, &v, CF_TRUE, &s->at);
    } else {
        ok = s->out->end(s->out->ctx);
    }
    if (ok && (f & XML_F_LAST)) {
        ok = s->out->end(s->out->ctx);
    }
    if (!ok) {
        xml_sink_failed(s, name);
        return;
    }
    --s->depth;
}

/* ================================================================== */

static cf_bool xml_run(xml_scan* s, const cf_blob* in) {
    if (in->n > 0x7FFFFFFFu) {
        return cf_error_set(s->err, CF_E_SOURCE, NULL, s->file,
                            "%s: too large for this XML reader", s->file);
    }
    if (XML_Parse(s->parser, in->p, (int)in->n, XML_TRUE) == XML_STATUS_ERROR &&
        !s->failed) {
        s->failed = CF_TRUE;
        cf_error_set(s->err, CF_E_SOURCE, NULL, s->file, "%s:%u:%u: %s", s->file,
                     (unsigned int)XML_GetCurrentLineNumber(s->parser),
                     (unsigned int)XML_GetCurrentColumnNumber(s->parser) + 1u,
                     XML_ErrorString(XML_GetErrorCode(s->parser)));
    }
    return s->failed ? CF_FALSE : CF_TRUE;
}

static cf_bool xml_decode(const cf_blob* in, const cf_doc_sink* out, cf_error* err) {
    xml_scan   s;
    XML_Parser first;
    cf_bool    ok;

    memset(&s, 0, sizeof s);
    s.file = (in->name != NULL) ? in->name : "<xml>";
    s.out  = out;
    s.err  = err;

    /* The first pass, whose parser also owns the record. */
    first = XML_ParserCreate("UTF-8");
    if (first == NULL) {
        return cf_error_set(err, CF_E_MEMORY, NULL, s.file,
                            "no room for an XML parser");
    }
    s.parser = first;
    XML_SetUserData(first, &s);
    XML_SetElementHandler(first, xml_scan_start, xml_scan_end);
    XML_SetCharacterDataHandler(first, xml_scan_text);
    XML_SetStartDoctypeDeclHandler(first, xml_doctype);
    ok = xml_run(&s, in);

    if (ok && s.nflags == 0) {
        /* expat refuses a document with no element, so this is not
           reached; an empty configuration is an empty root. */
        ok = CF_FALSE;
    }
    if (ok) {
        XML_Parser second = XML_ParserCreate("UTF-8");
        if (second == NULL) {
            ok = cf_error_set(err, CF_E_MEMORY, NULL, s.file,
                              "no room for an XML parser");
        } else {
            s.parser = second;
            s.depth  = 0;
            XML_SetUserData(second, &s);
            XML_SetElementHandler(second, xml_emit_start, xml_emit_end);
            XML_SetCharacterDataHandler(second, xml_emit_text);
            XML_SetStartDoctypeDeclHandler(second, xml_doctype);
            ok = xml_run(&s, in);
            if (s.text != NULL) {
                XML_MemFree(second, s.text);
            }
            XML_ParserFree(second);
        }
    }
    s.parser = first;
    XML_MemFree(first, s.flags);
    XML_MemFree(first, s.table);
    XML_MemFree(first, s.names);
    XML_ParserFree(first);
    return ok;
}

const cf_codec cf_codec_xml = {
    sizeof(cf_codec), "xml", ".xml", CF_CODEC_READ | CF_CODEC_ARRAYS,
    xml_decode,       NULL,  0};

#endif /* CF_WITH_XML */
