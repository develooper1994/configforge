/*
 * test_roundtrip.c - what a writer writes, its reader reads back.
 *
 * A format that cf_emit writes is held to one promise: what it writes is
 * read back, by the reader of the same format, as the configuration that
 * was written - or the writer refuses, and says which key it could not
 * write. The outcome that is neither, a file that reads back as
 * something else, is what these checks look for, in five places:
 *
 *   atoms    a string at a time, straight into a key space: every
 *            character that is syntax in one of the formats, and the
 *            text that reads like something it is not;
 *   shapes   the documents in which an array and a table look alike;
 *   trees    documents from a generator whose seed is the bug report,
 *            as test_fuzz.c has its mutations;
 *   refusals what a format cannot write, and what is said of it;
 *   emit     the same refusals as the caller of cf_emit meets them.
 *
 * What a format cannot hold is a fact about the format and is listed
 * once, in g_formats. A generated document never asks for it, and a
 * pinned one asks for it to be refused.
 *
 * The key spaces here are written and read below cf_emit, so a value is
 * written as it is. A reference, ${name}, means nothing at that level.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#if CF_WITH_EMIT

#define LEN(a) (sizeof(a) / sizeof((a)[0]))

static int g_fail = 0, g_run = 0, g_shown = 0;

/* `n` bytes of `s` as they can be read in a log: a control byte or a
   byte above ASCII is \xHH, and what does not fit is "...". */
static void esc(char* out, size_t cap, const char* s, size_t n) {
    static const char hex[] = "0123456789abcdef";
    size_t            o     = 0, i;

    for (i = 0; i < n && o + 8 < cap; ++i) {
        unsigned char c = (unsigned char)s[i];

        if (c == '\n') {
            out[o++] = '\\';
            out[o++] = 'n';
        } else if (c == '\r') {
            out[o++] = '\\';
            out[o++] = 'r';
        } else if (c == '\t') {
            out[o++] = '\\';
            out[o++] = 't';
        } else if (c < 0x20 || c >= 0x7f) {
            out[o++] = '\\';
            out[o++] = 'x';
            out[o++] = hex[c >> 4];
            out[o++] = hex[c & 15u];
        } else {
            out[o++] = (char)c;
        }
    }
    if (i < n) {
        out[o++] = '.';
        out[o++] = '.';
        out[o++] = '.';
    }
    out[o] = '\0';
}

static void check(int c, const char* w) {
    ++g_run;
    if (!c) {
        ++g_fail;
        printf("FAIL: %s\n", w);
    }
}

static void eq(const char* got, const char* want, const char* w) {
    ++g_run;
    if (got == NULL || strcmp(got, want) != 0) {
        char a[320], b[320];

        ++g_fail;
        esc(a, sizeof a, got != NULL ? got : "(null)", got != NULL ? strlen(got) : 6u);
        esc(b, sizeof b, want, strlen(want));
        printf("FAIL: %s\n  got  |%s|\n  want |%s|\n", w, a, b);
    }
}

/* A refusal that says which key it could not write. */
static void refused_key(const cf_error* err, const char* quoted, const char* what) {
    ++g_run;
    if (err->code != CF_E_SOURCE || strstr(err->message, quoted) == NULL) {
        ++g_fail;
        printf("FAIL: %s: the refusal is code %d, \"%s\"\n", what, err->code,
               err->message);
    }
}

/* ---------------------------------------------------------------- *
 * The formats
 * ---------------------------------------------------------------- */

/* EXACT: the same keys, the same text and the same types. UNTYPED is a
   format that carries none (D-39): what is read back is given the types
   that inference gives it, and a real may come back as the text of one
   (D-24). TEXT_ONLY is INI, which has no types at all: the same keys
   and the same text, whatever either is called.

   A document a format has no syntax for is refused and not written as
   something else. TOML has no null. INI has no array, and no line break
   in a value: the line ends where the break is and there is no escape. */
enum { EXACT, UNTYPED, TEXT_ONLY };

typedef struct format {
    const char* name;
    int         compare; /* how a read-back is judged       */
    int         arrays; /* syntax for any array (1), or for arrays of values only (2) */
    int         nulls;  /* for a null                      */
    int         lines;  /* and for a line break in a value */
} format;

static const format g_formats[] = {{"json", EXACT, 1, 1, 1},
                                   {"toml", EXACT, 1, 0, 1},
                                   {"yaml", UNTYPED, 1, 1, 1},
                                   {"ini", TEXT_ONLY, 2, 1, 0}};

static const format* g_have[LEN(g_formats)]; /* the ones this build writes */
static size_t        g_nhave;

static const format* format_named(const char* name) {
    size_t i;

    for (i = 0; i < g_nhave; ++i) {
        if (strcmp(g_have[i]->name, name) == 0) {
            return g_have[i];
        }
    }
    return NULL;
}

/* ---------------------------------------------------------------- *
 * Reading, writing and comparing
 * ---------------------------------------------------------------- */

#define OUT_MAX (1 << 16)

static char   g_out[OUT_MAX]; /* what the last writer wrote */
static size_t g_len;          /* and how much of it         */
static char   g_why[240];     /* how a read-back differs    */

/* A char array is not aligned for a struct, as cf_emit's own is not. */
static union {
    cf_i64 align;
    char   b[CF_ENCODER_STATE_MAX];
} g_state;

static cf_bool decode(const char* fmt, const char* text, size_t n, cf_keyspace* ks,
                      cf_error* err) {
    const cf_codec* c = cf_codec_by_name(fmt);
    cf_flatten      fl;
    cf_blob         b;

    if (c == NULL) {
        return CF_FALSE;
    }
    cf_blob_mem(&b, text, n, "doc");
    cf_flatten_init(&fl, ks, 10);
    if (!c->decode(&b, cf_flatten_sink(&fl), err)) {
        return CF_FALSE;
    }
    /* As a file source sees it: a key the flattener could not hold. */
    return (fl.failed || fl.overflow) ? CF_FALSE : CF_TRUE;
}

/* The key space written by `fmt`, into g_out. A value may hold a NUL, so
   the length is kept apart from the text. */
static cf_bool encode(const char* fmt, const cf_keyspace* ks, cf_error* err) {
    const cf_codec* c = cf_codec_by_name(fmt);
    cf_doc_sink     sink;
    cf_sink         out;

    if (c == NULL || c->encoder == NULL || c->encoder_state_size > sizeof g_state.b) {
        return CF_FALSE;
    }
    cf_sink_init(&out, g_out, sizeof g_out);
    if (!c->encoder(&out, g_state.b, &sink)) {
        return CF_FALSE;
    }
    if (!cf_doc_emit(ks, &sink, err)) {
        return CF_FALSE;
    }
    cf_sink_terminate(&out);
    if (cf_sink_overflowed(&out)) {
        return cf_error_set(err, CF_E_SOURCE, NULL, fmt, "the output outgrew the test");
    }
    g_len = out.len;
    return CF_TRUE;
}

