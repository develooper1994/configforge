/*
 * test_yaml.c - YAML, through the built-in parser, libfyaml or libyaml
 * (D-39, D-62, D-63, D-69):
 * what it reads, what it refuses by name, and that what it writes reads
 * back as the same configuration.
 */

#include <configforge/configforge.h>
#include <configforge/cf_serialize.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cf_test.h"

#if CF_WITH_YAML

/* A configuration from one YAML file, with no definitions. */
static cf_config* load(const char* text, cf_error* err) {
    cf_builder* b = cf_build(NULL);
    if (!spill("cf_yaml.yaml", text)) {
        return NULL;
    }
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_yaml.yaml", NULL, 0);
    return cf_builder_create(b, err);
}

static const char g_doc[] = "# a comment\n"
                            "title: ConfigForge\n"
                            "server:\n"
                            "  host: \"0.0.0.0\"\n"
                            "  port: 8080\n"
                            "  tls: {enabled: true, cert: 'c\xc3\xa9rt.pem'}\n"
                            "  tags: [a, \"b\"]\n"
                            "country: no\n"
                            "version: \"3.10\"\n"
                            "nothing: ~\n"
                            "empty:\n"
                            "upstream:\n"
                            "  - host: a\n"
                            "  - host: b\n"
                            "    weight: 2\n"
                            "motd: |\n"
                            "  two\n"
                            "  lines\n";

static void test_reads(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c   = load(g_doc, &err);
    static char ex[4096];

    check(c != NULL, "a YAML document builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(strcmp(cf_get_text(c, "title", ""), "ConfigForge") == 0, "a plain scalar");
    check(cf_get_int(c, "server.port", 0) == 8080, "a number, by Tier 0 inference");
    check(strcmp(cf_get_text(c, "server.host", ""), "0.0.0.0") == 0, "a quoted string");
    check(cf_get_bool(c, "server.tls.enabled", CF_FALSE), "a flow mapping");
    check(strcmp(cf_get_text(c, "server.tls.cert", ""), "c\xc3\xa9rt.pem") == 0,
          "with UTF-8 in it");
    check(cf_array_count(c, "server.tags") == 2, "a flow sequence");
    check(strcmp(cf_get_text(c, "server.tags.1", ""), "b") == 0,
          "its elements by index");
    check(cf_array_count(c, "upstream") == 2, "a block sequence of mappings");
    check(cf_get_int(c, "upstream.1.weight", 0) == 2, "and the keys inside them");
    check(strcmp(cf_get_text(c, "motd", ""), "two\nlines\n") == 0, "a literal block");

    /* The YAML problem, which the library's one spelling of a boolean
       (D-24) answers for every version of YAML at once. */
    check(cf_type_of(c, "country") == CF_TYPE_TEXT, "no is the text \"no\", not false");
    check(strcmp(cf_get_text(c, "version", ""), "3.10") == 0,
          "and a quoted 3.10 keeps its zero");
    check(cf_type_of(c, "nothing") == CF_TYPE_NULL, "~ is null");
    check(cf_type_of(c, "empty") == CF_TYPE_NULL, "and so is nothing at all");

    cf_explain(c, "server.port", ex, sizeof ex);
    has(ex, "cf_yaml.yaml:5:9", "explain() names the line and column");
    cf_config_free(c);
}

/* With a contract, the definition types a plain scalar - and a quoted
   one says it is text, which a number definition refuses. */
static void test_types(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b;
    cf_config*  c;

    check(spill("cf_yaml.yaml", "port: 9000\nname: 42\n"), "wrote a file");
    b = cf_build(NULL);
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_yaml.yaml", NULL, 0);
    cf_builder_int(b, "port", 1, 1, 65535, 0);
    cf_builder_text(b, "name", "x", 0);
    c = cf_builder_create(b, &err);
    check(c != NULL && cf_get_int(c, "port", 0) == 9000,
          "a plain scalar becomes an int");
    check(c != NULL && strcmp(cf_get_text(c, "name", ""), "42") == 0,
          "or text, as the definition says");
    cf_config_free(c);

    check(spill("cf_yaml.yaml", "port: \"9000\"\n"), "wrote a quoted number");
    b = cf_build(NULL);
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_yaml.yaml", NULL, 0);
    cf_builder_int(b, "port", 1, 1, 65535, 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, "a quoted number is text, and an int definition refuses it");
    cf_config_free(c);
}

static void refused(const char* text, const char* needle, const char* what) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = load(text, &err);
    check(c == NULL, what);
    has(err.message, needle, what);
    cf_config_free(c);
}

