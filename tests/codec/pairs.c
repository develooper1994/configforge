/*
 * pairs.c - every document through every ordered pair of the six writing
 * formats, and what comes back held to the table of what each format can
 * hold (D-109).
 *
 * For a document read through its own format F and each pair (A, B):
 * hop 0 writes it into A, hop 1 writes that into B and hop 2 writes the
 * result into A again, all through cv_convert, the converter's own code,
 * without a process. Each hop is held to chain.h's oracle: what the
 * target's reader gives back is what the table says the target writes of
 * what the source's reader gave, and nothing is lost without a finding
 * or a count. A pair whose target holds everything its source holds
 * loses nothing.
 *
 * The documents are the edge documents built here, the project's own
 * fixtures and, from pairs.py, the conformance suites' valid ones
 * (--docs <manifest>: `<format> TAB <path> TAB <mode>` a line, mode `-`,
 * `wrap` or `split`; --root <dir> is put before each relative path).
 * C89; it sees the public headers only.
 */

/* The tests' directory defines CF_INTERNAL_API as 1; this program does not
   use the internal tier. */
#undef CF_INTERNAL_API
#define CF_INTERNAL_API 0

#include "chain.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PR_SHOWN 20 /* failures printed */

static const char* const g_writers[] = {"ini", "json", "json5", "toml", "yaml", "cbor"};

#define WRITERS (sizeof g_writers / sizeof g_writers[0])

typedef struct pr_doc {
    char*           name;
    const cf_codec* codec;
    char*           bytes;
    size_t          n;
} pr_doc;

typedef struct pr_state {
    pr_doc* docs;
    size_t  ndocs;
    size_t  cap;
    size_t  not_read;
    size_t  checks;
    size_t  failures;
    size_t  show;    /* failures printed */
    cf_bool verbose; /* name the documents that were not read */
} pr_state;

static void pr_die(const char* what) {
    fprintf(stderr, "pairs: %s\n", what);
    exit(2);
}

static void* pr_alloc(size_t n) {
    void* p = malloc(n == 0 ? 1u : n);

    if (p == NULL) {
        pr_die("out of memory");
    }
    return p;
}

static char* pr_dup(const char* p, size_t n) {
    char* s = (char*)pr_alloc(n + 1u);

    memcpy(s, p, n);
    s[n] = '\0';
    return s;
}

/* A string builder. */
typedef struct pr_buf {
    char*  p;
    size_t n;
    size_t cap;
} pr_buf;