static void text_of(const cf_value* v, char* tmp, const char** p, size_t* n) {
    if (v->type == CF_TYPE_TEXT) {
        *p = v->u.t.p;
        *n = v->u.t.n;
    } else {
        *n = cf_value_to_text(v, tmp, CF_VALUE_TEXT_MAX);
        *p = tmp;
    }
}

/* Whether `b` is `a` read back, by the rule of `mode`; and when it is
   not, `why` says where it first differs. A key without a selected
   value is not in the configuration, and is skipped on both sides. */
static int same(const cf_keyspace* a, const cf_keyspace* b, int mode, char* why,
                size_t cap) {
    size_t i = 0, j = 0;

    for (;;) {
        const cf_entry *ea = NULL, *eb = NULL;
        const cf_value *x, *y;
        char            tx[CF_VALUE_TEXT_MAX], ty[CF_VALUE_TEXT_MAX];
        const char *    px, *py;
        size_t          nx, ny;

        while (i < cf_ks_count(a) && cf_ks_at(a, i)->selected == NULL) {
            ++i;
        }
        while (j < cf_ks_count(b) && cf_ks_at(b, j)->selected == NULL) {
            ++j;
        }
        if (i < cf_ks_count(a)) {
            ea = cf_ks_at(a, i);
        }
        if (j < cf_ks_count(b)) {
            eb = cf_ks_at(b, j);
        }
        if (ea == NULL && eb == NULL) {
            return 1;
        }
        if (ea == NULL || eb == NULL || ea->keylen != eb->keylen ||
            memcmp(ea->key, eb->key, ea->keylen) != 0) {
            cf_fmt(why, cap, "the keys differ: '%S' and '%S'",
                   ea != NULL ? ea->key : "(end)", ea != NULL ? ea->keylen : (size_t)5,
                   eb != NULL ? eb->key : "(end)", eb != NULL ? eb->keylen : (size_t)5);
            return 0;
        }
        ++i;
        ++j;
        x = &ea->selected->value;
        y = &eb->selected->value;
        text_of(x, tx, &px, &nx);
        text_of(y, ty, &py, &ny);
        if (nx != ny || memcmp(px, py, nx) != 0) {
            char ex[64], ey[64];

            esc(ex, sizeof ex, px, nx);
            esc(ey, sizeof ey, py, ny);
            cf_fmt(why, cap, "'%S' was %s |%s| and came back %s |%s|", ea->key,
                   ea->keylen, cf_type_name(x->type), ex, cf_type_name(y->type), ey);
            return 0;
        }
        if (mode != TEXT_ONLY && x->type != y->type &&
            !(mode == UNTYPED && x->type == CF_TYPE_REAL && y->type == CF_TYPE_TEXT)) {
            cf_fmt(why, cap, "'%S' was %s and came back %s", ea->key, ea->keylen,
                   cf_type_name(x->type), cf_type_name(y->type));
            return 0;
        }
    }
}

enum { TRIP_SAME, TRIP_REFUSED, TRIP_UNREADABLE, TRIP_DIFFERENT, TRIP_NO_DOC };

static const char* const g_outcome[] = {"read back the same", "refused", "not readable",
                                        "read back different",
                                        "not a document the JSON reader takes"};

/* `ks` written by `f` and read back by it. What was written is left in
   g_out and g_len; what the writer or the reader said, in `err`; and how
   a read-back differs, in g_why. */
static int trip(const format* f, const cf_keyspace* ks, cf_error* err) {
    cf_arena*   ar;
    cf_keyspace back;
    int         result = TRIP_SAME;

    cf_error_clear(err);
    g_len    = 0;
    g_out[0] = '\0';
    g_why[0] = '\0';
    if (!encode(f->name, ks, err)) {
        return TRIP_REFUSED;
    }
    ar = cf_arena_new(NULL, 1 << 16);
    check(ar != NULL, "an arena for the read-back");
    if (ar == NULL) {
        return TRIP_UNREADABLE;
    }
    cf_ks_init(&back, ar, 128);
    if (!decode(f->name, g_out, g_len, &back, err)) {
        result = TRIP_UNREADABLE;
    } else {
        cf_ks_resolve(&back);
        if (f->compare == UNTYPED) {
            cf_ks_infer_untyped(&back);
        }
        if (!same(ks, &back, f->compare, g_why, sizeof g_why)) {
            result = TRIP_DIFFERENT;
        }
    }
    cf_arena_free(ar);
    return result;
}

/* The outcome of a trip against the one wanted. A failure is shown in
   full - what was written, and how it came back - for the first dozen;
   the rest are counted, so that a regression is a page and not a
   thousand. */
static void expect(int got, int want, const format* f, const char* what,
                   const cf_error* err) {
    static char shown[1024], text[512];

    ++g_run;
    if (got == want) {
        return;
    }
    ++g_fail;
    if (++g_shown > 12) {
        return;
    }
    esc(shown, sizeof shown, what, strlen(what));
    printf("FAIL: %s through %s: %s, and should be %s\n", shown, f->name,
           g_outcome[got], g_outcome[want]);
    if (got == TRIP_REFUSED || got == TRIP_UNREADABLE || got == TRIP_NO_DOC) {
        printf("  %s\n", err->message);
    } else if (got == TRIP_DIFFERENT) {
        printf("  %s\n", g_why);
    }
    if (got != TRIP_REFUSED && got != TRIP_NO_DOC) {
        esc(text, sizeof text, g_out, g_len);
        printf("  written: %s\n", text);
    }
}

/* A JSON document, in a format that can hold it, comes back from it as it
   was. The outcome is what the document did, and `err` what was said. */
static int trip_doc(const format* f, const char* doc, cf_error* err) {
    cf_arena*   ar = cf_arena_new(NULL, 1 << 16);
    cf_keyspace ks;
    int         got;

    if (ar == NULL) {
        check(0, "an arena");
        return TRIP_UNREADABLE;
    }
    cf_ks_init(&ks, ar, 128);
    cf_error_clear(err);
    if (decode("json", doc, strlen(doc), &ks, err)) {
        cf_ks_resolve(&ks);
        got = trip(f, &ks, err);
    } else {
        got = TRIP_NO_DOC;
    }
    cf_arena_free(ar);
    return got;
}

static void round_trip_doc(const format* f, const char* doc, const char* what) {
    cf_error err = CF_ERROR_INIT;

    expect(trip_doc(f, doc, &err), TRIP_SAME, f, what, &err);
}

/* A document at the limit of what a format holds is written and read
   back, or it is refused. What it is never is read back as something
   else. */