/* Anchors and aliases (D-63): the alias is the anchored node again, and
   each value it holds explains itself at the place it was written. */
static void test_anchors(void) {
    cf_error    err = CF_ERROR_INIT;
    static char ex[1024];
    cf_config*  c = load("base: &b\n"
                         "  host: a\n"
                         "  ports: [1, 2]\n"
                         "other: *b\n"
                         "name: &n plain\n"
                         "again: *n\n"
                         "list: [*n, *n]\n",
                         &err);

    check(c != NULL, "anchors and aliases read");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(strcmp(cf_get_text(c, "other.host", ""), "a") == 0, "an aliased mapping");
    check(cf_array_count(c, "other.ports") == 2 &&
              cf_get_int(c, "other.ports.1", 0) == 2,
          "with its sequence");
    check(strcmp(cf_get_text(c, "again", ""), "plain") == 0, "an aliased scalar");
    check(cf_array_count(c, "list") == 2, "aliases as elements");
    cf_explain(c, "other.host", ex, sizeof ex);
    has(ex,
        "cf_yaml.yaml:2:", "and an aliased value is explained where it was written");
    cf_config_free(c);
}

/* The core schema's tags type a value as YAML 1.2 says. */
static void test_tags(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = load("port: !!int \"80\"\n"
                          "zip: !!str 01234\n"
                          "on: !!bool true\n"
                          "none: !!null ~\n"
                          "t: !!map {a: 1}\n"
                          "plain: ! 12\n",
                          &err);

    check(c != NULL, "core tags read");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_type_of(c, "port") == CF_TYPE_INT && cf_get_int(c, "port", 0) == 80,
          "!!int on a quoted scalar is an int");
    check(cf_type_of(c, "zip") == CF_TYPE_TEXT &&
              strcmp(cf_get_text(c, "zip", ""), "01234") == 0,
          "!!str keeps the zero");
    check(cf_get_bool(c, "on", CF_FALSE), "!!bool");
    check(cf_type_of(c, "none") == CF_TYPE_NULL, "!!null");
    check(cf_get_int(c, "t.a", 0) == 1, "!!map");
    check(cf_type_of(c, "plain") == CF_TYPE_TEXT, "the non-specific ! is text");
    cf_config_free(c);
}

static void test_refuses(void) {
    refused("other: *b\n", "not defined before it", "an alias of no anchor");
    refused("a: &a [1, *a]\n", "inside the node", "an alias inside its own anchor");
    refused("a: &a [x, x, x, x, x, x, x, x]\n"
            "b: &b [*a, *a, *a, *a, *a, *a, *a, *a]\n"
            "c: &c [*b, *b, *b, *b, *b, *b, *b, *b]\n"
            "d: &d [*c, *c, *c, *c, *c, *c, *c, *c]\n"
            "e: &e [*d, *d, *d, *d, *d, *d, *d, *d]\n"
            "f: &f [*e, *e, *e, *e, *e, *e, *e, *e]\n",
            "far more than the document", "a billion laughs, refused in linear time");
    refused("port: !!int eighty\n", "not a !!int", "a core tag on the wrong value");
    refused("port: !custom 80\n", "not one this library resolves",
            "a tag of the writing application's");
    refused("a: 1\n---\nb: 2\n", "a second document", "a second document is refused");
    refused("- a\n- b\n", "a mapping at the top", "a top-level sequence is refused");
    refused("just text\n", "a mapping at the top", "a top-level scalar is refused");
    refused("? [a, b]\n: 1\n", "a key has to be a scalar", "a complex key is refused");
    refused("a: [1, 2\n", "cf_yaml.yaml:", "a syntax error names the place");
    refused("Bad Key: 1\n", "not a valid key", "a key the library cannot hold");
}

