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

#include "cf_test.h"

#if CF_WITH_REPORT && CF_WITH_JSON

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
    } else if (strcmp(w->path, "properties.plugins.additionalProperties") == 0 &&
               v->type == CF_TYPE_BOOL) {
        w->plugins_closed = v->u.b ? 2 : 1;
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
    memset(&sink, 0, sizeof sink);
    sink.size        = sizeof(cf_doc_sink);
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
    check(w.plugins_closed == 2, "and an opened one says it is open");
}

/* Reading a schema back (cf_defset_schema): the contract written out and
   read in is the same contract, written out again byte for byte. */
static void test_read_round_trip(void) {
    static char one[8192], two[8192];
    cf_config*  c = contract(CF_UNKNOWN_REJECT);
    cf_config*  back;
    cf_builder* b;
    cf_error    err = CF_ERROR_INIT;
    size_t      n;

    if (c == NULL) {
        check(0, "round trip: the contract builds");
        return;
    }
    n = cf_schema(c, "myapp", one, sizeof one);
    cf_config_free(c);
    b = cf_build(NULL);
    check(cf_builder_schema(b, one, n, "myapp.schema.json"), "the schema is read");
    cf_builder_unknown(b, CF_UNKNOWN_REJECT);
    back = cf_builder_create(b, &err);
    check(back != NULL, "and the contract it describes builds");
    if (back == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    cf_schema(back, "myapp", two, sizeof two);
    check(strcmp(one, two) == 0, "and writes the same schema back");
    if (strcmp(one, two) != 0) {
        printf("--- written:\n%s\n--- read and written again:\n%s\n", one, two);
    }
    check(strcmp(cf_get_text(back, "server.host", ""), "0.0.0.0") == 0,
          "a default, read");
    cf_config_free(back);
}

static const cf_def* schema_def(cf_defset* ds, cf_arena* ar, const char* text,
                                const char* key) {
    cf_error err = CF_ERROR_INIT;
    if (!cf_defset_init(ds, ar, 8) ||
        !cf_defset_schema(ds, text, strlen(text), "s.json", &err)) {
        printf("  %s\n", err.message);
        return NULL;
    }
    return cf_defset_find(ds, key, strlen(key));
}

/* What JSON Schema says beyond what the writer writes. */
static void test_read_keywords(void) {
    cf_arena*     ar = cf_arena_new(NULL, 0);
    cf_defset     ds;
    const cf_def* d;

    d = schema_def(&ds, ar,
                   "{\"properties\": {\"n\": {\"type\": \"integer\", "
                   "\"exclusiveMinimum\": 0, \"exclusiveMaximum\": 10}}}",
                   "n");
    check(d != NULL && d->has_range && d->imin == 1 && d->imax == 9,
          "an integer's exclusive bounds are its inclusive ones, one in");
    d = schema_def(
        &ds, ar,
        "{\"required\": [\"k\"], \"properties\": {\"k\": {\"type\": \"string\", "
        "\"title\": \"The key\", \"writeOnly\": true}}}",
        "k");
    check(d != NULL && (d->flags & CF_REQUIRED) && (d->flags & CF_SENSITIVE),
          "required and writeOnly are the flags");
    check(d != NULL && d->description != NULL && strcmp(d->description, "The key") == 0,
          "and a title is the description when there is none");
    d = schema_def(&ds, ar,
                   "{\"properties\": {\"ports\": {\"type\": \"array\", \"items\": "
                   "{\"type\": \"integer\", \"minimum\": 1}, \"default\": [80, 443]}}}",
                   "ports");
    check(d != NULL && d->list_mode == CF_LIST_REPLACE && d->type == CF_TYPE_INT &&
              d->has_default && d->list_default_count == 2,
          "an array of integers is a list, its default the list's");
    cf_arena_free(ar);
}

static void schema_refused(const char* text, const char* needle, const char* what) {
    cf_arena* ar  = cf_arena_new(NULL, 0);
    cf_error  err = CF_ERROR_INIT;
    cf_defset ds;
    check(cf_defset_init(&ds, ar, 8) &&
              !cf_defset_schema(&ds, text, strlen(text), "s.json", &err),
          what);
    has(err.message, needle, what);
    has(err.message, "s.json", "and the refusal names the schema");
    cf_arena_free(ar);
}

/* A constraint the library cannot enforce is refused, never dropped. */
static void test_read_refusals(void) {
    schema_refused("{\"properties\": {\"a\": {\"$ref\": \"#/$defs/a\"}}}",
                   "'$ref' is a JSON Schema keyword this library does not read",
                   "a $ref");
    schema_refused("{\"allOf\": [{}]}", "'allOf'", "a combinator");
    schema_refused(
        "{\"properties\": {\"a\": {\"type\": \"string\", \"pattern\": \"x\"}}}",
        "'pattern'", "a pattern");
    schema_refused("{\"properties\": {\"a\": {\"type\": [\"string\", \"null\"]}}}",
                   "not a type this library reads", "a type that is a list");
    schema_refused("{\"required\": [\"b\"], \"properties\": {\"a\": {\"type\": "
                   "\"string\"}}}",
                   "'b' is required and is not a property",
                   "a required name not there");
    schema_refused("{\"properties\": {\"a\": {\"type\": \"string\", \"enum\": [1]}}}",
                   "enum of a string holds strings", "an enum of numbers for a string");
    schema_refused("{\"$schema\": \"http://json-schema.org/draft-07/schema#\"}",
                   "draft this library does not read", "an older draft");
    schema_refused(
        "{\"properties\": {\"a\": {\"type\": \"integer\", \"default\": \"x\"}}}",
        "the default is not a value of its type", "a default of another type");
    schema_refused("{\"properties\": {\"a\": {\"type\": \"integer\", \"minimum\": 5, "
                   "\"default\": 1}}}",
                   "s.json:1:", "a default outside its range, with the place");
    schema_refused("{\"properties\": {\"a\": {}}}", "has no type",
                   "a property with no type");
    schema_refused("{\"properties\": ", "s.json", "JSON that does not close");
}

#endif /* CF_WITH_REPORT && CF_WITH_JSON */

int main(void) {
#if CF_WITH_REPORT && CF_WITH_JSON
    test_keywords();
    test_structure();
    test_read_round_trip();
    test_read_keywords();
    test_read_refusals();
#else
    static char out[64];
    cf_config*  none = NULL;
    check(cf_schema(none, "x", out, sizeof out) == 0,
          "nothing without a configuration");
    printf("skipped: this build has no JSON Schema (CF_WITH_REPORT or CF_WITH_JSON "
           "off)\n");
#endif
    return cf_test_done();
}