static void expect_holds(int got, const format* f, const char* what,
                         const cf_error* err) {
    expect((got == TRIP_REFUSED) ? TRIP_SAME : got, TRIP_SAME, f, what, err);
}

/* What the JSON writer writes for `doc` as `fmt` read it. Nothing types
   what a text format gives, so a number in INI is its text. */
static void written_from(const char* fmt, const char* doc, const char* want,
                         const char* what) {
    cf_arena*   ar  = cf_arena_new(NULL, 1 << 16);
    cf_error    err = CF_ERROR_INIT;
    cf_keyspace ks;

    if (ar == NULL) {
        check(0, "an arena");
        return;
    }
    cf_ks_init(&ks, ar, 128);
    if (decode(fmt, doc, strlen(doc), &ks, &err)) {
        cf_ks_resolve(&ks);
        if (encode("json", &ks, &err)) {
            eq(g_out, want, what);
        } else {
            check(0, what);
            printf("  %s\n", err.message);
        }
    } else {
        check(0, what);
        printf("  the document does not read: %s\n", err.message);
    }
    cf_arena_free(ar);
}

/* ---------------------------------------------------------------- *
 * Atoms
 * ---------------------------------------------------------------- */

typedef struct atom {
    const char* p;
    size_t      n;
} atom;

/* clang-format off */
#define ATOM(s) {s, sizeof(s) - 1}
/* clang-format on */

/* One string at a time: the characters that are syntax in one of the
   formats, the text that is not what it looks like, and what is not
   ASCII. A literal is split where a hex escape would take the letter
   after it. All of them are valid UTF-8, so all of them can be written
   by every format; and a reference is written as it is, because nothing
   here reads one. */
static const atom atoms[] = {
    /* where the blanks are */
    ATOM(""), ATOM("a"), ATOM("two words"), ATOM(" lead"), ATOM("trail "), ATOM("  "),
    ATOM("a  b"), ATOM("x\ty"), ATOM("\t"),
    /* what reads as something else */
    ATOM("true"), ATOM("false"), ATOM("null"), ATOM("~"), ATOM("yes"), ATOM("no"),
    ATOM("on"), ATOM("1"), ATOM("-1"), ATOM("+1"), ATOM("007"), ATOM("1.5"),
    ATOM("1e3"), ATOM(".5"), ATOM("0x1F"), ATOM(".inf"), ATOM("nan"),
    ATOM("2001-02-03"), ATOM("12:30:00"),
    /* what is syntax in one of them */
    ATOM("a: b"), ATOM("a:b"), ATOM(":"), ATOM("- x"), ATOM("-"), ATOM("---"),
    ATOM("? x"), ATOM("# no"), ATOM("a #b"), ATOM("a#b"), ATOM("a;b"), ATOM("[x]"),
    ATOM("{y}"), ATOM(","), ATOM("="), ATOM("a=b"), ATOM("k = v"), ATOM("[sec]"),
    ATOM("&anchor"), ATOM("*alias"), ATOM("!tag"), ATOM("%dir"), ATOM("@at"), ATOM("|"),
    ATOM(">"),
    /* quotes and escapes */
    ATOM("\""), ATOM("'"), ATOM("it's"), ATOM("say \"hi\""), ATOM("a\"b"), ATOM("''"),
    ATOM("'a'"), ATOM("\"a\""), ATOM("\\"), ATOM("C:\\dir"), ATOM("\\u0041"),
    ATOM("\\x41"), ATOM("\\N"), ATOM("\\_"),
    /* line breaks, which INI cannot write */
    ATOM("line1\nline2"), ATOM("cr\rlf"), ATOM("\n"), ATOM("\n\n"), ATOM("trail\n"),
    ATOM("\r"), ATOM("lead\r"), ATOM("a\r\nb"),
    /* control characters, and NUL */
    ATOM("\x01\x02"), ATOM("\x0b"), ATOM("\x0c"), ATOM("\x1b"),
    ATOM("a\x0b"
         "b"),
    ATOM("\x7f"),
    ATOM("a\x7f"
         "b"),
    ATOM("a\0b"), ATOM("\0"), ATOM("a\0"),
    /* not ASCII: where YAML's idea of a printable character ends */
    ATOM("caf\xc3\xa9"), ATOM("\xe2\x98\x83"), ATOM("\xf0\x9f\x98\x80"),
    ATOM("\xc2\x80"), ATOM("\xc2\x84"), ATOM("\xc2\x85"), ATOM("\xc2\x86"),
    ATOM("\xc2\x9f"), ATOM("\xc2\xa0"),
    ATOM("a\xc2\xa0"
         "b"),
    ATOM("\xc2\x85nel"), ATOM("\xc2\x85\xc2\x85"), ATOM("a\xc2\x85"),
    ATOM("\xc2\x85"
         "a"),
    ATOM("a\xc2\x85"
         "b\xc2\x85"
         "c"),
    ATOM("\xc2\x9f"
         "x"),
    ATOM("x\xc2\x80"), ATOM("\xef\xbb\xbf"),
    ATOM("x\xef\xbb\xbf"
         "y"),
    ATOM("\xef\xbf\xbd"), ATOM("\xef\xbf\xbe"), ATOM("\xef\xbf\xbf"),
    ATOM("\xf4\x8f\xbf\xbf"), ATOM("\xf0\x90\x80\x80"), ATOM("\xed\x9f\xbf"),
    ATOM("\xee\x80\x80"), ATOM("\xe2\x80\xa8"),
    ATOM("a\xe2\x80\xa8"
         "ls"),
    ATOM("\xe2\x80\xa9"),
    ATOM("a\xe2\x80\xa9"
         "ps"),
    ATOM("\xe2\x80\x8b"), ATOM("\xe2\x80\xae"), ATOM("\xcc\x81"), ATOM("e\xcc\x81"),
    ATOM("\xe3\x80\x80"), ATOM("\xc2\xad"),
    /* references, which nothing here reads */
    ATOM("${ref}"), ATOM("a${"), ATOM("$HOME"), ATOM("$${x}")};

/* A key space of the one key, `k`, holding what somebody wrote. */
static void atom_space(cf_keyspace* ks, cf_arena* ar, const char* p, size_t n) {
    cf_value v;

    cf_ks_init(ks, ar, 8);
    cf_value_text(&v, p, n);
    cf_ks_add_authored(ks, "k", 1, 10, &v, NULL);
    cf_ks_resolve(ks);
}

static int has_newline(const atom* a) {
    return memchr(a->p, '\n', a->n) != NULL;
}

/* One string through every format: written and read back the same, or
   refused by the format that has no way to write it, with the key it
   could not write. */