/* "base: &b" and a quoted scalar of `len` A's, then `uses` aliases of it
   in one flow sequence. */
static const char* big_anchor(size_t len, unsigned uses) {
    static char doc[96 * 1024];
    size_t      n = 0;
    unsigned    i;

    if (len + uses * 4u + 64u > sizeof doc) {
        return "";
    }
    memcpy(doc, "base: &b \"", 10);
    n = 10;
    memset(doc + n, 'A', len);
    n += len;
    memcpy(doc + n, "\"\nuse: [", 8);
    n += 8;
    for (i = 0; i < uses; ++i) {
        memcpy(doc + n, (i > 0) ? ", *b" : "*b", (i > 0) ? 4u : 2u);
        n += (i > 0) ? 4u : 2u;
    }
    memcpy(doc + n, "]\n", 3);
    return doc;
}

/* The bound on aliases counts bytes as well as events: one large scalar
   aliased many times is a few events and a great deal of memory, since
   each replay copies the text into the configuration. Shared defaults of
   a few kilobytes reused dozens of times are what anchors are for, and
   still load. */
static void test_alias_bytes(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = load(big_anchor(4096, 48), &err);

    check(c != NULL, "a 4 KiB anchor aliased 48 times loads");
    if (c == NULL) {
        printf("  %s\n", err.message);
    }
    check(c != NULL && cf_array_count(c, "use") == 48 &&
              strlen(cf_get_text(c, "use.47", "")) == 4096,
          "and every alias holds the whole text");
    cf_config_free(c);

    c = load(big_anchor(64u * 1024u, 40), &err);
    check(c != NULL, "a 64 KiB anchor aliased 40 times loads");
    if (c == NULL) {
        printf("  %s\n", err.message);
    }
    check(c != NULL && strlen(cf_get_text(c, "use.39", "")) == 64u * 1024u,
          "with the text whole");
    cf_config_free(c);

    /* Two bounds on the same bytes, and whichever trips first is right:
       YAML's own (it counts only the replayed text) and the flattener's
       (D-82, which also counts the first copy and each key, so for a
       large scalar it can come a few aliases sooner). */
    {
        cf_error   e2 = CF_ERROR_INIT;
        cf_config* c2 = load(big_anchor(64u * 1024u, 200), &e2);

        check(c2 == NULL, "a 64 KiB anchor aliased 200 times is refused by its bytes");
        check(strstr(e2.message, "far more than the document") != NULL ||
                  strstr(e2.message, "makes more than") != NULL,
              "by one of the two byte bounds, YAML's or the flattener's");
        cf_config_free(c2);
    }
}

/* A name anchored again: an alias means the latest definition, the inner
   one when an anchor is anchored again inside its own node. */
static void test_reanchor(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = load("a: &x one\n"
                          "b: *x\n"
                          "c: &x two\n"
                          "d: *x\n"
                          "e: &x [&x in, *x]\n"
                          "f: *x\n"
                          "g: &x three\n"
                          "h: *x\n",
                          &err);

    check(c != NULL, "a name anchored again reads");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check_text(cf_get_text(c, "b", ""), "one", "an alias of the first definition");
    check_text(cf_get_text(c, "d", ""), "two", "and of the second, after it");
    check_text(cf_get_text(c, "e.1", ""), "in",
               "inside an anchored node, the inner one");
    check_text(cf_get_text(c, "f", ""), "in", "which stays the latest after the node");
    check_text(cf_get_text(c, "h", ""), "three", "and a third definition replaces it");
    cf_config_free(c);
}

/* A sink that accepts every event and counts the workspace asked of it. */
static char   g_ws[1024 * 1024];
static size_t g_ws_used;

static cf_bool ws_open(void* ctx, const char* name, size_t n) {
    (void)ctx;
    (void)name;
    (void)n;
    return CF_TRUE;
}
static cf_bool ws_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                         cf_bool textual, const cf_loc* at) {
    (void)ctx;
    (void)name;
    (void)n;
    (void)v;
    (void)textual;
    (void)at;
    return CF_TRUE;
}
static cf_bool ws_end(void* ctx) {
    (void)ctx;
    return CF_TRUE;
}
static void* ws_work(void* ctx, size_t n) {
    char* p;
    (void)ctx;
    n = (n + 15u) & ~(size_t)15u;
    if (n > sizeof g_ws - g_ws_used) {
        return NULL;
    }
    p = g_ws + g_ws_used;
    g_ws_used += n;
    return p;
}

