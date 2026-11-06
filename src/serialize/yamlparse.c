/*
 * yamlparse.c - the built-in YAML parser (D-69). C89.
 *
 * YAML 1.2 (yaml.org/spec/1.2.2) read by recursive descent, the way the
 * specification is written: block structure by indentation, flow
 * structure by brackets, and a plain, quoted, alias or flow node taken
 * as a key when ": " follows it on its line. It allocates nothing: a
 * scalar is decoded into one buffer the length of the document, which
 * the sink lends, and the recursion is bounded by the nesting limit.
 * Every event carries its line and column, so explain() names the place
 * a value was written. What a configuration makes of the events - and
 * anchors, which are replayed - is yaml.c's.
 */

#include "configforge/cf_yamlparse.h"

#include <string.h>

#if CF_WITH_YAML && !CF_YAML_LIBFYAML && !CF_YAML_LIBYAML

#define YP_TAGS 8       /* %TAG directives in one document */
#define YP_KEY_MAX 1024 /* an implicit key's length (spec 7.4.2) */
#define YP_DEPTH (CF_MAX_DEPTH * 2 + 8)
#define YP_CORE "tag:yaml.org,2002:"

/* Where a node's block context began: in a document, a mapping value, or
   a sequence entry or an explicit key or value, where a collection may
   start on the same line ("- a: 1", "- - a", "? - a"). */
enum { YP_DOC, YP_VALUE, YP_ENTRY, YP_EXPLICIT };

typedef struct yp_tagdef {
    const char* h;
    size_t      hn;
    const char* pre;
    size_t      pn;
} yp_tagdef;

typedef struct yp {
    const char*         p;
    const char*         end;
    const char*         ls; /* the start of the current line */
    unsigned            line;
    const char*         file;
    cf_error*           err;
    const cf_yp_events* ev;
    char*               buf; /* a scalar, decoded */
    size_t              cap;
    char                tag[256];
    yp_tagdef           tags[YP_TAGS];
    int                 ntags;
    int                 depth;
    int                 flow;
    int                 style; /* the next node's, for the event */
} yp;

/* A node's properties: an anchor in the document, and a tag resolved
   into y->tag until the node is handed over. */
typedef struct yp_props {
    const char* anchor;
    size_t      alen;
    cf_bool     has_tag;
    const char* at; /* where the first property was, or NULL */
} yp_props;

/* ------------------------------------------------------------------ *
 * Characters and places
 * ------------------------------------------------------------------ */

static int yp_c(const yp* y, size_t k) {
    return (y->p + k < y->end) ? (unsigned char)y->p[k] : -1;
}
static int yp_blank(int c) {
    return c == ' ' || c == '\t';
}
static int yp_brk(int c) {
    return c == '\n' || c == '\r';
}
static int yp_blankz(int c) {
    return c < 0 || yp_blank(c) || yp_brk(c);
}
static int yp_flowind(int c) {
    return c == ',' || c == '[' || c == ']' || c == '{' || c == '}';
}

static unsigned yp_col(const yp* y, const char* q) {
    return (unsigned)(q - y->ls) + 1u;
}

static cf_bool yp_fail_at(yp* y, const char* q, const char* what) {
    if (q < y->ls || q > y->end) {
        q = y->p;
    }
    return cf_error_set(y->err, CF_E_SOURCE, NULL, y->file, "%s:%u:%u: %s", y->file,
                        y->line, yp_col(y, q), what);
}

static cf_bool yp_fail(yp* y, const char* what) {
    return yp_fail_at(y, y->p, what);
}

static void yp_eat_break(yp* y) {
    if (*y->p == '\r' && y->p + 1 < y->end && y->p[1] == '\n') {
        y->p += 2;
    } else {
        ++y->p;
    }
    ++y->line;
    y->ls = y->p;
}

static void yp_blanks(yp* y) {
    while (y->p < y->end && yp_blank(*y->p)) {
        ++y->p;
    }
}

/* A comment starts at '#' after white space or at a line's start. */
static int yp_comment_here(const yp* y) {
    return y->p < y->end && *y->p == '#' && (y->p == y->ls || yp_blank(y->p[-1]));
}

/* Past white space, comments and line ends, to the next content. */
static void yp_skip(yp* y) {
    for (;;) {
        yp_blanks(y);
        if (yp_comment_here(y)) {
            while (y->p < y->end && !yp_brk(*y->p)) {
                ++y->p;
            }
        }
        if (y->p < y->end && yp_brk(*y->p)) {
            yp_eat_break(y);
            continue;
        }
        return;
    }
}

/* The rest of a line after a node: white space and a comment, nothing
   else. */
static cf_bool yp_line_rest(yp* y) {
    yp_blanks(y);
    if (yp_comment_here(y)) {
        while (y->p < y->end && !yp_brk(*y->p)) {
            ++y->p;
        }
    }
    if (y->p < y->end && !yp_brk(*y->p)) {
        return yp_fail(y, "unexpected text after the value");
    }
    return CF_TRUE;
}

/* The leading spaces of the current line: its indentation. */
static int yp_indent(const yp* y) {
    const char* q = y->ls;
    while (q < y->end && *q == ' ') {
        ++q;
    }
    return (int)(q - y->ls);
}

/* Only white space between the line's start and here. */
static int yp_fresh(const yp* y) {
    const char* q;
    for (q = y->ls; q < y->p; ++q) {
        if (!yp_blank(*q)) {
            return 0;
        }
    }
    return 1;
}

/* "---" or "..." at a line's start, then white space or the end. */
static int yp_marker(const yp* y) {
    return y->p == y->ls && y->end - y->p >= 3 &&
           ((y->p[0] == '-' && y->p[1] == '-' && y->p[2] == '-') ||
            (y->p[0] == '.' && y->p[1] == '.' && y->p[2] == '.')) &&
           yp_blankz(yp_c(y, 3));
}

/* A byte a scalar may hold: YAML's printable set, by its single bytes
   (a multi-byte character's bytes are all 0x80 and up). */
static int yp_printable(int c) {
    return c == '\t' || (c >= 0x20 && c != 0x7F);
}

/* ------------------------------------------------------------------ *
 * Events
 * ------------------------------------------------------------------ */

static cf_bool yp_emit(yp* y, int kind, const yp_props* pr, const char* text, size_t n,
                       cf_bool plain, const char* at) {
    cf_yp_node node;

    node.kind   = kind;
    node.text   = text;
    node.n      = n;
    node.tag    = (pr != NULL && pr->has_tag) ? y->tag : NULL;
    node.plain  = plain;
    node.style  = (kind == CF_YP_SCALAR && plain) ? ':' : y->style;
    y->style    = 0;
    node.anchor = (pr != NULL) ? pr->anchor : NULL;
    node.alen   = (pr != NULL) ? pr->alen : 0;
    if (pr != NULL && pr->at != NULL && kind != CF_YP_END) {
        at = pr->at;
    }
    node.line = y->line;
    node.col  = yp_col(y, (at >= y->ls && at <= y->end) ? at : y->p);
    return y->ev->node(y->ev->ctx, &node);
}

