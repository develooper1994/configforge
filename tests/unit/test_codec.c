/*
 * test_codec.c - a codec written outside the library.
 *
 * This is the shape of an integration with another parser - libyaml,
 * expat, anything that streams: translate its events into cf_doc_sink
 * calls, with locations, and hand the codec to the builder. Nothing
 * here uses anything but the public headers, which is the point. The
 * format below is deliberately tiny ("key: value" per line, "# "
 * comments) so that what is left is the integration itself.
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

/* ------------------------------------------------------------------ *
 * The codec: one document, one table, a scalar per line.
 *
 * A dotted key needs no nesting of its own here - "server.port" is one
 * key to the flattener - and the values are text, because this format
 * carries no types: the definitions decide, as they do for INI.
 * ------------------------------------------------------------------ */

static cf_bool kv_decode(const cf_blob* in, const cf_doc_sink* out, cf_error* err) {
    const char*  p    = in->p;
    const char*  end  = in->p + in->n;
    unsigned int line = 1;

    if (!out->begin_table(out->ctx, NULL, 0)) {
        return cf_error_set(err, CF_E_SOURCE, NULL, in->name, "sink refused");
    }
    while (p < end) {
        const char* eol = p;
        const char* colon;
        cf_value    v;
        cf_loc      at;
        const char* val;
        size_t      klen, vlen;

        while (eol < end && *eol != '\n') {
            ++eol;
        }
        if (eol > p && *p != '#') {
            colon = p;
            while (colon < eol && *colon != ':') {
                ++colon;
            }
            if (colon == eol) {
                return cf_error_set(err, CF_E_SOURCE, NULL, in->name,
                                    "%s:%u: expected 'key: value'", in->name, line);
            }
            klen = (size_t)(colon - p);
            val  = colon + 1;
            while (val < eol && *val == ' ') {
                ++val;
            }
            vlen = (size_t)(eol - val);
            if (vlen > 0 && val[vlen - 1] == '\r') {
                --vlen;
            }
            at.file = in->name;
            at.line = line;
            at.col  = (unsigned int)(val - p) + 1u;
            cf_value_text(&v, val, vlen);
            if (!out->scalar(out->ctx, p, klen, &v, CF_TRUE, &at)) {
                return cf_error_set(err, CF_E_KEY, NULL, in->name,
                                    "%s:%u: not a valid key", in->name, line);
            }
        }
        p = (eol < end) ? eol + 1 : eol;
        ++line;
    }
    return out->end(out->ctx);
}

static const cf_codec kv_codec = {"kv", ".kv", CF_CODEC_READ, kv_decode, NULL, 0};

/* ------------------------------------------------------------------ */

static void test_reads_through_it(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;
    static char ex[2048];

    check(spill("cf_codec.kv", "# written for a codec the library does not have\n"
                               "server.port: 9443\n"
                               "server.host: example.org\n"),
          "wrote a file in a foreign format");
    cf_builder_file_codec(b, "file", CF_RANK_USER_FILE, "cf_codec.kv", &kv_codec, 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "0.0.0.0", 0);
    c = cf_builder_create(b, &err);
    check(c != NULL, "a codec handed to the builder is used");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_get_int(c, "server.port", 0) == 9443,
          "its text is typed by the definition, like any textual format");
    check(strcmp(cf_get_text(c, "server.host", ""), "example.org") == 0, "all of it");
    cf_explain(c, "server.port", ex, sizeof ex);
    has(ex, "cf_codec.kv:2:", "and the locations it reports reach explain()");
    cf_config_free(c);
}

static void test_the_contract_still_applies(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;

    check(spill("cf_codec.kv", "server.port: 99999\n"), "wrote an out-of-range value");
    cf_builder_file_codec(b, "file", CF_RANK_USER_FILE, "cf_codec.kv", &kv_codec, 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, "a value from a foreign codec is validated like any other");
    has(err.source, "cf_codec.kv:1", "and the error names the place");
    cf_config_free(c);
}

static void test_without_it(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;

    /* The same file by extension: the library does not know ".kv", and
       says so rather than reading it as something else. */
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_codec.kv", NULL, 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, "without the codec the format is unknown");
    has(err.message, "cannot tell the format", "and the error says so");
    cf_config_free(c);

    b = cf_build(NULL);
    cf_builder_file_codec(b, "file", CF_RANK_USER_FILE, "cf_codec.kv", NULL, 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, "a missing codec is refused at construction");
    has(err.message, "no codec", "and named");
    cf_config_free(c);
}

int main(void) {
    test_reads_through_it();
    test_the_contract_still_applies();
    test_without_it();
    remove("cf_codec.kv");
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