static void atom_trip(const char* p, size_t n) {
    size_t f;
    char   label[96];

    esc(label, sizeof label, p, n);
    for (f = 0; f < g_nhave; ++f) {
        const format* fm  = g_have[f];
        cf_arena*     ar  = cf_arena_new(NULL, 1 << 14);
        cf_error      err = CF_ERROR_INIT;
        cf_keyspace   ks;
        int           got;

        if (ar == NULL) {
            check(0, "an arena");
            return;
        }
        atom_space(&ks, ar, p, n);
        got = trip(fm, &ks, &err);
        if (!fm->lines && memchr(p, '\n', n) != NULL) {
            expect(got, TRIP_REFUSED, fm, label, &err);
            if (got == TRIP_REFUSED) {
                refused_key(&err, "'k'", label);
            }
        } else {
            expect(got, TRIP_SAME, fm, label, &err);
        }
        cf_arena_free(ar);
    }
}

static void test_atoms(void) {
    size_t i;

    for (i = 0; i < LEN(atoms); ++i) {
        atom_trip(atoms[i].p, atoms[i].n);
    }
}

/* The UTF-8 of a code point, and how many bytes it is. */
static size_t utf8_put(char* out, cf_u32 cp) {
    if (cp < 0x80u) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800u) {
        out[0] = (char)(0xC0u | (cp >> 6));
        out[1] = (char)(0x80u | (cp & 0x3Fu));
        return 2;
    }
    if (cp < 0x10000u) {
        out[0] = (char)(0xE0u | (cp >> 12));
        out[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[2] = (char)(0x80u | (cp & 0x3Fu));
        return 3;
    }
    out[0] = (char)(0xF0u | (cp >> 18));
    out[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
    out[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
    out[3] = (char)(0x80u | (cp & 0x3Fu));
    return 4;
}

/* Every code point to U+00FF, which is where the controls are, and the
   ones above it at which an encoding changes its mind: a length, a
   range, a line separator, a byte order mark, a non-character. Each is
   alone, at the start, at the end and between two letters, because a
   format treats the ends of a value differently from its middle. */
static void test_code_points(void) {
    static const cf_u32 more[] = {0x7FFu,  0x800u,   0xFFFu,   0x2028u, 0x2029u,
                                  0xD7FFu, 0xE000u,  0xFEFFu,  0xFFFDu, 0xFFFEu,
                                  0xFFFFu, 0x10000u, 0x10FFFFu};
    size_t              k;

    for (k = 0; k < 0x100u + LEN(more); ++k) {
        cf_u32 cp = (k < 0x100u) ? (cf_u32)k : more[k - 0x100u];
        char   one[4], buf[8];
        size_t n = utf8_put(one, cp);

        atom_trip(one, n);

        buf[0] = 'a';
        memcpy(buf + 1, one, n);
        atom_trip(buf, n + 1);

        memcpy(buf, one, n);
        buf[n] = 'b';
        atom_trip(buf, n + 1);

        buf[0] = 'a';
        memcpy(buf + 1, one, n);
        buf[n + 1] = 'b';
        atom_trip(buf, n + 2);
    }
}

typedef struct pin {
    atom        value;
    const char* line;
} pin;

/* What INI writes for a value, exactly. The reader takes the blanks and
   carriage returns at the ends of a value to be the line's and a pair of
   quotes round all of it to be quoting; the writer quotes a value that
   would be read as something else, and one that holds a character a
   reader that is not this one would take for syntax. Each of those is
   alone in a value here, so that taking any one of them out shows. */
static void test_ini_forms(void) {
    static const pin pins[] = {{ATOM(""), "k = \"\"\n"},
                               {ATOM("a"), "k = a\n"},
                               {ATOM("a b"), "k = \"a b\"\n"},
                               {ATOM(" lead"), "k = \" lead\"\n"},
                               {ATOM("trail "), "k = \"trail \"\n"},
                               {ATOM("x\ty"), "k = \"x\ty\"\n"},
                               {ATOM("a\rb"), "k = \"a\rb\"\n"},
                               {ATOM("a;b"), "k = \"a;b\"\n"},
                               {ATOM("a#b"), "k = \"a#b\"\n"},
                               {ATOM("a\"b"), "k = \"a\"b\"\n"},
                               {ATOM("\""), "k = \"\"\"\n"},
                               {ATOM("'a'"), "k = \"'a'\"\n"},
                               {ATOM("''"), "k = \"''\"\n"},
                               {ATOM("'"), "k = '\n"},
                               {ATOM("a'"), "k = a'\n"},
                               {ATOM("'a"), "k = 'a\n"},
                               {ATOM("x=y"), "k = x=y\n"},
                               {ATOM("[s]"), "k = [s]\n"}};
    size_t           i;

    if (format_named("ini") == NULL) {
        return;
    }
    for (i = 0; i < LEN(pins); ++i) {
        cf_arena*   ar  = cf_arena_new(NULL, 1 << 14);
        cf_error    err = CF_ERROR_INIT;
        cf_keyspace ks;
        char        label[96];

        if (ar == NULL) {
            check(0, "an arena");
            return;
        }
        esc(label, sizeof label, pins[i].value.p, pins[i].value.n);
        atom_space(&ks, ar, pins[i].value.p, pins[i].value.n);
        if (encode("ini", &ks, &err)) {
            eq(g_out, pins[i].line, label);
        } else {
            check(0, label);
            printf("  %s\n", err.message);
        }
        cf_arena_free(ar);
    }
}

/* Text that is not UTF-8 is not written by YAML, whose reader would
   refuse the file; it says which key. The other readers pass a byte on
   as it is, and what they should do instead is not decided here. One
   for each way a sequence can be wrong: a byte that begins nothing, one
   that is cut short, one whose continuation is not one, the long form of
   a short one, a surrogate, and a value past U+10FFFF. */
static void test_invalid_utf8(void) {
    static const atom bad[] = {ATOM("\xff"),
                               ATOM("\xfe"),
                               ATOM("\x80"),
                               ATOM("\xbf"),
                               ATOM("\xc3"),
                               ATOM("a\xe2\x98"),
                               ATOM("\xf0\x9f\x98"),
                               ATOM("\xc3\x28"),
                               ATOM("\xe2\x28\xa1"),
                               ATOM("\xe2\x82\x28"),
                               ATOM("\xf0\x28\x8c\xbc"),
                               ATOM("\xf0\x90\x28\xbc"),
                               ATOM("\xf0\x90\x80\x28"),
                               ATOM("\xc0\xaf"),
                               ATOM("\xc1\xbf"),
                               ATOM("\xe0\x80\x80"),
                               ATOM("\xe0\x9f\xbf"),
                               ATOM("\xf0\x80\x80\x80"),
                               ATOM("\xf0\x8f\xbf\xbf"),
                               ATOM("\xed\xa0\x80"),
                               ATOM("\xed\xbf\xbf"),
                               ATOM("\xf4\x90\x80\x80"),
                               ATOM("\xf5\x80\x80\x80"),
                               ATOM("\xf8\x88\x80\x80\x80"),
                               ATOM("\xfc\x84\x80\x80\x80\x80"),
                               ATOM("ok\xc3\xa9\xff")};
    const format*     yaml  = format_named("yaml");
    size_t            i;

    if (yaml == NULL) {
        return;
    }
    for (i = 0; i < LEN(bad); ++i) {
        cf_arena*   ar  = cf_arena_new(NULL, 1 << 14);
        cf_error    err = CF_ERROR_INIT;
        cf_keyspace ks;
        char        label[96];

        if (ar == NULL) {
            check(0, "an arena");
            return;
        }
        esc(label, sizeof label, bad[i].p, bad[i].n);
        atom_space(&ks, ar, bad[i].p, bad[i].n);
        expect(trip(yaml, &ks, &err), TRIP_REFUSED, yaml, label, &err);
        refused_key(&err, "'k'", label);
        cf_arena_free(ar);
    }
}

/* ---------------------------------------------------------------- *
 * Shapes
 * ---------------------------------------------------------------- */

typedef struct shape {
    const char* doc; /* JSON                                       */
    int         ini; /* and INI can hold it: arrays of values only */
} shape;

/* An array is the indexes 0 to count - 1, each spelled as the readers
   spell one, and the count, and nothing else. Every other group is a
   table, with its indexes and its count as members, and has to come back
   as the same keys: written as an array, {"count": 2, "0": "a"} was
   ["a"], and came back with a count of one. */
static const shape shapes[] = {
    /* a count that is not how many there are */
    {"{\"t\":{\"count\":2,\"0\":\"a\"}}", 1},
    {"{\"t\":{\"count\":1,\"0\":\"a\",\"1\":\"b\"}}", 1},
    {"{\"t\":{\"count\":0,\"0\":\"a\"}}", 1},
    /* an index that is missing, or not where it belongs */
    {"{\"t\":{\"count\":3,\"0\":\"a\",\"2\":\"c\"}}", 1},
    {"{\"t\":{\"count\":2,\"0\":\"a\",\"2\":\"c\"}}", 1},
    {"{\"t\":{\"count\":1,\"1\":\"b\"}}", 1},
    /* an index spelled otherwise than the readers spell it, or too long to
       be held: 2^64 would wrap to the first index, and be an array */
    {"{\"t\":{\"count\":2,\"0\":\"a\",\"01\":\"b\"}}", 1},
    {"{\"t\":{\"count\":1,\"18446744073709551616\":\"a\"}}", 1},
    /* a count that is not a whole number: a string somebody wrote is a
       string, and as an array it would come back an integer */
    {"{\"t\":{\"count\":-1,\"0\":\"a\"}}", 1},
    {"{\"t\":{\"count\":\"x\",\"0\":\"a\"}}", 1},
    {"{\"t\":{\"count\":\"2\",\"0\":\"a\",\"1\":\"b\"}}", 1},
    {"{\"t\":{\"count\":true,\"0\":\"a\"}}", 1},
#if CF_WITH_REAL
    {"{\"t\":{\"count\":2.5,\"0\":\"a\"}}", 1},
#endif
    /* a key that sorts between an index and what is below it */
    {"{\"t\":{\"count\":2,\"0\":\"a\",\"1\":{\"x\":\"b\"},\"1-\":\"c\"}}", 1},
    /* the root of a document is a table, whatever it holds */
    {"{\"count\":2,\"0\":\"a\",\"1\":\"b\"}", 1},
    {"{\"0\":{\"a\":1}}", 1},
    {"{\"t\":{\"0\":\"a\",\"1\":\"b\"}}", 1},
    {"{}", 1},
    /* keys that an array of one member has too, and that are not one */
    {"{\"l\":[{}]}", 1},
    /* arrays, with a table among the members, or a count among the
       members of one */
    {"{\"t\":{\"count\":1,\"0\":{\"a\":\"b\"}}}", 0},
    {"{\"l\":[{\"count\":2,\"0\":\"a\"}]}", 0},
    {"{\"l\":[{\"count\":0}]}", 0},
    {"{\"l\":[{\"0\":\"a\"}]}", 0},
    {"{\"l\":[{\"t\":{\"count\":1,\"1\":\"b\"}}]}", 0},
    {"{\"l\":[]}", 0},
    {"{\"l\":[[]]}", 0},
    {"{\"l\":[\"a\",\"b\"]}", 1}, /* INI's name[] lines (D-60) */
    {"{\"l\":[1,[2,[3,[4]]]]}", 0}};

static void test_shapes(void) {
    size_t i, f;

    for (i = 0; i < LEN(shapes); ++i) {
        for (f = 0; f < g_nhave; ++f) {
            if (g_have[f]->arrays == 1 || shapes[i].ini) {
                round_trip_doc(g_have[f], shapes[i].doc, shapes[i].doc);
            }
        }
    }
}

/* What is written, exactly: a group that is not an array is a table, and
   one that is is an array. */
static void test_written_as(void) {
    written_from("json", "{\"t\":{\"count\":1,\"1\":\"b\"}}",
                 "{\n  \"t\": {\n    \"1\": \"b\",\n    \"count\": 1\n  }\n}\n",
                 "a count and an index that is not the first is a table");
    written_from("json", "{\"t\":{\"count\":\"2\",\"0\":\"a\",\"1\":\"b\"}}",
                 "{\n  \"t\": {\n    \"0\": \"a\",\n    \"1\": \"b\",\n"
                 "    \"count\": \"2\"\n  }\n}\n",
                 "a count somebody wrote as a string is not a length");
    written_from("json", "{\"t\":[\"a\",\"b\"]}",
                 "{\n  \"t\": [\n    \"a\",\n    \"b\"\n  ]\n}\n",
                 "indexes from 0 and their count are an array");
    written_from("json", "{\"l\":[]}", "{\n  \"l\": []\n}\n",
                 "a count of zero and nothing else is an empty array");
    written_from("json", "{\"t\":{\"count\":0,\"0\":\"a\"}}",
                 "{\n  \"t\": {\n    \"0\": \"a\",\n    \"count\": 0\n  }\n}\n",
                 "a count of zero and an index is a table");

    /* A document read from a text format has not been typed, and its count
       is the text of a number. It is a length when it is the number of
       the elements, as it is once the document has been through a
       configuration and typed. */
    written_from("ini", "[t]\n0 = a\n1 = b\ncount = 2\n",
                 "{\n  \"t\": [\n    \"a\",\n    \"b\"\n  ]\n}\n",
                 "a count that is read as text is the length");
    written_from("ini", "[t]\n0 = a\ncount = 2\n",
                 "{\n  \"t\": {\n    \"0\": \"a\",\n    \"count\": \"2\"\n  }\n}\n",
                 "and is not when it is not how many there are");
    written_from("ini", "[t]\ncount = x\n0 = a\n",
                 "{\n  \"t\": {\n    \"0\": \"a\",\n    \"count\": \"x\"\n  }\n}\n",
                 "and is not when it is not a number");
    written_from("ini", "[t]\n0 = a\ncount = -1\n",
                 "{\n  \"t\": {\n    \"0\": \"a\",\n    \"count\": \"-1\"\n  }\n}\n",
                 "and is not when it is below nothing");
}

/* A value and a container cannot share a name in the written document. */
static void test_value_and_table(void) {
    cf_arena*   ar = cf_arena_new(NULL, 1 << 16);
    cf_keyspace ks;
    cf_value    v;
    size_t      f;

    if (ar == NULL) {
        check(0, "an arena");
        return;
    }
    cf_ks_init(&ks, ar, 8);
    cf_value_text(&v, "x", 1);
    cf_ks_add_authored(&ks, "t.0", 3, 10, &v, NULL);
    cf_value_text(&v, "y", 1);
    cf_ks_add_authored(&ks, "t.0.a", 5, 10, &v, NULL);
    cf_value_int(&v, 1);
    cf_ks_add(&ks, "t.count", 7, 10, &v, NULL);
    cf_ks_resolve(&ks);

    for (f = 0; f < g_nhave; ++f) {
        cf_error err = CF_ERROR_INIT;

        expect(trip(g_have[f], &ks, &err), TRIP_REFUSED, g_have[f],
               "t.0 as a value and a table", &err);
        check(err.code == CF_E_KEY, "a colliding key is refused as a key error");
        check(strcmp(err.key, "t.0") == 0, "the full conflicting key is named");
    }
    cf_arena_free(ar);
}

/* ---------------------------------------------------------------- *
 * What a format cannot hold
 * ---------------------------------------------------------------- */

static void test_nulls(void) {
    size_t f;

    for (f = 0; f < g_nhave; ++f) {
        const format* fm  = g_have[f];
        cf_arena*     ar  = cf_arena_new(NULL, 1 << 14);
        cf_error      err = CF_ERROR_INIT;
        cf_keyspace   ks;
        const char*   doc = "{\"a\":null}";
        int           got;

        if (ar == NULL) {
            check(0, "an arena");
            return;
        }
        cf_ks_init(&ks, ar, 8);
        check(decode("json", doc, strlen(doc), &ks, &err), "a null reads");
        cf_ks_resolve(&ks);
        got = trip(fm, &ks, &err);
        expect(got, fm->nulls ? TRIP_SAME : TRIP_REFUSED, fm, doc, &err);
        if (!fm->nulls) {
            refused_key(&err, "'a'", "a null");
        }
        cf_arena_free(ar);
    }
}

/* The same refusals as the caller of cf_emit meets them: no, the key, and
   nothing written. */
static void test_emit_refusals(void) {
    static const cf_pair line[] = {{"k", "a\nb"}};
    static const cf_pair utf8[] = {{"k", "\xff"}};
    static char          buf[256];
    cf_error             err = CF_ERROR_INIT;
    cf_builder*          b   = cf_build(NULL);
    cf_config*           c;

    cf_builder_memory(b, "m", CF_RANK_USER_FILE, line, 1);
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        check(0, "a value with a line break builds");
        return;
    }
    check(cf_emit(c, "json", CF_EMIT_ALL, 0, buf, sizeof buf, &err) > 0,
          "JSON writes a value with a line break");
    if (format_named("ini") != NULL) {
        buf[0] = 'x';
        check(cf_emit(c, "ini", CF_EMIT_ALL, 0, buf, sizeof buf, &err) == 0,
              "INI refuses it");
        refused_key(&err, "'k'", "INI and a line break");
        check(buf[0] == '\0', "and writes nothing");
    }
    cf_config_free(c);

    if (format_named("yaml") == NULL) {
        return;
    }
    b = cf_build(NULL);
    cf_builder_memory(b, "m", CF_RANK_USER_FILE, utf8, 1);
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        check(0, "a value that is not UTF-8 builds");
        return;
    }
    buf[0] = 'x';
    check(cf_emit(c, "yaml", CF_EMIT_ALL, 0, buf, sizeof buf, &err) == 0,
          "YAML refuses a value that is not UTF-8");
    refused_key(&err, "'k'", "YAML and bytes that are not UTF-8");
    check(buf[0] == '\0', "and writes nothing");
    cf_config_free(c);
}

