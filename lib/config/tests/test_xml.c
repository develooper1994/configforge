/*
 * test_xml.c - XML through expat (D-23): the mapping it declares, what
 * it refuses by name, and that an XML document and the JSON written for
 * the same configuration are the same configuration.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cf_test.h"

#if CF_WITH_XML

static cf_config* load_as(const char* path, const char* text, cf_error* err) {
    cf_builder* b = cf_build(NULL);
    if (!spill(path, text)) {
        cf_builder_abort(b);
        return NULL;
    }
    cf_builder_file(b, "file", CF_RANK_USER_FILE, path, NULL, 0);
    return cf_builder_create(b, err);
}

static cf_config* load(const char* text, cf_error* err) {
    return load_as("cf_xml.xml", text, err);
}

static const char g_doc[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                            "<!-- a comment -->\n"
                            "<config title=\"ConfigForge\">\n"
                            "  <server port=\"8080\">\n"
                            "    <host> 0.0.0.0 </host>\n"
                            "    <tls enabled=\"true\" cert=\"c\xc3\xa9rt.pem\"/>\n"
                            "    <tag>a</tag>\n"
                            "    <tag>b &amp; c</tag>\n"
                            "  </server>\n"
                            "  <upstream><host>a</host></upstream>\n"
                            "  <upstream><host>b</host><weight>2</weight></upstream>\n"
                            "  <motd><![CDATA[two <lines>]]></motd>\n"
                            "  <empty/>\n"
                            "</config>\n";

static void test_reads(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c   = load(g_doc, &err);
    static char ex[2048];

    check(c != NULL, "an XML document builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(strcmp(cf_get_text(c, "title", ""), "ConfigForge") == 0,
          "the root's attributes are the top level, its name in no key");
    check(cf_get_int(c, "server.port", 0) == 8080, "an attribute, by Tier 0 inference");
    check(strcmp(cf_get_text(c, "server.host", ""), "0.0.0.0") == 0,
          "an element's text, its surrounding whitespace trimmed");
    check(cf_get_bool(c, "server.tls.enabled", CF_FALSE),
          "an empty element's attributes");
    check(strcmp(cf_get_text(c, "server.tls.cert", ""), "c\xc3\xa9rt.pem") == 0,
          "with UTF-8 in them");
    check(cf_array_count(c, "server.tag") == 2, "repeated siblings are a list");
    check(strcmp(cf_get_text(c, "server.tag.1", ""), "b & c") == 0,
          "its values by index, entities decoded");
    check(cf_array_count(c, "upstream") == 2, "a list of elements with children");
    check(cf_get_int(c, "upstream.1.weight", 0) == 2, "and the keys inside them");
    check(strcmp(cf_get_text(c, "motd", ""), "two <lines>") == 0, "CDATA is text");
    check(cf_has(c, "empty") && strcmp(cf_get_text(c, "empty", "x"), "") == 0,
          "an empty element is an empty value");

    /* explain() names the line each value came from. */
    cf_explain(c, "server.host", ex, sizeof ex);
    has(ex, "cf_xml.xml:5:", "explain names the file and the line");
    cf_config_free(c);
}

/* The mapping, all of it at once: an XML document and the JSON a person
   would write for the same configuration resolve to the same digest. */
static void test_same_as_json(void) {
    static const char xml[] = "<c a=\"1\"><b><x>2</x><y>3</y></b>"
                              "<l>p</l><l>q</l><t><k>1</k></t><t><k>2</k></t></c>";
    /* Numbers, not strings: XML's values are untyped text, so Tier 0
       infers what JSON states. */
    static const char json[] =
        "{\"a\": 1, \"b\": {\"x\": 2, \"y\": 3},"
        " \"l\": [\"p\", \"q\"], \"t\": [{\"k\": 1}, {\"k\": 2}]}";
    cf_error   err = CF_ERROR_INIT;
    cf_config* from_xml;
    cf_config* from_json;

    from_xml  = load(xml, &err);
    from_json = load_as("cf_xml.json", json, &err);
    check(from_xml != NULL && from_json != NULL, "both build");
    if (from_xml != NULL && from_json != NULL) {
        check(cf_digest(from_xml) == cf_digest(from_json),
              "an XML document is the configuration its JSON would be");
        if (cf_digest(from_xml) != cf_digest(from_json)) {
            static char a[4096], b[4096];
            cf_inspect(from_xml, a, sizeof a);
            cf_inspect(from_json, b, sizeof b);
            printf("xml:\n%s\njson:\n%s\n", a, b);
        }
    } else {
        printf("  %s\n", err.message);
    }
    cf_config_free(from_xml);
    cf_config_free(from_json);
    remove("cf_xml.json");
}

static void refused(const char* text, const char* why, const char* what) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = load(text, &err);
    check(c == NULL, what);
    if (c == NULL) {
        has(err.message, why, what);
    }
    cf_config_free(c);
}

static void test_refuses(void) {
    refused("<!DOCTYPE c [<!ENTITY x \"y\">]><c>&x;</c>", "a DOCTYPE is not read",
            "a DOCTYPE, and with it every entity, is refused");
    refused("<c a=\"1\"><a>2</a></c>",
            "an attribute and a child element have the same name",
            "an attribute and an element of one name collide");
    refused("<c><l>1</l><m>2</m><l>3</l></c>", "must be next to each other",
            "a list whose elements are apart is refused");
    refused("<c><s>text<x>1</x></s></c>", "text beside child elements",
            "mixed content is refused");
    refused("<c><s unit=\"ms\">5</s></c>", "text in an element with attributes",
            "an attribute beside text is refused");
    refused("<c xmlns:p=\"u\"><p:x>1</p:x></c>", "namespaces are not",
            "a namespaced element is refused");
    refused("<c>just text</c>", "the root element holds settings",
            "a root with only text is refused");
    refused("<c><a>1</a>", "cf_xml.xml:1:", "a syntax error names the place");
    refused("<c><Bad>1</Bad></c>", "not a valid key", "a name the library cannot hold");
}