/* A node with no content: an empty plain scalar, null to the reader. */
static cf_bool yp_empty(yp* y, const yp_props* pr) {
    return yp_emit(y, CF_YP_SCALAR, pr, "", 0, CF_TRUE, y->p);
}

static cf_bool yp_end(yp* y) {
    return yp_emit(y, CF_YP_END, NULL, NULL, 0, CF_FALSE, y->p);
}

/* ------------------------------------------------------------------ *
 * Properties: anchors and tags
 * ------------------------------------------------------------------ */

static int yp_anchor_char(int c) {
    return c >= 0 && !yp_blankz(c) && !yp_flowind(c);
}

/* An anchor's or an alias's name, after its '&' or '*'. */
static size_t yp_name(yp* y) {
    const char* q = y->p;
    while (y->p < y->end && yp_anchor_char((unsigned char)*y->p)) {
        ++y->p;
    }
    return (size_t)(y->p - q);
}

static cf_bool yp_tag_put(yp* y, size_t* n, const char* p, size_t k) {
    size_t i;
    for (i = 0; i < k; ++i) {
        int c = (unsigned char)p[i];
        /* %XX in a tag stands for the byte it names. */
        if (c == '%' && i + 2 < k) {
            int hi =
                cf_is_digit(p[i + 1]) ? p[i + 1] - '0' : (p[i + 1] | 0x20) - 'a' + 10;
            int lo =
                cf_is_digit(p[i + 2]) ? p[i + 2] - '0' : (p[i + 2] | 0x20) - 'a' + 10;
            if (hi < 0 || hi > 15 || lo < 0 || lo > 15) {
                return yp_fail(y, "an escape in a tag is two hexadecimal digits");
            }
            c = hi * 16 + lo;
            i += 2;
        }
        if (*n + 1 >= sizeof y->tag) {
            return yp_fail(y, "the tag is too long");
        }
        y->tag[(*n)++] = (char)c;
    }
    y->tag[*n] = '\0';
    return CF_TRUE;
}

/* A tag at '!', resolved into y->tag: verbatim, the secondary handle's
   core schema, a named handle a %TAG declared, or the primary "!". */
static cf_bool yp_tag(yp* y) {
    const char* start = y->p;
    const char* bang2 = NULL;
    size_t      n     = 0;
    const char* tok;
    size_t      tn;
    const char* h;
    size_t      hn;
    int         i;

    ++y->p;
    if (y->p < y->end && *y->p == '<') {
        const char* q = ++y->p;
        while (y->p < y->end && *y->p != '>' && !yp_blankz((unsigned char)*y->p)) {
            ++y->p;
        }
        if (y->p >= y->end || *y->p != '>' || y->p == q) {
            return yp_fail_at(y, start,
                              "a verbatim tag is !<...> with something inside");
        }
        ++y->p;
        return yp_tag_put(y, &n, q, (size_t)(y->p - 1 - q));
    }
    while (y->p < y->end && !yp_blankz((unsigned char)*y->p) &&
           !(y->flow > 0 && yp_flowind((unsigned char)*y->p))) {
        if (*y->p == '!' && bang2 == NULL) {
            bang2 = y->p;
        }
        ++y->p;
    }
    tok = start;
    tn  = (size_t)(y->p - start);
    for (i = 1; (size_t)i < tn; ++i) {
        if (yp_flowind((unsigned char)tok[i]) || (tok[i] == '!' && tok + i != bang2)) {
            return yp_fail_at(y, start,
                              "a tag cannot hold ',', '[', ']', '{', '}' or '!'");
        }
    }
    if (tn == 1) {
        return yp_tag_put(y, &n, "!", 1); /* the non-specific tag */
    }
    if (bang2 != NULL) {
        h  = tok;
        hn = (size_t)(bang2 - tok) + 1;
    } else {
        h  = "!";
        hn = 1;
    }
    for (i = 0; i < y->ntags; ++i) {
        if (y->tags[i].hn == hn && memcmp(y->tags[i].h, h, hn) == 0) {
            return yp_tag_put(y, &n, y->tags[i].pre, y->tags[i].pn) &&
                   yp_tag_put(y, &n, tok + hn, tn - hn);
        }
    }
    if (hn == 2 && h[1] == '!') {
        if (tn == 2) {
            return yp_fail_at(y, start, "!! names no tag");
        }
        return yp_tag_put(y, &n, YP_CORE, sizeof YP_CORE - 1) &&
               yp_tag_put(y, &n, tok + 2, tn - 2);
    }
    if (hn == 1) {
        return yp_tag_put(y, &n, tok, tn);
    }
    return yp_fail_at(y, start, "the tag's handle is not declared by a TAG directive");
}

/* Properties on this line, in either order, each at most once. */
static cf_bool yp_props_read(yp* y, yp_props* pr) {
    for (;;) {
        int c = yp_c(y, 0);
        if (c == '&') {
            if (pr->anchor != NULL) {
                return yp_fail(y, "a node has one anchor");
            }
            if (pr->at == NULL) {
                pr->at = y->p;
            }
            ++y->p;
            pr->anchor = y->p;
            pr->alen   = yp_name(y);
            if (pr->alen == 0) {
                return yp_fail(y, "an anchor has a name");
            }
        } else if (c == '!') {
            if (pr->has_tag) {
                return yp_fail(y, "a node has one tag");
            }
            if (pr->at == NULL) {
                pr->at = y->p;
            }
            if (!yp_tag(y)) {
                return CF_FALSE;
            }
            pr->has_tag = CF_TRUE;
        } else {
            return CF_TRUE;
        }
        if (y->p < y->end && !yp_blankz((unsigned char)*y->p) &&
            !(y->flow > 0 && yp_flowind((unsigned char)*y->p))) {
            return yp_fail(y, "a property is followed by white space");
        }
        yp_blanks(y);
    }
}

/* ------------------------------------------------------------------ *
 * Scalars
 * ------------------------------------------------------------------ */

static cf_bool yp_put(yp* y, size_t* n, int c) {
    if (*n + 1 >= y->cap) {
        return yp_fail(y, "a value is longer than the document");
    }
    y->buf[(*n)++] = (char)c;
    return CF_TRUE;
}