/* ---------------------------------------------------------------- *
 * Trees
 *
 * xorshift64*, as test_fuzz.c has it and for the same reason: a seed is
 * only a bug report if the sequence is the same on every platform.
 * ---------------------------------------------------------------- */

static cf_u64 g_rng;

static void seed(cf_u64 s) {
    g_rng = (s != 0) ? s : (((cf_u64)20529293U << 32) | (cf_u64)3418323524U);
}

static cf_u64 next(void) {
    g_rng ^= g_rng >> 12;
    g_rng ^= g_rng << 25;
    g_rng ^= g_rng >> 27;
    return g_rng * (((cf_u64)625341585U << 32) | (cf_u64)1332534557U);
}

static size_t pick(size_t n) {
    return (n == 0) ? 0 : (size_t)(next() % (cf_u64)n);
}

/* Keys a document might have: words that are another format's syntax, and
   none that is a `count` or an index, which the shapes are for. */
static const char* const g_keys[] = {
    "a",  "b", "host", "x1",  "a-b", "a_b", "true", "null", "yes",     "on", "1a",
    "a1", "-", "--",   "0x1", "1e3", "inf", "e",    "size", "version", "id", "name"};

/* What else a value can be, spelled as JSON spells it. */
static const char* const g_raws[] = {"0",    "-1",    "9007199254740993",
#if CF_WITH_REAL
                                     "0.5",  "-2.5",  "1e300",
#endif
                                     "true", "false", "null"};