static void pb_put(pr_buf* b, const char* s, size_t n) {
    if (b->n + n + 1u > b->cap) {
        size_t cap = (b->cap == 0) ? 256u : b->cap;
        char*  p;

        while (cap < b->n + n + 1u) {
            cap *= 2u;
        }
        p = (char*)pr_alloc(cap);
        if (b->n > 0) {
            memcpy(p, b->p, b->n);
        }
        free(b->p);
        b->p   = p;
        b->cap = cap;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void pb_str(pr_buf* b, const char* s) {
    pb_put(b, s, strlen(s));
}

static void pb_rep(pr_buf* b, const char* s, size_t times) {
    size_t i;

    for (i = 0; i < times; ++i) {
        pb_str(b, s);
    }
}

static void pb_num(pr_buf* b, unsigned long v) {
    char num[32];

    sprintf(num, "%lu", v);
    pb_str(b, num);
}

static void pb_free(pr_buf* b) {
    free(b->p);
    memset(b, 0, sizeof *b);
}

/* ------------------------------------------------------------------ *
 * The list of documents
 * ------------------------------------------------------------------ */

static void pr_add(pr_state* st, const char* name, const cf_codec* codec, const char* p,
                   size_t n) {
    pr_doc* d;

    if (st->ndocs == st->cap) {
        size_t  cap = (st->cap == 0) ? 128u : st->cap * 2u;
        pr_doc* v   = (pr_doc*)pr_alloc(cap * sizeof *v);

        if (st->ndocs > 0) {
            memcpy(v, st->docs, st->ndocs * sizeof *v);
        }
        free(st->docs);
        st->docs = v;
        st->cap  = cap;
    }
    d        = &st->docs[st->ndocs++];
    d->name  = pr_dup(name, strlen(name));
    d->codec = codec;
    d->bytes = pr_dup(p, n);
    d->n     = n;
}

/* ------------------------------------------------------------------ *
 * The edge documents, JSON text: a single quote stands for a double
 * quote, and a quote that is meant is the escape u0027 or u0022.
 * ------------------------------------------------------------------ */

static void pr_edge(pr_state* st, const char* name, const char* json) {
    pr_buf b;
    size_t i;

    memset(&b, 0, sizeof b);
    for (i = 0; json[i] != '\0'; ++i) {
        pb_put(&b, (json[i] == '\'') ? "\"" : &json[i], 1u);
    }
    pr_add(st, name, cf_codec_by_name("json"), b.p, b.n);
    pb_free(&b);
}

/* `count` tables nested in one another, each called `name`, the last
   holding a leaf; the document is the first brace. */
static void pr_nest(pr_buf* b, size_t count, const char* name) {
    size_t i;

    pb_str(b, "{");
    for (i = 0; i < count; ++i) {
        pb_str(b, "\"");
        pb_str(b, name);
        pb_str(b, "\":{");
    }
    pb_str(b, "\"leaf\":1");
    pb_rep(b, "}", count + 1u);
}

static char* pr_name(size_t len) {
    char* s = (char*)pr_alloc(len + 1u);

    memset(s, 'n', len);
    s[len] = '\0';
    return s;
}

static void pr_nest_doc(pr_state* st, const char* doc, size_t count, const char* name) {
    pr_buf b;

    memset(&b, 0, sizeof b);
    pr_nest(&b, count, name);
    pr_add(st, doc, cf_codec_by_name("json"), b.p, b.n);
    pb_free(&b);
}

#define NAMES                                                                   \
    "'Mixed_Case':1,'with space':2,'dot.ted':3,'quote\\u0022d':4,'\\t':5,"      \
    "'\\u00e9':6,'':7,'a-b_c9':8"

static void pr_edges(pr_state* st) {
    pr_buf b;
    size_t i;
    char*  name;
    size_t lim = (size_t)CF_KEY_MAX;

    pr_edge(st, "types", "{'i': 1, 'f': 0.5, 'b': true, 'n': null, 's': 'x'}");
    pr_edge(st, "ints",
            "{'a':-9223372036854775808,'b':9223372036854775807,'c':0,'d':-1}");
    pr_edge(st, "reals",
            "{'a':5e-324,'b':1.7976931348623157e308,'c':0.30000000000000004,"
            "'d':0.1,'e':1e22,'f':1e-7,'g':-0.0,'h':1e16,'i':123456789012345.6}");
    pr_edge(st, "non_finite", "{'a':'nan','b':'inf','c':'-inf','d':'Infinity'}");
    pr_edge(st, "texts",
            "{'t0':'','t1':'  x  ','t2':'\\u0022','t3':'\\\\','t4':'\\u0027q\\u0027',"
            "'t5':'#','t6':';','t7':'=','t8':'[x]','t9':'${x}','t10':'$${y}',"
            "'t11':'\\t','t12':'\\u00e9','t13':'\\u20ac','t14':'\\ud83d\\ude00',"
            "'t15':'\\udbff\\udfff','t16':'\\ufffe','t17':'\\u007f','t18':'\\u0085',"
            "'t19':'\\u2028'}");
    pr_edge(st, "breaks", "{'a':'a\\nb','b':'a\\r\\nb','c':'a\\rb','k':'v'}");
    pr_edge(st, "nul", "{'a':'x\\u0000y','b':'\\u0000'}");

    memset(&b, 0, sizeof b);
    pb_str(&b, "{" NAMES ",'t':{" NAMES "}}");
    pr_edge(st, "names", b.p);
    pb_free(&b);

    pr_edge(st, "arrays", "{'a':[1,2,3],'b':[1,'a',true,null,0.5],'c':['a','b'],'d':[7]}");
    pr_edge(st, "arrays_hard",
            "{'a':[],'b':[{'a':1},{'b':2}],'c':[[1,2],[3]],'d':[[[1]]],'e':[{'a':[1,2]}]}");
    pr_edge(st, "empty_tables",
            "{'e': {}, 'n': {'e': {}}, 'k': 1, 't': {'only': {'x': 1}}, 'o': {'p': {}}}");
    pr_edge(st, "order", "{'z': 1, 'a': 2, 'm': {'b': 1, 'a': 2}}");
    pr_edge(st, "looks",
            "{'a':'true','b':'123','c':'null','d':'0x1F','e':'1e3','f':'~','g':'yes','n':5}");

    pr_nest_doc(st, "deep31", 31u, "a");
    name = pr_name(60);
    pr_nest_doc(st, "ok_path", 4u, name);
    free(name);
    name = pr_name(50);
    pr_nest_doc(st, "long_path", 6u, name);
    free(name);

    /* The boundary of the path rule at CF_KEY_MAX: a table is refused when
       its parent's path and its own name pass CF_KEY_MAX less two bytes. */
    name = pr_name(lim - 2u);
    pr_nest_doc(st, "path_1_ok", 1u, name);
    free(name);
    name = pr_name(lim - 1u);
    pr_nest_doc(st, "path_1_long", 1u, name);
    free(name);
    memset(&b, 0, sizeof b);
    name = pr_name(lim - 3u);
    pb_str(&b, "{\"a\":{\"");
    pb_str(&b, name);
    pb_str(&b, "\":{\"leaf\":1}}}");
    pr_add(st, "path_2_ok", cf_codec_by_name("json"), b.p, b.n);
    free(name);
    pb_free(&b);
    name = pr_name(lim - 2u);
    pb_str(&b, "{\"a\":{\"");
    pb_str(&b, name);
    pb_str(&b, "\":{\"leaf\":1}}}");
    pr_add(st, "path_2_long", cf_codec_by_name("json"), b.p, b.n);
    free(name);
    pb_free(&b);

    pb_str(&b, "{\"a\":[");
    for (i = 0; i < 2000u; ++i) {
        if (i > 0) {
            pb_str(&b, ",");
        }
        pb_num(&b, (unsigned long)i);
    }
    pb_str(&b, "],\"t\":\"");
    pb_rep(&b, "x", 70000u);
    pb_str(&b, "\"}");
    pr_add(st, "big", cf_codec_by_name("json"), b.p, b.n);
    pb_free(&b);
}

/* ------------------------------------------------------------------ *
 * The manifest
 * ------------------------------------------------------------------ */

/* `path` read whole into memory, NULL when it cannot be read. */
static char* pr_slurp(const char* path, size_t* n) {
    FILE*  f = fopen(path, "rb");
    pr_buf b;
    char   chunk[4096];
    size_t got;

    if (f == NULL) {
        return NULL;
    }
    memset(&b, 0, sizeof b);
    while ((got = fread(chunk, 1, sizeof chunk, f)) > 0) {
        pb_put(&b, chunk, got);
    }
    (void)fclose(f);
    if (b.p == NULL) {
        pb_put(&b, "", 0u);
    }
    *n = b.n;
    return b.p;
}

/* A line of five dashes, and nothing else, ends a document of a `split`
   file. */
static cf_bool pr_dashes(const char* p, size_t n) {
    return (n == 5u && memcmp(p, "-----", 5u) == 0) ? CF_TRUE : CF_FALSE;
}

static void pr_split(pr_state* st, const char* name, const cf_codec* c, const char* p,
                     size_t n) {
    size_t at    = 0;
    size_t start = 0;
    size_t index = 0;
    char   label[512];

    while (start < n) {
        size_t end = start;
        size_t len;

        while (end < n && p[end] != '\n') {
            ++end;
        }
        len = end - start;
        if (len > 0 && p[start + len - 1u] == '\r') {
            --len;
        }
        if (pr_dashes(p + start, len)) {
            sprintf(label, "%.400s#%lu", name, (unsigned long)index++);
            pr_add(st, label, c, p + at, start - at);
            at = (end < n) ? end + 1u : n;
        }
        start = end + 1u;
    }
    if (at < n) {
        sprintf(label, "%.400s#%lu", name, (unsigned long)index);
        pr_add(st, label, c, p + at, n - at);
    }
}

/* Each line of the manifest: its documents are added. */
static void pr_manifest(pr_state* st, const char* manifest, const char* root) {
    size_t n    = 0;
    char*  text = pr_slurp(manifest, &n);
    size_t at   = 0;

    if (text == NULL) {
        fprintf(stderr, "pairs: cannot read %s\n", manifest);
        exit(2);
    }
    while (at < n) {
        size_t end = at;
        char*  line;
        char*  tab1;
        char*  tab2;

        while (end < n && text[end] != '\n') {
            ++end;
        }
        line = pr_dup(text + at, end - at);
        at   = end + 1u;
        if (line[0] != '\0' && line[strlen(line) - 1u] == '\r') {
            line[strlen(line) - 1u] = '\0';
        }
        tab1 = strchr(line, '\t');
        tab2 = (tab1 != NULL) ? strchr(tab1 + 1, '\t') : NULL;
        if (line[0] != '\0' && line[0] != '#') {
            const cf_codec* c;
            pr_buf          path;
            char*           bytes;
            size_t          len = 0;

            if (tab2 == NULL) {
                fprintf(stderr, "pairs: %s: a line needs three fields: %s\n", manifest,
                        line);
                exit(2);
            }
            *tab1 = '\0';
            *tab2 = '\0';
            c     = cf_codec_by_name(line);
            if (c == NULL) {
                fprintf(stderr, "pairs: %s: no codec '%s' in this build\n", manifest,
                        line);
                exit(2);
            }
            memset(&path, 0, sizeof path);
            if (root != NULL) {
                pb_str(&path, root);
                pb_str(&path, "/");
            }
            pb_str(&path, tab1 + 1);
            bytes = pr_slurp(path.p, &len);
            if (bytes == NULL) {
                fprintf(stderr, "pairs: cannot read %s\n", path.p);
                exit(2);
            }
            if (strcmp(tab2 + 1, "split") == 0) {
                pr_split(st, tab1 + 1, c, bytes, len);
            } else if (strcmp(tab2 + 1, "-") == 0) {
                pr_add(st, tab1 + 1, c, bytes, len);
            } else {
                fprintf(stderr, "pairs: %s: mode '%s' is not - or split\n", manifest,
                        tab2 + 1);
                exit(2);
            }
            free(bytes);
            pb_free(&path);
        }
        free(line);
    }
    free(text);
}

/* ------------------------------------------------------------------ *
 * The checks
 * ------------------------------------------------------------------ */

static void pr_fail(pr_state* st, const pr_doc* d, const char* a, const char* b,
                    const char* where, const char* why) {
    if (st->failures < st->show) {
        printf("FAIL %s %s -> %s %s: %s\n", d->name, a, b, where, why);
    }
    ++st->failures;
}

/* The pair's own check: a pair the table says loses nothing loses
   nothing. */
static cf_bool pr_lossless(unsigned from, unsigned to) {
    unsigned have = to & ~(unsigned)CF_REFUSES_NULL;
    unsigned need = from & ~(unsigned)CF_REFUSES_NULL;

    return ((need & ~have) == 0u) ? CF_TRUE : CF_FALSE;
}

/* One document through one ordered pair. False when the document turned
   out not to be convertible at all (the bound at hop 0): it is counted
   as not read, once. */
static cf_bool pr_pair(pr_state* st, const pr_doc* d, size_t ia, size_t ib) {
    const cf_codec* ca = cf_codec_by_name(g_writers[ia]);
    const cf_codec* cb = cf_codec_by_name(g_writers[ib]);
    unsigned        ha = 0;
    unsigned        hb = 0;
    cf_blob         in;
    cf_arena*       ar;
    ch_hop          h1;
    ch_hop          h2;
    ch_hop          h3;
    int             rc;
    cf_error        err = CF_ERROR_INIT;

    (void)cf_format_holds(g_writers[ia], &ha);
    (void)cf_format_holds(g_writers[ib], &hb);
    ar = cf_arena_new(NULL, 0);
    if (ar == NULL) {
        pr_die("out of memory");
    }
    cf_blob_mem(&in, d->bytes, d->n, d->name);
    rc = ch_hop_run(ar, &in, d->codec, ca, &h1);
    ++st->checks;
    if (rc == CH_BOUND) {
        cf_arena_free(ar);
        return CF_FALSE;
    }
    if (rc != CH_OK) {
        pr_fail(st, d, g_writers[ia], g_writers[ib], "hop 0", h1.why);
        cf_arena_free(ar);
        return CF_TRUE;
    }
    rc = ch_hop_run(ar, &h1.out, ca, cb, &h2);
    ++st->checks;
    if (rc != CH_OK) {
        pr_fail(st, d, g_writers[ia], g_writers[ib], "hop 1",
                (rc == CH_BOUND) ? "the converter refused by its bound" : h2.why);
        cf_arena_free(ar);
        return CF_TRUE;
    }
    rc = ch_hop_run(ar, &h2.out, cb, ca, &h3);
    ++st->checks;
    if (rc != CH_OK) {
        pr_fail(st, d, g_writers[ia], g_writers[ib], "hop 2",
                (rc == CH_BOUND) ? "the converter refused by its bound" : h3.why);
        cf_arena_free(ar);
        return CF_TRUE;
    }
    if (pr_lossless(ha, hb)) {
        ch_tree* r1 = ch_read(ar, &h1.out, ca, &err);
        ch_tree* r3 = ch_read(ar, &h3.out, ca, &err);

        ++st->checks;
        if (r1 == NULL || r3 == NULL || !ch_same(ar, r1, r3)) {
            pr_fail(st, d, g_writers[ia], g_writers[ib], "pair",
                    "a pair the table says loses nothing lost something");
        } else if (ia == ib) {
            /* A document written again is the same bytes: hop 2's are hop
               1's. (Not hop 0's: a YAML file written from a typed source
               has `1`, and from its own reading `"1"`.) */
            ++st->checks;
            if (h2.out.n != h3.out.n || memcmp(h2.out.p, h3.out.p, h2.out.n) != 0) {
                pr_fail(st, d, g_writers[ia], g_writers[ib], "pair",
                        "writing is not stable: hop 2's bytes are not hop 1's");
            }
        }
    }
    cf_arena_free(ar);
    return CF_TRUE;
}

/* Whether the document is read by its own format at all. */
static cf_bool pr_readable(const pr_doc* d) {
    cf_arena* ar = cf_arena_new(NULL, 0);
    cf_blob   in;
    cf_error  err = CF_ERROR_INIT;
    cf_bool   ok;

    if (ar == NULL) {
        pr_die("out of memory");
    }
    cf_blob_mem(&in, d->bytes, d->n, d->name);
    ok = (ch_read(ar, &in, d->codec, &err) != NULL) ? CF_TRUE : CF_FALSE;
    cf_arena_free(ar);
    return ok;
}

static void pr_run_docs(pr_state* st) {
    size_t i;
    size_t a;
    size_t b;

    for (i = 0; i < st->ndocs; ++i) {
        const pr_doc* d = &st->docs[i];
        cf_bool       ok = pr_readable(d);

        for (a = 0; ok && a < WRITERS; ++a) {
            for (b = 0; ok && b < WRITERS; ++b) {
                ok = pr_pair(st, d, a, b);
            }
        }
        if (!ok) {
            ++st->not_read;
            if (st->verbose) {
                printf("not read: %s\n", d->name);
            }
        }
    }
}

int main(int argc, char** argv) {
    pr_state    st;
    const char* root = NULL;
    const char* manifests[8];
    size_t      nman = 0;
    size_t      m;
    int         i;

    memset(&st, 0, sizeof st);
    st.show = PR_SHOWN;
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
            root = argv[++i];
        } else if (strcmp(argv[i], "--show") == 0 && i + 1 < argc) {
            st.show = (size_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--verbose") == 0) {
            st.verbose = CF_TRUE;
        } else if (strcmp(argv[i], "--docs") == 0 && i + 1 < argc &&
                   nman < sizeof manifests / sizeof manifests[0]) {
            manifests[nman++] = argv[++i];
        } else {
            fprintf(stderr,
                    "usage: codec_pairs [--root <dir>] [--docs <manifest>]... "
                    "[--show <n>] [--verbose]\n");
            return 2;
        }
    }
    pr_edges(&st);
    for (m = 0; m < nman; ++m) {
        pr_manifest(&st, manifests[m], root);
    }
    for (i = 0; i < (int)WRITERS; ++i) {
        if (cf_codec_by_name(g_writers[i]) == NULL) {
            fprintf(stderr, "pairs: the build has no '%s' codec\n", g_writers[i]);
            return 2;
        }
    }
    pr_run_docs(&st);
    printf("pairs: %lu documents (%lu not read), 36 pairs, %lu checks, %lu failures\n",
           (unsigned long)st.ndocs, (unsigned long)st.not_read,
           (unsigned long)st.checks, (unsigned long)st.failures);
    return (st.failures == 0) ? 0 : 1;
}