static cf_bool yp_put_cp(yp* y, size_t* n, cf_u32 cp) {
    if (cp < 0x80u) {
        return yp_put(y, n, (int)cp);
    }
    if (cp < 0x800u) {
        return yp_put(y, n, (int)(0xC0u | (cp >> 6))) &&
               yp_put(y, n, (int)(0x80u | (cp & 0x3Fu)));
    }
    if (cp < 0x10000u) {
        return yp_put(y, n, (int)(0xE0u | (cp >> 12))) &&
               yp_put(y, n, (int)(0x80u | ((cp >> 6) & 0x3Fu))) &&
               yp_put(y, n, (int)(0x80u | (cp & 0x3Fu)));
    }
    return yp_put(y, n, (int)(0xF0u | (cp >> 18))) &&
           yp_put(y, n, (int)(0x80u | ((cp >> 12) & 0x3Fu))) &&
           yp_put(y, n, (int)(0x80u | ((cp >> 6) & 0x3Fu))) &&
           yp_put(y, n, (int)(0x80u | (cp & 0x3Fu)));
}

/* ':' as the value indicator: before white space, or a flow indicator
   in flow - not the first character of ":foo". */
static int yp_colon(const yp* y) {
    int d = yp_c(y, 1);
    return yp_c(y, 0) == ':' && (yp_blankz(d) || (y->flow > 0 && yp_flowind(d)));
}

/* A plain scalar may not start with an indicator, but for '-', '?' and
   ':' before a character that could follow in a plain scalar. */
static int yp_plain_start(const yp* y) {
    int c = yp_c(y, 0);
    int d = yp_c(y, 1);

    if (c < 0 || yp_blankz(c)) {
        return 0;
    }
    if (c == '-' || c == '?' || c == ':') {
        return !yp_blankz(d) && !(y->flow > 0 && yp_flowind(d));
    }
    return strchr(",[]{}#&*!|>'\"%@`", c) == NULL;
}

/* Where a plain scalar's line stops: ": " (": " or ':' before a flow
   indicator in flow), " #", a flow indicator in flow, or the line end. */
static int yp_plain_stop(const yp* y) {
    int c = yp_c(y, 0);
    int d = yp_c(y, 1);

    if (c < 0 || yp_brk(c)) {
        return 1;
    }
    if (c == ':' && (yp_blankz(d) || (y->flow > 0 && yp_flowind(d)))) {
        return 1;
    }
    if (y->flow > 0 && yp_flowind(c)) {
        return 1;
    }
    return 0;
}

/* One line of a plain scalar, decoded; white space at its end dropped. */
static cf_bool yp_plain_line(yp* y, size_t* n) {
    while (!yp_plain_stop(y)) {
        int c = yp_c(y, 0);
        if (yp_blank(c)) {
            const char* q = y->p;
            while (q < y->end && yp_blank(*q)) {
                ++q;
            }
            {
                const char* here = y->p;
                int         stop;
                y->p = q;
                stop = yp_plain_stop(y);
                y->p = here;
                if (q >= y->end || yp_brk(*q) || *q == '#' || stop) {
                    y->p = q; /* trailing white space, before a comment or a stop */
                    return CF_TRUE;
                }
            }
            while (y->p < q) {
                if (!yp_put(y, n, *y->p++)) {
                    return CF_FALSE;
                }
            }
            continue;
        }
        if (!yp_printable(c)) {
            return yp_fail(y, "a value holds a character YAML does not print");
        }
        if (!yp_put(y, n, c)) {
            return CF_FALSE;
        }
        ++y->p;
    }
    return CF_TRUE;
}

/* A plain scalar, perhaps over several lines more indented than n, each
   line break folded to a space and each empty line kept as one. */
static cf_bool yp_plain(yp* y, int n, cf_bool key, const yp_props* pr) {
    const char* at = y->p;
    size_t      k  = 0;
    unsigned    first_line;
    const char* first_ls;

    if (!yp_plain_start(y)) {
        return yp_fail(y, "a value cannot start with this character");
    }
    first_line = y->line;
    first_ls   = y->ls;
    if (!yp_plain_line(y, &k)) {
        return CF_FALSE;
    }
    while (!key && y->p < y->end && yp_brk(*y->p)) {
        const char* save_p    = y->p;
        const char* save_ls   = y->ls;
        unsigned    save_line = y->line;
        int         empty     = 0;

        yp_eat_break(y);
        for (;;) {
            yp_blanks(y);
            if (y->p < y->end && yp_brk(*y->p)) {
                ++empty;
                yp_eat_break(y);
                continue;
            }
            break;
        }
        y->p = y->ls;
        while (y->p < y->end && *y->p == ' ') {
            ++y->p;
        }
        if (y->p >= y->end || yp_marker(y) || (y->flow == 0 && yp_indent(y) <= n) ||
            (yp_blanks(y), y->p >= y->end) || *y->p == '#' || yp_plain_stop(y)) {
            y->p    = save_p;
            y->ls   = save_ls;
            y->line = save_line;
            break;
        }
        if (empty == 0) {
            if (!yp_put(y, &k, ' ')) {
                return CF_FALSE;
            }
        }
        while (empty-- > 0) {
            if (!yp_put(y, &k, '\n')) {
                return CF_FALSE;
            }
        }
        if (!yp_plain_line(y, &k)) {
            return CF_FALSE;
        }
    }
    y->buf[k] = '\0';
    {
        /* The event is placed where the scalar began. */
        const char* now_ls   = y->ls;
        unsigned    now_line = y->line;
        cf_bool     ok;
        y->ls   = first_ls;
        y->line = first_line;
        ok      = yp_emit(y, CF_YP_SCALAR, pr, y->buf, k, CF_TRUE, at);
        y->ls   = now_ls;
        y->line = now_line;
        return ok;
    }
}

/* Folding inside a quoted scalar, at a line break: the break is a space,
   or each empty line after it one line feed; the next line's leading
   white space goes. Its indentation must be more than n's, outside flow. */
static cf_bool yp_quoted_fold(yp* y, size_t* k, int n, cf_bool escaped) {
    int empty = 0;

    yp_eat_break(y);
    for (;;) {
        yp_blanks(y);
        if (y->p < y->end && yp_brk(*y->p)) {
            ++empty;
            yp_eat_break(y);
            continue;
        }
        break;
    }
    if (y->p >= y->end) {
        return yp_fail(y, "a quoted value is not closed");
    }
    {
        const char* q = y->p;
        y->p          = y->ls;
        if (yp_marker(y)) {
            return yp_fail(y, "a document marker inside a quoted value");
        }
        y->p = q;
    }
    if (y->flow == 0 && yp_indent(y) <= n) {
        return yp_fail(y, "a quoted value's next line is not indented enough");
    }
    if (!escaped && empty == 0) {
        return yp_put(y, k, ' ');
    }
    while (empty-- > 0) {
        if (!yp_put(y, k, '\n')) {
            return CF_FALSE;
        }
    }
    return CF_TRUE;
}