#define TREES 250
#define TREE_DEPTH 4
#define DOC_MAX 8192

typedef struct gen {
    char*  out;
    size_t cap;
    size_t at;
    int    arrays; /* the format has syntax for an array... */
    int    nulls;  /* ...for a null... */
    int    lines;  /* ...and for a line break */
} gen;

static void put(gen* g, const char* z) {
    g->at = cf_fmt_append(g->out, g->cap, g->at, "%s", z);
}

/* `n` bytes as a JSON string. */
static void put_string(gen* g, const char* p, size_t n) {
    static const char hex[] = "0123456789abcdef";
    size_t            i;

    put(g, "\"");
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)p[i];

        if (c == '"') {
            put(g, "\\\"");
        } else if (c == '\\') {
            put(g, "\\\\");
        } else if (c == '\n') {
            put(g, "\\n");
        } else if (c == '\r') {
            put(g, "\\r");
        } else if (c == '\t') {
            put(g, "\\t");
        } else if (c < 0x20) {
            g->at = cf_fmt_append(g->out, g->cap, g->at, "\\u00%c%c", hex[c >> 4],
                                  hex[c & 15u]);
        } else {
            g->at = cf_fmt_append(g->out, g->cap, g->at, "%c", (char)c);
        }
    }
    put(g, "\"");
}