/* The workspace a document's anchors take: 1000 definitions of one name,
   or of 1000 names. */
static size_t ws_of(cf_bool one_name) {
    static char     doc[32 * 1024];
    size_t          n = 0;
    unsigned        i;
    cf_doc_sink     out;
    cf_blob         in;
    cf_error        err  = CF_ERROR_INIT;
    const cf_codec* yaml = cf_codec_by_name("yaml");

    for (i = 0; i < 1000; ++i) {
        n +=
            (size_t)sprintf(doc + n, one_name ? "k%u: &x %u\n" : "k%u: &x%u 1\n", i, i);
    }
    memset(&out, 0, sizeof out);
    out.size         = sizeof out;
    out.begin_table  = ws_open;
    out.begin_array  = ws_open;
    out.scalar       = ws_scalar;
    out.end          = ws_end;
    out.reserve_work = ws_work;
    memset(&in, 0, sizeof in);
    in.p      = doc;
    in.n      = n;
    in.name   = "ws.yaml";
    g_ws_used = 0;
    if (yaml == NULL || !yaml->decode(&in, &out, &err)) {
        printf("  %s\n", err.message);
        return 0;
    }
    return g_ws_used;
}

/* A name anchored again replaces its entry rather than adding one, so a
   lookup's chain stays as long as the names are many, not the
   definitions. Seen from outside as workspace: an entry per definition
   costs as much again as the recordings themselves. */
static void test_reanchor_entries(void) {
    size_t one  = ws_of(CF_TRUE);
    size_t many = ws_of(CF_FALSE);
    check(one > 0 && many > 0, "both documents decode");
    check(one * 4u < many * 3u, "one name anchored 1000 times keeps one entry");
    if (!(one * 4u < many * 3u)) {
        printf("  workspace: one name %lu, many names %lu\n", (unsigned long)one,
               (unsigned long)many);
    }
}

/* 149 bytes beginning with '%' and malformed UTF-8 in the directive,
   which libfyaml 0.8 loops on forever (libfyaml issue 101): refused, by
   either parser, rather than read or hung on. */
static void test_hostile_directive(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;

    cf_builder_file_codec(b, "file", CF_RANK_USER_FILE,
                          CF_CONFORM_DATA "/libfyaml-0.8-hang.yaml.bin",
                          cf_codec_by_name("yaml"), 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, "a malformed directive is refused, not hung on");
    has(err.message, "libfyaml-0.8-hang.yaml.bin", "and the refusal names the file");
    cf_config_free(c);
}

static void test_empty(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = load("# nothing but a comment\n", &err);
    check(c != NULL && cf_count(c) == 0, "an empty file is an empty configuration");
    cf_config_free(c);
}

#if CF_WITH_EMIT
/* What YAML writes, YAML reads, and it is the same configuration. */
static void test_round_trip(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  a   = load(g_doc, &err);
    cf_config*  b;
    static char out[8192];
    size_t      n;

    if (a == NULL) {
        check(0, "round trip: the original builds");
        return;
    }
    n = cf_emit(a, "yaml", CF_EMIT_ALL, 0, out, sizeof out, &err);
    check(n > 0 && n < sizeof out, "the configuration is written as YAML");
    has(out, "country: \"no\"\n", "a string that looks like a word is quoted");
    has(out, "  port: 8080\n", "a number is not");
    cf_config_free(a);
    if (n == 0) {
        printf("  %s\n", err.message);
        return;
    }
    a = load(g_doc, &err);
    b = load(out, &err);
    check(b != NULL, "and what was written parses");
    if (b == NULL) {
        printf("  %s\n--- written:\n%s\n", err.message, out);
        cf_config_free(a);
        return;
    }
    check(a != NULL && cf_digest(a) == cf_digest(b), "as the same configuration");
    if (a != NULL && cf_digest(a) != cf_digest(b)) {
        static char ia[8192], ib[8192];
        cf_inspect(a, ia, sizeof ia);
        cf_inspect(b, ib, sizeof ib);
        printf("--- written:\n%s\n--- original:\n%s\n--- read back:\n%s\n", out, ia,
               ib);
    }
    cf_config_free(a);
    cf_config_free(b);
}
#endif

