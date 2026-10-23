/*
 * test_schema.c - the contract as a JSON Schema an editor can use.
 *
 * Two kinds of check. The text is searched for the keywords each kind of
 * definition should produce; and the schema is decoded by the library's
 * own JSON reader into a sink that follows the paths, which proves it is
 * JSON and checks the structure by path - properties.server.properties.
 * port.maximum is 65535 - rather than by the layout of the text. Not by
 * loading it as a configuration: "$schema" is not a configuration key.
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
#if CF_WITH_REPORT && CF_WITH_JSON

static void has(const char* hay, const char* needle, const char* w) {
    ++g_run;
    if (hay == NULL || strstr(hay, needle) == NULL) {
        ++g_fail;
        printf("FAIL: %s  missing \"%s\" in \"%s\"\n", w, needle, hay ? hay : "(null)");
    }
}

static cf_config* contract(cf_unknown_policy unknown) {
    static const char* const levels[]   = {"debug", "info", "warn", NULL};
    static const cf_pair     supplied[] = {{"server.port", "9000"}};
    cf_error                 err        = CF_ERROR_INIT;
    cf_builder*              b          = cf_build(NULL);
    cf_def                   d;
    cf_config*               c;

    /* Required: a default does not satisfy it, a source has to. */
    cf_builder_memory(b, "memory", CF_RANK_ENV, supplied, 1);
    cf_builder_int(b, "server.port", 8080, 1, 65535, CF_REQUIRED);
    cf_builder_text(b, "server.host", "0.0.0.0", 0);
    cf_builder_bool(b, "tls.enabled", CF_FALSE, 0);
    cf_builder_text(b, "db.password", "default-secret", CF_SENSITIVE);
    cf_def_init(&d, CF_TYPE_TEXT, "log.level");
    d.has_default = CF_TRUE;
    cf_value_text(&d.default_value, "info", 4);
    d.allowed = levels;
    cf_builder_define(b, &d);

    cf_def_init(&d, CF_TYPE_TEXT, "log.file");
    d.description = "Where the log goes; \"-\" is standard output";
    d.flags       = CF_DEPRECATED;
    cf_builder_define(b, &d);

    cf_builder_open(b, "plugins");
    cf_builder_unknown(b, unknown);
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        printf("  %s\n", err.message);
    }
    return c;
}

static void test_keywords(void) {
    cf_config*  c = contract(CF_UNKNOWN_REJECT);
    static char out[8192];
    size_t      n;

    check(c != NULL, "the contract builds");
    if (c == NULL) {
        return;
    }
    n = cf_schema(c, "myapp", out, sizeof out);
    check(n > 0 && n < sizeof out, "a schema is written");
    check(cf_schema(c, "myapp", NULL, 0) == n, "and can be measured first");

    has(out, "\"$schema\": \"https://json-schema.org/draft/2020-12/schema\"",
        "the dialect");
    has(out, "\"title\": \"myapp\"", "the title");
    /* Required, so its default is never used and is not offered. */
    has(out, "\"port\": { \"type\": \"integer\", \"minimum\": 1, \"maximum\": 65535 }",
        "an integer with its range");
    has(out, "\"host\": { \"type\": \"string\", \"default\": \"0.0.0.0\" }",
        "a text with its default");
    has(out, "\"enabled\": { \"type\": \"boolean\", \"default\": false }", "a boolean");
    has(out, "\"enum\": [\"debug\", \"info\", \"warn\"]", "an enumeration");
    has(out, "\"description\": \"Where the log goes; \\\"-\\\" is standard output\"",
        "a description, escaped");
    has(out, "\"deprecated\": true", "a deprecation");
    has(out, "\"password\": { \"type\": \"string\", \"writeOnly\": true }",
        "a secret: write-only, and without its default");
    check(strstr(out, "default-secret") == NULL, "the secret's default is not in it");
    check(strstr(out, "\"required\"") == NULL,
          "and nothing is required: a required key may come from the environment");
    has(out, "\"additionalProperties\": false",
        "rejecting unknown keys closes the objects");
    has(out, "\"plugins\": {\n", "an open prefix is described");
    cf_config_free(c);

    c = contract(CF_UNKNOWN_IGNORE);
    check(c != NULL, "the same contract, ignoring unknown keys");
    if (c != NULL) {
        cf_schema(c, NULL, out, sizeof out);
        check(strstr(out, "additionalProperties") == NULL, "leaves the objects open");
        check(strstr(out, "\"title\"") == NULL, "and a NULL title is left out");
        cf_config_free(c);
    }
}