static void put_scalar(gen* g) {
    if (pick(100) < 62) {
        const atom* a;

        do {
            a = &atoms[pick(LEN(atoms))];
        } while (!g->lines && has_newline(a));
        put_string(g, a->p, a->n);
    } else {
        const char* s;

        do {
            s = g_raws[pick(LEN(g_raws))];
        } while (!g->nulls && strcmp(s, "null") == 0);
        put(g, s);
    }
}

static void put_value(gen* g, int depth);

static void put_object(gen* g, int depth) {
    size_t used[3];
    size_t n = 1 + pick(3), i, j, k;

    put(g, "{");
    for (i = 0; i < n; ++i) {
        int dup;

        do {
            k   = pick(LEN(g_keys));
            dup = 0;
            for (j = 0; j < i; ++j) {
                if (used[j] == k) {
                    dup = 1;
                }
            }
        } while (dup);
        used[i] = k;
        if (i > 0) {
            put(g, ",");
        }
        put_string(g, g_keys[k], strlen(g_keys[k]));
        put(g, ":");
        put_value(g, depth + 1);
    }
    put(g, "}");
}

static void put_array(gen* g, int depth) {
    size_t n = 1 + pick(3), i;

    put(g, "[");
    for (i = 0; i < n; ++i) {
        if (i > 0) {
            put(g, ",");
        }
        if (g->arrays == 2) {
            put_scalar(g); /* values only: INI's name[] lines */
        } else {
            put_value(g, depth + 1);
        }
    }
    put(g, "]");
}

static void put_value(gen* g, int depth) {
    size_t r = pick(100);

    if (depth >= TREE_DEPTH || r < 50) {
        put_scalar(g);
    } else if (r < 75 || !g->arrays) {
        put_object(g, depth);
    } else {
        put_array(g, depth);
    }
}

/* Every format is given the same sequence of seeds' worth of documents,
   each as much as it can hold: a document in which something goes wrong
   is shown as it is, and it is the same document on every run. */
static void test_trees(void) {
    static char doc[DOC_MAX];
    size_t      f, t;

    for (f = 0; f < g_nhave; ++f) {
        const format* fm = g_have[f];
        gen           g;

        seed(0);
        for (t = 0; t < TREES; ++t) {
            g.out    = doc;
            g.cap    = sizeof doc;
            g.at     = 0;
            g.arrays = fm->arrays;
            g.nulls  = fm->nulls;
            g.lines  = fm->lines;
            doc[0]   = '\0';
            put_object(&g, 0);
            round_trip_doc(fm, doc, doc);
        }
    }
}

/* ---------------------------------------------------------------- *
 * The edges of what can be held
 * ---------------------------------------------------------------- */

/* A number as JSON spells it comes back as that number: an integer to
   its last digit, and a real as the same real. */
static void test_numbers(void) {
    static const char* const numbers[] = {"0",
                                          "-0",
                                          "1",
                                          "-1",
                                          "42",
                                          "2147483647",
                                          "2147483648",
                                          "4294967296",
                                          "9007199254740992",
                                          "9007199254740993",
                                          "9223372036854775807",
                                          "-9223372036854775808",
#if CF_WITH_REAL
                                          "0.5",
                                          "-2.5",
                                          "1.0",
                                          "-0.0",
                                          "0.1",
                                          "100.0",
                                          "1e5",
                                          "1E5",
                                          "1e-7",
                                          "1.5e300",
                                          "1e308",
                                          "-1e308",
                                          "5e-324",
                                          "2.2250738585072014e-308",
                                          "1.7976931348623157e308",
                                          "123456789.125",
                                          "0.30000000000000004",
                                          "1e21",
                                          "1e22"
#endif
    };
    static char doc[64];
    size_t      i, f;

    for (i = 0; i < LEN(numbers); ++i) {
        cf_fmt(doc, sizeof doc, "{\"n\":%s}", numbers[i]);
        for (f = 0; f < g_nhave; ++f) {
            round_trip_doc(g_have[f], doc, doc);
        }
    }
}

/* The container `level` of `levels` in a document that is nothing but
   containers, with a string in the innermost. Of kind 0 they are all
   objects; of kind 1 the root is an object and what is inside it arrays;
   of kind 2 they are objects and arrays by turns. */
static void nest(gen* g, int level, int levels, int kind) {
    cf_bool object =
        (level == 1 || kind == 0 || (kind == 2 && level % 2 == 1)) ? CF_TRUE : CF_FALSE;

    if (level > levels) {
        put(g, "\"x\"");
    } else if (object) {
        put(g, "{\"a\":");
        nest(g, level + 1, levels, kind);
        put(g, "}");
    } else {
        put(g, "[");
        nest(g, level + 1, levels, kind);
        put(g, "]");
    }
}

/* CF_MAX_DEPTH is the deepest a document may be, the document being the
   first level, and every reader and every writer takes that much. A
   document of every depth up to it comes back from every format, however
   it is made of objects and arrays; and the JSON reader takes no more,
   so there is nothing deeper to write. */
static void test_depth(void) {
    static char doc[1024];
    int         kind, levels;
    size_t      f;

    for (kind = 0; kind < 3; ++kind) {
        for (levels = 1; levels <= CF_MAX_DEPTH + 1; ++levels) {
            gen g;

            g.out    = doc;
            g.cap    = sizeof doc;
            g.at     = 0;
            g.arrays = g.nulls = g.lines = 1;
            doc[0]                       = '\0';
            nest(&g, 1, levels, kind);
            for (f = 0; f < g_nhave; ++f) {
                cf_error err = CF_ERROR_INIT;
                int      got;

                if (kind != 0 && g_have[f]->arrays != 1) {
                    continue;
                }
                got = trip_doc(g_have[f], doc, &err);
                expect(got, (levels <= CF_MAX_DEPTH) ? TRIP_SAME : TRIP_NO_DOC,
                       g_have[f], doc, &err);
            }
        }
    }
}

/* A key of the length at which the buffers end, as one name and as
   several. The longest key is CF_KEY_MAX - 1 bytes: the limit counts the
   terminator. */