/* The double-quoted escapes (spec 5.7). */
static cf_bool yp_escape(yp* y, size_t* k, int n) {
    int    c = yp_c(y, 0);
    cf_u32 cp;
    int    digits = 0;
    int    i;

    switch (c) {
    case '0':
        cp = 0;
        break;
    case 'a':
        cp = 7;
        break;
    case 'b':
        cp = 8;
        break;
    case 't':
    case '\t':
        cp = 9;
        break;
    case 'n':
        cp = 10;
        break;
    case 'v':
        cp = 11;
        break;
    case 'f':
        cp = 12;
        break;
    case 'r':
        cp = 13;
        break;
    case 'e':
        cp = 27;
        break;
    case ' ':
        cp = 32;
        break;
    case '"':
        cp = 34;
        break;
    case '/':
        cp = 47;
        break;
    case '\\':
        cp = 92;
        break;
    case 'N':
        cp = 0x85u;
        break;
    case '_':
        cp = 0xA0u;
        break;
    case 'L':
        cp = 0x2028u;
        break;
    case 'P':
        cp = 0x2029u;
        break;
    case 'x':
        digits = 2;
        cp     = 0;
        break;
    case 'u':
        digits = 4;
        cp     = 0;
        break;
    case 'U':
        digits = 8;
        cp     = 0;
        break;
    case '\n':
    case '\r':
        /* An escaped line break: the lines join with nothing between. */
        return yp_quoted_fold(y, k, n, CF_TRUE);
    default:
        return yp_fail(y, "an unknown escape");
    }
    ++y->p;
    for (i = 0; i < digits; ++i) {
        int d = yp_c(y, 0);
        int v = cf_is_digit(d)           ? d - '0'
                : (d >= 'a' && d <= 'f') ? d - 'a' + 10
                : (d >= 'A' && d <= 'F') ? d - 'A' + 10
                                         : -1;
        if (v < 0) {
            return yp_fail(y, "an escape needs its hexadecimal digits");
        }
        cp = (cp << 4) | (cf_u32)v;
        ++y->p;
    }
    if ((cp >= 0xD800u && cp <= 0xDFFFu) || cp > 0x10FFFFu) {
        return yp_fail(y, "an escape names no character");
    }
    return yp_put_cp(y, k, cp);
}

/* A single- or double-quoted scalar. */
static cf_bool yp_quoted(yp* y, int n, cf_bool key, const yp_props* pr) {
    const char* at         = y->p;
    int         quote      = (unsigned char)*y->p;
    size_t      k          = 0;
    unsigned    first_line = y->line;
    const char* first_ls   = y->ls;
    cf_bool     ok;

    ++y->p;
    for (;;) {
        int c = yp_c(y, 0);
        if (c < 0) {
            return yp_fail(y, "a quoted value is not closed");
        }
        if (c == quote) {
            if (quote == '\'' && yp_c(y, 1) == '\'') {
                if (!yp_put(y, &k, '\'')) {
                    return CF_FALSE;
                }
                y->p += 2;
                continue;
            }
            ++y->p;
            break;
        }
        if (quote == '"' && c == '\\') {
            ++y->p;
            if (!yp_escape(y, &k, n)) {
                return CF_FALSE;
            }
            continue;
        }
        if (yp_blank(c)) {
            const char* q = y->p;
            while (q < y->end && yp_blank(*q)) {
                ++q;
            }
            if (q < y->end && yp_brk(*q)) {
                y->p = q; /* white space before a break goes */
                continue;
            }
            while (y->p < q) {
                if (!yp_put(y, &k, *y->p++)) {
                    return CF_FALSE;
                }
            }
            continue;
        }
        if (yp_brk(c)) {
            if (key) {
                return yp_fail(y, "a key is on one line");
            }
            if (!yp_quoted_fold(y, &k, n, CF_FALSE)) {
                return CF_FALSE;
            }
            continue;
        }
        if (!yp_printable(c)) {
            return yp_fail(y, "a value holds a character YAML does not print");
        }
        if (!yp_put(y, &k, c)) {
            return CF_FALSE;
        }
        ++y->p;
    }
    y->buf[k] = '\0';
    {
        const char* now_ls   = y->ls;
        unsigned    now_line = y->line;
        y->ls                = first_ls;
        y->line              = first_line;
        y->style             = quote;
        ok                   = yp_emit(y, CF_YP_SCALAR, pr, y->buf, k, CF_FALSE, at);
        y->ls                = now_ls;
        y->line              = now_line;
    }
    return ok;
}

/* Line ends and the indentation before a block scalar's next content
   line, the breaks kept in *breaks; the indentation found when it was
   not given. */
static cf_bool yp_block_breaks(yp* y, int least, int* indent, int* breaks,
                               int* max_empty) {
    int max = 0;

    *breaks = 0;
    for (;;) {
        int col;
        while (y->p < y->end && *y->p == ' ' &&
               (*indent < 0 || (int)(y->p - y->ls) < *indent)) {
            ++y->p;
        }
        col = (int)(y->p - y->ls);
        if (col > max) {
            max = col;
        }
        /* A tab short of a known indentation; after it, a tab is content. */
        if (y->p < y->end && *y->p == '\t' && *indent < 0 && col < least) {
            return yp_fail(y, "a tab where the block's indentation is");
        }
        if (y->p < y->end && *y->p == '\t' && *indent >= 0 && col < *indent) {
            const char* q = y->p;
            while (q < y->end && yp_blank(*q)) {
                ++q;
            }
            if (q < y->end && !yp_brk(*q)) {
                return yp_fail(y, "a tab where the block's indentation is");
            }
        }
        if (y->p >= y->end) {
            /* The last line, spaces without a break: an empty line. */
            if (col > 0 && (*indent < 0 || col <= *indent)) {
                if (*indent < 0 && col > *max_empty) {
                    *max_empty = col;
                }
                ++*breaks;
            }
            break;
        }
        if (!yp_brk(*y->p)) {
            break;
        }
        /* An empty line: only white space. */
        if (*indent < 0 && col > *max_empty) {
            *max_empty = col;
        }
        ++*breaks;
        yp_eat_break(y);
    }
    if (*indent < 0) {
        *indent = max;
    }
    return CF_TRUE;
}

/* A literal (|) or folded (>) block scalar, with its chomping and
   indentation indicators, in a block indented n. */