/* A sink that follows the document's paths and records what the test
   asks about. */
typedef struct walk {
    char    path[512];
    size_t  len;
    size_t  saved[32];
    int     depth;
    cf_bool overflow;
    cf_i64  port_max;
    int     enum_items;
    int     server_closed; /* 1 false, 2 true, 0 absent */
    int     plugins_closed;
    cf_bool root_object;
} walk;

static void walk_name(walk* w, const char* name, size_t n) {
    if (name == NULL) {
        return; /* an array element, or the root */
    }
    if (w->len + n + 2 > sizeof w->path) {
        w->overflow = CF_TRUE;
        return;
    }
    if (w->len > 0) {
        w->path[w->len++] = '.';
    }
    memcpy(w->path + w->len, name, n);
    w->len += n;
    w->path[w->len] = '\0';
}

static cf_bool walk_open(void* ctx, const char* name, size_t n) {
    walk* w = (walk*)ctx;
    if (w->depth >= 32) {
        return CF_FALSE;
    }
    w->saved[w->depth++] = w->len;
    walk_name(w, name, n);
    return CF_TRUE;
}

static cf_bool walk_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                           cf_bool textual, const cf_loc* at) {
    walk*  w    = (walk*)ctx;
    size_t keep = w->len;

    (void)textual;
    (void)at;
    walk_name(w, name, n);
    if (strcmp(w->path, "properties.server.properties.port.maximum") == 0 &&
        v->type == CF_TYPE_INT) {
        w->port_max = v->u.i;
    } else if (strcmp(w->path, "properties.log.properties.level.enum") == 0 &&
               name == NULL) {
        ++w->enum_items;
    } else if (strcmp(w->path, "properties.server.additionalProperties") == 0 &&
               v->type == CF_TYPE_BOOL) {
        w->server_closed = v->u.b ? 2 : 1;
    } else if (strcmp(w->path, "properties.plugins.additionalProperties") == 0) {
        w->plugins_closed = 1;
    } else if (strcmp(w->path, "type") == 0 && v->type == CF_TYPE_TEXT &&
               v->u.t.n == 6 && memcmp(v->u.t.p, "object", 6) == 0) {
        w->root_object = CF_TRUE;
    }
    w->len          = keep;
    w->path[w->len] = '\0';
    return CF_TRUE;
}

static cf_bool walk_end(void* ctx) {
    walk* w = (walk*)ctx;
    if (w->depth > 0) {
        w->len          = w->saved[--w->depth];
        w->path[w->len] = '\0';
    }
    return CF_TRUE;
}

static void test_structure(void) {
    cf_config*      c = contract(CF_UNKNOWN_REJECT);
    static char     out[8192];
    static walk     w;
    cf_error        err  = CF_ERROR_INIT;
    const cf_codec* json = cf_codec_by_name("json");
    cf_doc_sink     sink;
    cf_blob         blob;
    size_t          n;

    if (c == NULL || json == NULL) {
        check(0, "the contract builds and JSON is compiled in");
        cf_config_free(c);
        return;
    }
    n = cf_schema(c, "myapp", out, sizeof out);
    cf_config_free(c);

    memset(&w, 0, sizeof w);
    sink.begin_table = walk_open;
    sink.begin_array = walk_open;
    sink.scalar      = walk_scalar;
    sink.end         = walk_end;
    sink.ctx         = &w;
    blob.p           = out;
    blob.n           = n;
    blob.name        = "schema.json";
    check(json->decode(&blob, &sink, &err), "the schema is valid JSON");
    if (err.message[0] != '\0') {
        printf("  %s\n", err.message);
    }
    check(!w.overflow, "and its paths fit");
    check(w.root_object, "the root is an object");
    check(w.port_max == 65535, "server.port is nested where an editor looks for it");
    check(w.enum_items == 3, "the enumeration is an array of three");
    check(w.server_closed == 1, "a defined object is closed");
    check(w.plugins_closed == 0, "and an opened one is not");
}

#endif /* CF_WITH_REPORT && CF_WITH_JSON */

int main(void) {
#if CF_WITH_REPORT && CF_WITH_JSON
    test_keywords();
    test_structure();
#else
    static char out[64];
    cf_config*  none = NULL;
    check(cf_schema(none, "x", out, sizeof out) == 0,
          "nothing without a configuration");
    printf("skipped: this build has no JSON Schema (CF_WITH_REPORT or CF_WITH_JSON "
           "off)\n");
#endif
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