static void test_empty_root(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = load("<config/>", &err);
    check(c != NULL && cf_count(c) == 0, "an empty root is an empty configuration");
    cf_config_free(c);
}

/* Typed by the contract, as INI is: the document says nothing of types. */
static void test_typed_by_contract(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;

    check(spill("cf_xml.xml", "<c><port>8080</port><name>007</name></c>"), "wrote it");
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_xml.xml", NULL, 0);
    cf_builder_int(b, "port", 1, 1, 65535, 0);
    cf_builder_text(b, "name", "", 0);
    c = cf_builder_create(b, &err);
    check(c != NULL && cf_get_int(c, "port", 0) == 8080, "a defined int");
    check(c != NULL && strcmp(cf_get_text(c, "name", ""), "007") == 0,
          "and a defined text keeps its leading zeros");
    cf_config_free(c);
}

/* Which earlier name a child element's name meets decides whether it
   continues a list, collides with an attribute, or is refused as a list
   apart; only names at the same level, under the same element, count. */
static void test_names_by_level(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c;

    c = load("<c><n>1</n><n>2</n></c>", &err);
    check(c != NULL && cf_array_count(c, "n") == 2,
          "one name twice at one level, side by side, is a list");
    cf_config_free(c);

    c = load("<c><x><n>1</n></x><n>2</n></c>", &err);
    check(c != NULL && cf_get_int(c, "x.n", 0) == 1 && cf_get_int(c, "n", 0) == 2,
          "one name at two levels is two keys, not a list");
    check(c != NULL && cf_array_count(c, "n") == 0, "and neither is a list");
    cf_config_free(c);

    c = load("<c><n><n>1</n></n></c>", &err);
    check(c != NULL && cf_get_int(c, "n.n", 0) == 1, "a name inside itself");
    cf_config_free(c);

    c = load("<c><p><n>1</n></p><q><n>2</n></q></c>", &err);
    check(c != NULL && cf_get_int(c, "p.n", 0) == 1 && cf_get_int(c, "q.n", 0) == 2,
          "one name under two parents is two keys");
    cf_config_free(c);

    c = load("<c><p><n>1</n></p><p><n>2</n></p></c>", &err);
    check(c != NULL && cf_array_count(c, "p") == 2 && cf_get_int(c, "p.1.n", 0) == 2,
          "a name under each member of a list is no list of its own");
    cf_config_free(c);

    c = load("<c a=\"1\"><b><a>2</a></b></c>", &err);
    check(c != NULL && cf_get_int(c, "a", 0) == 1 && cf_get_int(c, "b.a", 0) == 2,
          "an attribute and a child of one name at different levels");
    cf_config_free(c);

    refused("<c><a/><b><a/><x/></b><a/></c>", "must be next to each other",
            "a name met again after a subtree that used it is still apart");
    c = load("<c><b a=\"1\"/><b><a>2</a></b></c>", &err);
    check(c != NULL && cf_get_int(c, "b.1.a", 0) == 2,
          "each member of a list is its own level");
    cf_config_free(c);
}

/* Each child's name asks only its own level's names, and asks them in
   constant time: 40,000 distinct siblings once took a scan of every
   earlier one each. */
static void test_many_names(void) {
    const unsigned long count = 40000ul;
    size_t              room = 64 + (size_t)count * 12, at = 0;
    char*               text = (char*)malloc(room);
    cf_error            err  = CF_ERROR_INIT;
    cf_config*          c;
    unsigned long       i;

    check(text != NULL, "room for the document");
    if (text == NULL) {
        return;
    }
    memcpy(text, "<r>", 3);
    at = 3;
    for (i = 0; i < count; ++i) {
        sprintf(text + at, "<a%06lu/>", i);
        at += 10;
    }
    memcpy(text + at, "</r>", 5);
    c = load(text, &err);
    free(text);
    check(c != NULL, "40,000 distinct sibling names build");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_count(c) == (size_t)count, "every one of them a key");
    check(cf_has(c, "a000000") && cf_has(c, "a039999"), "the first and the last");
    cf_config_free(c);
}

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

static const char g_seeded_doc[] =
    "<r k=\"1\"><a>1</a><b><c>2</c><d>3</d></b><e>4</e><f/>"
    "<t>x</t><t>y</t><u><a>5</a></u><u><a>6</a><c>7</c></u></r>";

static void test_seeded(void) {
    seeded_same("xml", g_seeded_doc, "XML read twice is the same");
}

#else

/* Without it, an .xml file is not read as nothing: the build says which
   option it needs. */
static void test_disabled(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;

    check(spill("cf_xml.xml", "<c/>"), "wrote an XML file");
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_xml.xml", NULL, 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, "an XML file is refused without CF_WITH_XML");
    has(err.message, "CF_WITH_XML", "and the error names the option");
    check(!cf_has_feature("xml"), "and the build does not claim it");
    cf_config_free(c);
}

#endif /* CF_WITH_XML */

int main(void) {
#if CF_WITH_XML
    check(cf_has_feature("xml"), "the build says it reads XML");
    test_reads();
    test_same_as_json();
    test_refuses();
    test_empty_root();
    test_typed_by_contract();
    test_names_by_level();
    test_many_names();
    test_seeded();
#else
    test_disabled();
#endif
    remove("cf_xml.xml");
    return cf_test_done();
}