static cf_bool yp_block_scalar(yp* y, int n, const yp_props* pr) {
    const char* at            = y->p;
    unsigned    at_line       = y->line;
    const char* at_ls         = y->ls;
    cf_bool     folded        = (*y->p == '>') ? CF_TRUE : CF_FALSE;
    int         chomp         = 0; /* -1 strip, 0 clip, +1 keep */
    int         inc           = 0;
    int         indent        = -1; /* not known yet */
    int         breaks        = 0;
    int         max_empty     = 0;
    int         leading_break = 0;
    int         leading_blank = 0;
    int         trailing_blank;
    size_t      k = 0;
    int         i;
    cf_bool     ok;

    ++y->p;
    for (i = 0; i < 2; ++i) {
        int c = yp_c(y, 0);
        if ((c == '+' || c == '-') && chomp == 0) {
            chomp = (c == '+') ? 1 : -1;
            ++y->p;
        } else if (c >= '1' && c <= '9' && inc == 0) {
            inc = c - '0';
            ++y->p;
        } else if (c == '0') {
            return yp_fail(y, "a block scalar's indentation is 1 to 9");
        }
    }
    if (y->p < y->end && !yp_blankz((unsigned char)*y->p)) {
        return yp_fail(y, "a block scalar's header is its indicators and a comment");
    }
    if (!yp_line_rest(y)) {
        return CF_FALSE;
    }
    if (y->p < y->end) {
        yp_eat_break(y);
    }
    if (inc > 0) {
        indent = (n >= 0) ? n + inc : inc;
    }
    {
        int given = indent;
        if (!yp_block_breaks(y, n + 1, &indent, &breaks, &max_empty)) {
            return CF_FALSE;
        }
        if (given < 0) {
            if (indent < n + 1) {
                indent = n + 1; /* at the top, n is -1: column 0 is content */
            }
            if (y->p < y->end && !yp_brk(*y->p) && max_empty > (int)(y->p - y->ls)) {
                return yp_fail(y, "an empty line before the content is indented more "
                                  "than the content");
            }
        }
    }
    while ((int)(y->p - y->ls) == indent && y->p < y->end && !yp_marker(y)) {
        trailing_blank = yp_blank((unsigned char)*y->p);
        if (folded && leading_break && !leading_blank && !trailing_blank) {
            if (breaks == 0 && !yp_put(y, &k, ' ')) {
                return CF_FALSE;
            }
        } else if (leading_break && !yp_put(y, &k, '\n')) {
            return CF_FALSE;
        }
        for (; breaks > 0; --breaks) {
            if (!yp_put(y, &k, '\n')) {
                return CF_FALSE;
            }
        }
        leading_blank = yp_blank((unsigned char)*y->p);
        while (y->p < y->end && !yp_brk(*y->p)) {
            if (!yp_printable((unsigned char)*y->p)) {
                return yp_fail(y, "a value holds a character YAML does not print");
            }
            if (!yp_put(y, &k, *y->p++)) {
                return CF_FALSE;
            }
        }
        leading_break = 1; /* the input's end ends the last line too */
        if (y->p >= y->end) {
            break;
        }
        yp_eat_break(y);
        if (!yp_block_breaks(y, n + 1, &indent, &breaks, &max_empty)) {
            return CF_FALSE;
        }
    }
    if (chomp != -1 && leading_break && !yp_put(y, &k, '\n')) {
        return CF_FALSE;
    }
    if (chomp == 1) {
        for (; breaks > 0; --breaks) {
            if (!yp_put(y, &k, '\n')) {
                return CF_FALSE;
            }
        }
    }
    y->buf[k] = '\0';
    {
        const char* now_ls   = y->ls;
        unsigned    now_line = y->line;
        y->ls                = at_ls;
        y->line              = at_line;
        y->style             = folded ? '>' : '|';
        ok                   = yp_emit(y, CF_YP_SCALAR, pr, y->buf, k, CF_FALSE, at);
        y->ls                = now_ls;
        y->line              = now_line;
    }
    return ok;
}

/* ------------------------------------------------------------------ *
 * Whether a key is here
 * ------------------------------------------------------------------ */

/* Past a single-line node without reading it: an alias, a quoted
   scalar, a flow collection or a plain scalar. Returns its end on this
   line, or NULL when it does not end on it. *json says it was quoted or
   a flow collection, after which ':' needs no space in flow. */
static const char* yp_peek_node(const yp* y, const char* q, int* json) {
    const char* e = y->end;
    int         c;

    *json = 0;
    /* properties */
    while (q < e && (*q == '&' || *q == '!')) {
        while (q < e && !yp_blankz((unsigned char)*q) &&
               !(y->flow > 0 && yp_flowind((unsigned char)*q))) {
            ++q;
        }
        while (q < e && yp_blank(*q)) {
            ++q;
        }
    }
    if (q >= e) {
        return q;
    }
    c = (unsigned char)*q;
    if (c == '*') {
        ++q;
        while (q < e && yp_anchor_char((unsigned char)*q)) {
            ++q;
        }
        return q;
    }
    if (c == '"' || c == '\'') {
        ++q;
        while (q < e && !yp_brk(*q)) {
            if (c == '"' && *q == '\\' && q + 1 < e) {
                q += 2;
                continue;
            }
            if (*q == c) {
                if (c == '\'' && q + 1 < e && q[1] == '\'') {
                    q += 2;
                    continue;
                }
                *json = 1;
                return q + 1;
            }
            ++q;
        }
        return NULL;
    }
    if (c == '[' || c == '{') {
        int depth = 0;
        while (q < e && !yp_brk(*q)) {
            if (*q == '[' || *q == '{') {
                ++depth;
            } else if (*q == ']' || *q == '}') {
                if (--depth == 0) {
                    *json = 1;
                    return q + 1;
                }
            } else if (*q == '"' || *q == '\'') {
                int qc = (unsigned char)*q++;
                while (q < e && !yp_brk(*q) && (unsigned char)*q != qc) {
                    q += (qc == '"' && *q == '\\' && q + 1 < e) ? 2 : 1;
                }
                if (q >= e || yp_brk(*q)) {
                    return NULL;
                }
            }
            ++q;
        }
        return NULL;
    }
    /* plain: up to ": ", " #" or the line end */
    while (q < e && !yp_brk(*q)) {
        if (*q == ':' && (q + 1 >= e || yp_blankz((unsigned char)q[1]) ||
                          (y->flow > 0 && yp_flowind((unsigned char)q[1])))) {
            break;
        }
        if (y->flow > 0 && yp_flowind((unsigned char)*q)) {
            break;
        }
        if (*q == '#' && yp_blank(q[-1])) {
            break;
        }
        ++q;
    }
    while (q > y->p && yp_blank(q[-1])) {
        --q;
    }
    return q;
}

/* Whether the node here is an implicit key: ':' after it on its line,
   then white space (or, in flow, after a quoted or flow key, anything). */