/* A reader seeds its name tables a decode (D-81). Nothing a caller can see
   may depend on the seed, so the same document read twice, through two
   arenas that lie at different addresses, gives the same keys in the same
   order, with the same values and places. */
static cf_bool seeded_read(const char* fmt, const char* text, cf_arena* ar,
                           char** dump) {
    const cf_codec* c = cf_codec_by_name(fmt);
    cf_keyspace     ks;
    cf_flatten      fl;
    cf_blob         b;
    cf_error        err = CF_ERROR_INIT;
    size_t          i, n = 0;
    char*           out;

    if (c == NULL) {
        return CF_FALSE;
    }
    cf_ks_init(&ks, ar, 16);
    cf_blob_mem(&b, text, strlen(text), "doc");
    cf_flatten_init(&fl, &ks, 10);
    if (!c->decode(&b, cf_flatten_sink(&fl), &err)) {
        return CF_FALSE;
    }
    cf_ks_resolve(&ks);
    cf_ks_infer_untyped(&ks);
    out = (char*)malloc(cf_ks_count(&ks) * 200 + 64);
    if (out == NULL) {
        return CF_FALSE;
    }
    for (i = 0; i < cf_ks_count(&ks); ++i) {
        const cf_entry* e = cf_ks_at(&ks, i);
        n += (size_t)sprintf(out + n, "%.*s|%lu|%.100s\n", (int)e->keylen, e->key,
                             (unsigned long)cf_ks_value_digest(&e->chain->value),
                             e->chain->at);
    }
    out[n] = '\0';
    *dump  = out;
    return CF_TRUE;
}

static void seeded_same(const char* fmt, const char* text, const char* what) {
    cf_arena* a1  = cf_arena_new(NULL, 1 << 12);
    cf_arena* a2  = cf_arena_new(NULL, 1 << 16);
    char*     d1  = NULL;
    char*     d2  = NULL;
    cf_bool   ok1 = seeded_read(fmt, text, a1, &d1);
    cf_bool   ok2 = seeded_read(fmt, text, a2, &d2);

    check(ok1 && ok2, "the document reads twice");
    if (ok1 && ok2) {
        check(strcmp(d1, d2) == 0, what);
    }
    free(d1);
    free(d2);
    cf_arena_free(a1);
    cf_arena_free(a2);
}

static const char g_seeded_doc[] = "base: &b\n  host: a\n  ports: [1, 2]\n"
                                   "other: *b\n"
                                   "name: &n plain\n"
                                   "name2: &m plain\n"
                                   "again: *n\n"
                                   "more: *m\n"
                                   "name3: &n again\n"
                                   "list: [*n, *n]\n";

static void test_seeded(void) {
    seeded_same("yaml", g_seeded_doc, "YAML read twice is the same");
}

#else /* !CF_WITH_YAML */

/* Without it, a .yaml file is not read as nothing: the build says which
   option it needs. */
static void test_disabled(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;

    check(spill("cf_yaml.yaml", "port: 1\n"), "wrote a YAML file");
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_yaml.yaml", NULL, 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, "a YAML file is refused without CF_WITH_YAML");
    has(err.message, "CF_WITH_YAML", "and the error names the option");
    cf_config_free(c);
}

#endif /* CF_WITH_YAML */

int main(void) {
#if CF_WITH_YAML
    test_reads();
    test_types();
    test_anchors();
    test_tags();
    test_refuses();
    test_alias_bytes();
    test_reanchor();
    test_reanchor_entries();
    test_hostile_directive();
    test_empty();
    test_seeded();
#if CF_WITH_EMIT
    test_round_trip();
#endif
#else
    test_disabled();
#endif
    remove("cf_yaml.yaml");
    return cf_test_done();
}
