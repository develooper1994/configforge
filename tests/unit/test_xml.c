/*
 * test_xml.c - XML through expat (D-23): the mapping it declares, what
 * it refuses by name, and that an XML document and the JSON written for
 * the same configuration are the same configuration.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

static int  g_fail = 0, g_run = 0;
static void check(int c, const char* w) {
    ++g_run;
    if (!c) {
        ++g_fail;
        printf("FAIL: %s\n", w);
    }
}
static void has(const char* hay, const char* needle, const char* w) {
    ++g_run;
    if (hay == NULL || strstr(hay, needle) == NULL) {
        ++g_fail;
        printf("FAIL: %s  missing \"%s\" in \"%s\"\n", w, needle, hay ? hay : "(null)");
    }
}

static cf_bool spill(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        return CF_FALSE;
    }
    fputs(text, f);
    fclose(f);
    return CF_TRUE;
}

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
#else
    test_disabled();
#endif
    remove("cf_xml.xml");
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