static int yp_is_key(const yp* y) {
    int         json;
    const char* q;

    if (y->p < y->end && *y->p == ':' && yp_blankz(yp_c(y, 1))) {
        return 1; /* an empty key */
    }
    q = yp_peek_node(y, y->p, &json);
    if (q == NULL || q - y->p > YP_KEY_MAX) {
        return 0;
    }
    while (q < y->end && yp_blank(*q)) {
        ++q;
    }
    if (q >= y->end || *q != ':') {
        return 0;
    }
    ++q;
    if (q >= y->end || yp_blankz((unsigned char)*q)) {
        return 1;
    }
    return y->flow > 0 && (json || yp_flowind((unsigned char)*q));
}

/* ------------------------------------------------------------------ *
 * Nodes
 * ------------------------------------------------------------------ */

static cf_bool yp_block_node(yp* y, int n, int ctx);
static cf_bool yp_flow_node(yp* y, int n);

static cf_bool yp_deeper(yp* y) {
    if (++y->depth > YP_DEPTH) {
        return yp_fail(y, "nesting is too deep");
    }
    return CF_TRUE;
}

/* An alias, where a node would be. */
static cf_bool yp_alias(yp* y, const yp_props* pr) {
    const char* at = y->p;
    const char* name;
    size_t      nlen;

    if (pr->at != NULL) {
        return yp_fail_at(y, pr->at, "an alias has no properties of its own");
    }
    ++y->p;
    name = y->p;
    nlen = yp_name(y);
    if (nlen == 0) {
        return yp_fail_at(y, at, "an alias has a name");
    }
    return y->ev->alias(y->ev->ctx, name, nlen, y->line, yp_col(y, at));
}

/* In flow: white space, comments and line breaks, each new line more
   indented than the block around the collection. */
static cf_bool yp_flow_skip(yp* y, int n) {
    for (;;) {
        yp_blanks(y);
        if (yp_comment_here(y)) {
            while (y->p < y->end && !yp_brk(*y->p)) {
                ++y->p;
            }
        }
        if (y->p < y->end && yp_brk(*y->p)) {
            yp_eat_break(y);
            if (yp_marker(y)) {
                return yp_fail(y, "a document marker inside a flow collection");
            }
            {
                const char* q = y->p;
                while (q < y->end && yp_blank(*q)) {
                    ++q;
                }
                if (q < y->end && !yp_brk(*q) && *q != '#' && yp_indent(y) <= n &&
                    n >= 0) {
                    return yp_fail(y,
                                   "a flow collection's line is not indented enough");
                }
            }
            continue;
        }
        return CF_TRUE;
    }
}

/* A key and its value inside flow, after an explicit '?' or before an
   implicit ':': the value empty when ',' or the close follows. */
static cf_bool yp_flow_pair_value(yp* y, int n, int close) {
    if (!yp_flow_skip(y, n)) {
        return CF_FALSE;
    }
    if (y->p < y->end && *y->p == ':') {
        ++y->p;
        if (!yp_flow_skip(y, n)) {
            return CF_FALSE;
        }
        if (y->p >= y->end || *y->p == ',' || *y->p == close) {
            return yp_empty(y, NULL);
        }
        return yp_flow_node(y, n);
    }
    return yp_empty(y, NULL);
}

static cf_bool yp_flow_key(yp* y, int n, int close) {
    if (y->p < y->end && *y->p == '?' && yp_blankz(yp_c(y, 1))) {
        ++y->p;
        if (!yp_flow_skip(y, n)) {
            return CF_FALSE;
        }
        if (y->p >= y->end || yp_colon(y) || *y->p == ',' || *y->p == close) {
            return yp_empty(y, NULL);
        }
        return yp_flow_node(y, n);
    }
    if (yp_colon(y)) {
        return yp_empty(y, NULL); /* an empty key */
    }
    return yp_flow_node(y, n);
}

static cf_bool yp_flow_collection(yp* y, int n, const yp_props* pr) {
    int     close = (*y->p == '[') ? ']' : '}';
    cf_bool map   = (close == '}') ? CF_TRUE : CF_FALSE;

    y->style = map ? '{' : '[';
    if (!yp_deeper(y) ||
        !yp_emit(y, map ? CF_YP_MAP : CF_YP_SEQ, pr, NULL, 0, CF_FALSE, y->p)) {
        return CF_FALSE;
    }
    ++y->p;
    ++y->flow;
    for (;;) {
        if (!yp_flow_skip(y, n)) {
            return CF_FALSE;
        }
        if (y->p >= y->end) {
            return yp_fail(y, map ? "a flow mapping is not closed"
                                  : "a flow sequence is not closed");
        }
        if (*y->p == close) {
            ++y->p;
            break;
        }
        if (*y->p == ',') {
            return yp_fail(y, "an entry is missing before ','");
        }
        if (map) {
            if (!yp_flow_key(y, n, close) || !yp_flow_pair_value(y, n, close)) {
                return CF_FALSE;
            }
        } else if ((*y->p == '?' && yp_blankz(yp_c(y, 1))) || yp_is_key(y)) {
            /* a single pair: a mapping of one key, inside the sequence */
            y->style = '{';
            if (!yp_emit(y, CF_YP_MAP, NULL, NULL, 0, CF_FALSE, y->p) ||
                !yp_flow_key(y, n, close) || !yp_flow_pair_value(y, n, close) ||
                !yp_end(y)) {
                return CF_FALSE;
            }
        } else if (!yp_flow_node(y, n)) {
            return CF_FALSE;
        }
        if (!yp_flow_skip(y, n)) {
            return CF_FALSE;
        }
        if (y->p < y->end && *y->p == ',') {
            ++y->p;
            continue;
        }
        if (y->p < y->end && *y->p == close) {
            ++y->p;
            break;
        }
        return yp_fail(y, map ? "expected ',' or '}'" : "expected ',' or ']'");
    }
    --y->flow;
    --y->depth;
    return yp_end(y);
}

/* A node inside flow. */
static cf_bool yp_flow_node(yp* y, int n) {
    yp_props pr;
    int      c;

    memset(&pr, 0, sizeof pr);
    if (!yp_props_read(y, &pr) || !yp_flow_skip(y, n)) {
        return CF_FALSE;
    }
    c = yp_c(y, 0);
    if (c < 0 || c == ',' || c == ']' || c == '}' || yp_colon(y)) {
        return yp_empty(y, &pr);
    }
    if (c == '*') {
        return yp_alias(y, &pr);
    }
    if (c == '[' || c == '{') {
        return yp_flow_collection(y, n, &pr);
    }
    if (c == '"' || c == '\'') {
        return yp_quoted(y, n, CF_FALSE, &pr);
    }
    return yp_plain(y, n, CF_FALSE, &pr);
}