static void test_long_keys(void) {
    static char         key[CF_KEY_MAX + 1];
    static const size_t lens[] = {
        1, 2, 3, CF_KEY_MAX - 3, CF_KEY_MAX - 2, CF_KEY_MAX - 1};
    static const size_t segs[] = {CF_KEY_MAX, 31, 7, 1};
    size_t              i, j;

    memset(key, 'a', CF_KEY_MAX);
    check(cf_key_valid(key, CF_KEY_MAX - 1), "the longest key is a key");
    check(!cf_key_valid(key, CF_KEY_MAX), "a key as long as the limit is not");

    for (i = 0; i < LEN(lens); ++i) {
        for (j = 0; j < LEN(segs); ++j) {
            size_t      n = lens[i], k, parts = 1, f;
            cf_arena*   ar = cf_arena_new(NULL, 1 << 14);
            cf_keyspace ks;
            cf_value    v;

            if (ar == NULL) {
                check(0, "an arena");
                return;
            }
            for (k = 0; k < n; ++k) {
                key[k] = (k + 1 < n && k % (segs[j] + 1) == segs[j]) ? '.' : 'a';
            }
            for (k = 0; k < n; ++k) {
                if (key[k] == '.') {
                    ++parts;
                }
            }
            key[n] = '\0';

            cf_ks_init(&ks, ar, 8);
            cf_value_text(&v, "v", 1);
            check(cf_ks_add_authored(&ks, key, n, 10, &v, NULL), "the key is a key");
            cf_ks_resolve(&ks);
            for (f = 0; f < g_nhave; ++f) {
                cf_error err = CF_ERROR_INIT;
                int      got = trip(g_have[f], &ks, &err);

                if (parts <= CF_MAX_DEPTH) {
                    expect(got, TRIP_SAME, g_have[f], key, &err);
                } else {
                    expect_holds(got, g_have[f], key, &err);
                }
            }
            cf_arena_free(ar);
        }
    }
}

/* An array whose length key is the longest key, and one whose length
   key would be a byte longer: its elements fit, and it is refused rather
   than read without a length. */
static void test_long_array_key(void) {
    static char doc[CF_KEY_MAX + 32];
    size_t      plen, f;

    for (plen = CF_KEY_MAX - 7; plen <= CF_KEY_MAX - 6; ++plen) {
        cf_arena*   ar = cf_arena_new(NULL, 1 << 14);
        cf_keyspace ks;
        cf_error    err = CF_ERROR_INIT;
        cf_bool     read;

        if (ar == NULL) {
            check(0, "an arena");
            return;
        }
        doc[0] = '{';
        doc[1] = '"';
        memset(doc + 2, 'a', plen);
        memcpy(doc + 2 + plen, "\":[1,2]}", 9);
        cf_ks_init(&ks, ar, 8);
        read = decode("json", doc, strlen(doc), &ks, &err);
        cf_arena_free(ar);
        if (plen + 6 >= CF_KEY_MAX) {
            check(!read, "an array whose length key is too long is refused");
            continue;
        }
        check(read, "an array whose length key is the longest key is read");
        for (f = 0; f < g_nhave; ++f) {
            if (g_have[f]->arrays == 1) {
                round_trip_doc(g_have[f], doc, "an array at the longest path");
            }
        }
    }
}

/* Values at buffer boundaries, independent of the error message limit. */
static void test_long_values(void) {
    static char         big[8192];
    static const size_t sizes[] = {1,    2,    3,    63,   64,   65,   127, 128, 129,
                                   253,  254,  255,  256,  257,  511,  512, 513, 1023,
                                   1024, 1025, 4095, 4096, 4097, 8191, 8192};
    static const char   plain[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    static const char   odd[]   = "a b\"c\\d\te#f;g'h=i[j]k{l}m:n,o ";
    size_t              i, k;

    for (i = 0; i < LEN(sizes); ++i) {
        for (k = 0; k < sizes[i]; ++k) {
            big[k] = plain[k % (sizeof plain - 1)];
        }
        atom_trip(big, sizes[i]);
        for (k = 0; k < sizes[i]; ++k) {
            big[k] = odd[k % (sizeof odd - 1)];
        }
        atom_trip(big, sizes[i]);
    }
}

/* Arrays long enough for the indexes to be of two digits and of three,
   which as keys are not in the order of the numbers: 1, 10, 100, 101,
   11, 12, 2 and so on. An element is written where its index says. */
static void test_long_arrays(void) {
    static char         doc[4096];
    static const size_t sizes[] = {2, 9, 10, 11, 12, 99, 100, 101, 120};
    size_t              i, j, f;
    int                 tables;

    for (i = 0; i < LEN(sizes); ++i) {
        for (tables = 0; tables < 2; ++tables) {
            gen g;

            g.out    = doc;
            g.cap    = sizeof doc;
            g.at     = 0;
            g.arrays = g.nulls = g.lines = 1;
            doc[0]                       = '\0';
            put(&g, "{\"l\":[");
            for (j = 0; j < sizes[i]; ++j) {
                if (j > 0) {
                    put(&g, ",");
                }
                if (tables) {
                    g.at = cf_fmt_append(g.out, g.cap, g.at, "{\"a\":%u}", (unsigned)j);
                } else {
                    g.at = cf_fmt_append(g.out, g.cap, g.at, "%u", (unsigned)j);
                }
            }
            put(&g, "]}");
            for (f = 0; f < g_nhave; ++f) {
                if (g_have[f]->arrays == 1 || (g_have[f]->arrays == 2 && !tables)) {
                    round_trip_doc(g_have[f], doc, doc);
                }
            }
        }
    }
}

int main(void) {
    size_t i;

    if (cf_codec_by_name("json") == NULL) {
        printf("skipped: this build has no JSON, and the documents are JSON\n");
        return 0;
    }
    for (i = 0; i < LEN(g_formats); ++i) {
        const cf_codec* c = cf_codec_by_name(g_formats[i].name);

        if (c != NULL && c->encoder != NULL) {
            g_have[g_nhave++] = &g_formats[i];
            check(((c->caps & CF_CODEC_ARRAYS) != 0) == (g_formats[i].arrays == 1) &&
                      ((c->caps & (CF_CODEC_ARRAYS | CF_CODEC_SCALAR_ARRAYS)) != 0) ==
                          (g_formats[i].arrays != 0),
                  "the table says what the codec says about arrays");
        }
    }

    test_atoms();
    test_code_points();
    test_ini_forms();
    test_invalid_utf8();
    test_shapes();
    test_written_as();
    test_value_and_table();
    test_nulls();
    test_emit_refusals();
    test_numbers();
    test_depth();
    test_long_keys();
    test_long_array_key();
    test_long_values();
    test_long_arrays();
    test_trees();

    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}

#else /* !CF_WITH_EMIT */

int main(void) {
    printf("skipped: this build has no emitter (CF_WITH_EMIT=OFF)\n");
    return 0;
}

#endif /* CF_WITH_EMIT */