/* A key in a block mapping: properties, then one node on the line. */
static cf_bool yp_block_key(yp* y) {
    yp_props pr;
    int      c;

    memset(&pr, 0, sizeof pr);
    if (!yp_props_read(y, &pr)) {
        return CF_FALSE;
    }
    c = yp_c(y, 0);
    if (yp_colon(y)) {
        return yp_empty(y, &pr);
    }
    if (c == '*') {
        return yp_alias(y, &pr);
    }
    if (c == '[' || c == '{') {
        return yp_flow_collection(y, -1, &pr);
    }
    if (c == '"' || c == '\'') {
        return yp_quoted(y, -1, CF_TRUE, &pr);
    }
    return yp_plain(y, -1, CF_TRUE, &pr);
}

/* A tab in the current line's indentation, before the content here. */
static int yp_tabbed(const yp* y) {
    const char* q;
    for (q = y->ls; q < y->p; ++q) {
        if (*q == '\t') {
            return 1;
        }
    }
    return 0;
}

/* The entries of a block mapping at column m. */
static cf_bool yp_block_map(yp* y, int m, const yp_props* pr) {
    if (!yp_deeper(y) || !yp_emit(y, CF_YP_MAP, pr, NULL, 0, CF_FALSE, y->p)) {
        return CF_FALSE;
    }
    for (;;) {
        if (*y->p == '?' && yp_blankz(yp_c(y, 1))) {
            ++y->p;
            if (!yp_block_node(y, m, YP_EXPLICIT)) {
                return CF_FALSE;
            }
            yp_skip(y);
            if (y->p < y->end && !yp_marker(y) && (int)(y->p - y->ls) == m &&
                *y->p == ':' && yp_blankz(yp_c(y, 1))) {
                ++y->p;
                if (!yp_block_node(y, m, YP_EXPLICIT)) {
                    return CF_FALSE;
                }
            } else if (!yp_empty(y, NULL)) {
                return CF_FALSE;
            }
        } else {
            if (!yp_is_key(y)) {
                return yp_fail(y, "expected a key and ':'");
            }
            if (!yp_block_key(y)) {
                return CF_FALSE;
            }
            yp_blanks(y);
            if (y->p >= y->end || *y->p != ':') {
                return yp_fail(y, "expected ':' after the key");
            }
            ++y->p;
            if (!yp_block_node(y, m, YP_VALUE)) {
                return CF_FALSE;
            }
        }
        yp_skip(y);
        if (y->p >= y->end || yp_marker(y)) {
            break;
        }
        if (yp_indent(y) < m) {
            break;
        }
        if (yp_indent(y) > m || (int)(y->p - y->ls) != m) {
            return yp_fail(y, yp_tabbed(y) ? "a tab cannot indent"
                                           : "this line is indented more than its "
                                             "mapping's keys");
        }
    }
    --y->depth;
    return yp_end(y);
}

/* The entries of a block sequence at column s. */
static cf_bool yp_block_seq(yp* y, int s, const yp_props* pr) {
    if (!yp_deeper(y) || !yp_emit(y, CF_YP_SEQ, pr, NULL, 0, CF_FALSE, y->p)) {
        return CF_FALSE;
    }
    for (;;) {
        ++y->p; /* '-' */
        if (!yp_block_node(y, s, YP_ENTRY)) {
            return CF_FALSE;
        }
        yp_skip(y);
        if (y->p >= y->end || yp_marker(y) || yp_indent(y) < s) {
            break;
        }
        if (yp_indent(y) > s || (int)(y->p - y->ls) != s) {
            return yp_fail(y, yp_tabbed(y) ? "a tab cannot indent"
                                           : "this line is indented more than its "
                                             "sequence's entries");
        }
        if (!(*y->p == '-' && yp_blankz(yp_c(y, 1)))) {
            break; /* the mapping this sequence is the value of goes on */
        }
    }
    --y->depth;
    return yp_end(y);
}

/* A node in block context, below a block indented n. */
/* A block sequence at the indentation of the key or the '?' whose value
   it is, which YAML allows: "key:\n- a". */
static int yp_same_seq(const yp* y, int n, int ctx) {
    return (ctx == YP_VALUE || ctx == YP_EXPLICIT) && yp_indent(y) == n &&
           yp_c(y, 0) == '-' && yp_blankz(yp_c(y, 1));
}

/* Whether no node is here, below a block indented n: the end, a
   document marker, or a line not indented enough. */
static int yp_none_here(const yp* y, int n, int ctx) {
    return y->p >= y->end || yp_marker(y) ||
           (yp_fresh(y) && yp_indent(y) <= n && !yp_same_seq(y, n, ctx));
}

/* A tab between an indicator and a collection on its line. */
static int yp_tab_before(const yp* y) {
    const char* q = y->p;
    while (q > y->ls && yp_blank(q[-1])) {
        if (q[-1] == '\t') {
            return 1;
        }
        --q;
    }
    return 0;
}

static cf_bool yp_block_node(yp* y, int n, int ctx) {
    yp_props    pr;
    int         fresh;
    int         props_inline = 0; /* 1: a key follows; 2: the node does */
    int         compact      = (ctx == YP_ENTRY || ctx == YP_EXPLICIT);
    const char* entry;
    int         c;

    memset(&pr, 0, sizeof pr);
    yp_blanks(y);
    if (y->p >= y->end || yp_brk(*y->p) || yp_comment_here(y)) {
        yp_skip(y);
    }
    if (yp_none_here(y, n, ctx)) {
        return yp_empty(y, &pr);
    }
    fresh = yp_fresh(y);
    entry = y->p;
    /* Properties on lines of their own belong to the node below them; on
       the line of a key, to the key. */
    while (*y->p == '&' || *y->p == '!') {
        if (yp_is_key(y)) {
            props_inline = 1;
            break;
        }
        if (!yp_props_read(y, &pr)) {
            return CF_FALSE;
        }
        if (y->p < y->end && !yp_brk(*y->p) && !yp_comment_here(y)) {
            props_inline = 2;
            break;
        }
        yp_skip(y);
        if (yp_none_here(y, n, ctx)) {
            return yp_empty(y, &pr);
        }
        fresh = 1;
        entry = y->p;
    }
    c = yp_c(y, 0);
    if (c == '-' && yp_blankz(yp_c(y, 1)) && props_inline != 1) {
        if (props_inline == 2 || (!fresh && !compact)) {
            return yp_fail(y, "a block sequence cannot start on this line");
        }
        if ((fresh && yp_tabbed(y)) || (!fresh && yp_tab_before(y))) {
            return yp_fail(y, "a tab cannot indent");
        }
        return yp_block_seq(y, (int)(y->p - y->ls), &pr);
    }
    if (c == '?' && yp_blankz(yp_c(y, 1)) && props_inline != 1) {
        if (props_inline == 2 || (!fresh && !compact)) {
            return yp_fail(y, "a mapping cannot start on this line");
        }
        if (!fresh && yp_tab_before(y)) {
            return yp_fail(y, "a tab cannot indent");
        }
        return yp_block_map(y, (int)(y->p - y->ls), &pr);
    }
    if (props_inline != 2 && yp_is_key(y)) {
        if (!fresh && !compact) {
            return yp_fail(y, "a mapping cannot start on this line: a value "
                              "with ': ' in it is quoted");
        }
        if ((fresh && yp_tabbed(y)) || (!fresh && yp_tab_before(y))) {
            return yp_fail(y, "a tab cannot indent");
        }
        return yp_block_map(y, (int)(entry - y->ls), &pr);
    }
    if (c == '|' || c == '>') {
        return yp_block_scalar(y, n, &pr);
    }
    if (c == '*') {
        if (!yp_alias(y, &pr)) {
            return CF_FALSE;
        }
    } else if (c == '[' || c == '{') {
        if (!yp_flow_collection(y, n, &pr)) {
            return CF_FALSE;
        }
    } else if (c == '"' || c == '\'') {
        if (!yp_quoted(y, n, CF_FALSE, &pr)) {
            return CF_FALSE;
        }
    } else {
        if (!yp_plain(y, n, CF_FALSE, &pr)) {
            return CF_FALSE;
        }
    }
    return yp_line_rest(y);
}

/* ------------------------------------------------------------------ *
 * Documents
 * ------------------------------------------------------------------ */

/* One directive line: %YAML, %TAG, or a reserved one, which is read past. */
static cf_bool yp_directive(yp* y, int* seen_yaml) {
    const char* name = ++y->p;
    size_t      nlen;

    while (y->p < y->end && !yp_blankz((unsigned char)*y->p)) {
        ++y->p;
    }
    nlen = (size_t)(y->p - name);
    yp_blanks(y);
    if (nlen == 4 && memcmp(name, "YAML", 4) == 0) {
        if (*seen_yaml) {
            return yp_fail(y, "a second YAML directive");
        }
        *seen_yaml = 1;
        if (!cf_is_digit(yp_c(y, 0))) {
            return yp_fail(y, "the YAML directive names a version");
        }
        while (cf_is_digit(yp_c(y, 0))) {
            ++y->p;
        }
        if (yp_c(y, 0) != '.' || !cf_is_digit(yp_c(y, 1))) {
            return yp_fail(y, "the YAML directive names a version, as 1.2");
        }
        ++y->p;
        while (cf_is_digit(yp_c(y, 0))) {
            ++y->p;
        }
    } else if (nlen == 3 && memcmp(name, "TAG", 3) == 0) {
        yp_tagdef d;
        int       i;
        d.h = y->p;
        while (y->p < y->end && !yp_blankz((unsigned char)*y->p)) {
            ++y->p;
        }
        d.hn = (size_t)(y->p - d.h);
        if (d.hn == 0 || d.h[0] != '!' || d.h[d.hn - 1] != '!') {
            return yp_fail(y, "the TAG directive names a handle, as !e!");
        }
        yp_blanks(y);
        d.pre = y->p;
        while (y->p < y->end && !yp_blankz((unsigned char)*y->p)) {
            ++y->p;
        }
        d.pn = (size_t)(y->p - d.pre);
        if (d.pn == 0) {
            return yp_fail(y, "the TAG directive names a prefix");
        }
        for (i = 0; i < y->ntags; ++i) {
            if (y->tags[i].hn == d.hn && memcmp(y->tags[i].h, d.h, d.hn) == 0) {
                return yp_fail(y, "a handle declared twice");
            }
        }
        if (y->ntags >= YP_TAGS) {
            return yp_fail(y, "too many TAG directives");
        }
        y->tags[y->ntags++] = d;
    } else {
        while (y->p < y->end && !yp_brk(*y->p)) {
            ++y->p; /* reserved: read past */
        }
    }
    return yp_line_rest(y);
}

cf_bool cf_yaml_parse(const cf_blob* in, const char* file, const cf_yp_events* ev,
                      cf_error* err) {
    yp y;

    memset(&y, 0, sizeof y);
    y.p    = in->p;
    y.end  = in->p + in->n;
    y.ls   = in->p;
    y.line = 1;
    y.file = file;
    y.err  = err;
    y.ev   = ev;
    y.cap  = in->n + 1;
    y.buf  = (ev->work != NULL) ? (char*)ev->work(ev->ctx, y.cap) : NULL;
    if (y.buf == NULL) {
        return cf_error_set(err, CF_E_MEMORY, NULL, file,
                            "%s: no room to read YAML: the reader needs its "
                            "workspace (cf_doc_sink.reserve_work)",
                            file);
    }
    if (in->n >= 3 && (unsigned char)y.p[0] == 0xEF && (unsigned char)y.p[1] == 0xBB &&
        (unsigned char)y.p[2] == 0xBF) {
        y.p += 3;
        y.ls = y.p;
    }
    for (;;) {
        int seen_yaml  = 0;
        int directives = 0;

        y.ntags = 0;
        yp_skip(&y);
        while (y.p < y.end && y.p == y.ls && *y.p == '%') {
            directives = 1;
            if (!yp_directive(&y, &seen_yaml)) {
                return CF_FALSE;
            }
            yp_skip(&y);
        }
        if (y.p >= y.end) {
            if (directives) {
                return yp_fail(&y, "directives without a document after them");
            }
            break;
        }
        if (yp_marker(&y) && *y.p == '.') {
            if (directives) {
                return yp_fail(&y, "directives need '---' after them");
            }
            y.p += 3;
            if (!yp_line_rest(&y)) {
                return CF_FALSE;
            }
            continue;
        }
        if (!yp_marker(&y) && directives) {
            return yp_fail(&y, "directives need '---' after them");
        }
        if (!ev->document(ev->ctx, y.line, yp_col(&y, y.p))) {
            return CF_FALSE;
        }
        if (yp_marker(&y)) {
            y.p += 3;
        }
        if (!yp_block_node(&y, -1, YP_DOC)) {
            return CF_FALSE;
        }
        yp_skip(&y);
        if (y.p >= y.end) {
            break;
        }
        if (yp_marker(&y) && *y.p == '.') {
            y.p += 3;
            if (!yp_line_rest(&y)) {
                return CF_FALSE;
            }
            continue;
        }
        if (yp_marker(&y)) {
            continue;
        }
        return yp_fail(&y, "expected the end of the document");
    }
    return ev->stream_end(ev->ctx, y.line, yp_col(&y, y.p));
}

#else

/* ISO C wants a translation unit to declare something. */
typedef int cf_yamlparse_empty;

#endif /* the built-in YAML parser */
