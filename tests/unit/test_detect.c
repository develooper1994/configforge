/*
 * test_detect.c - the format from the name, then the content (D-73): a
 * file with no extension read as each built-in format, a renamed file
 * refused with a hint, an empty or ambiguous one refused, a document held
 * in memory, and the builder's own codec taken from content first.
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200112L /* setenv */
#endif

#include <configforge/configforge.h>
#include <configforge/cf_source.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cf_kv_codec.h"
#include "cf_test.h"

/* server.port from a file read with whatever format its content is; -1
   with `err` set when it was refused. */
static int port_of(cf_builder* b, cf_error* err) {
    cf_config* c;
    int        port;

    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    c = cf_builder_create(b, err);
    if (c == NULL) {
        return -1;
    }
    port = (int)cf_get_int(c, "server.port", 0);
    cf_config_free(c);
    return port;
}

static int file_port(const char* path, cf_error* err) {
    cf_builder* b = cf_build(NULL);

    cf_builder_file(b, "file", CF_RANK_USER_FILE, path, NULL, 0);
    return port_of(b, err);
}

/* What cf_codec_detect says for these bytes, by the codec's name. */
static const char* detected(const char* text, const char* name,
                            const cf_codec* const* own, size_t nown, cf_detect* d) {
    cf_arena*       ar = cf_arena_new(NULL, 0);
    cf_blob         blob;
    const cf_codec* c;
    static char     out[32];

    cf_blob_mem(&blob, text, strlen(text), "bytes");
    c      = cf_codec_detect(own, nown, &blob, name, ar, d);
    out[0] = '\0';
    if (c != NULL) {
        strncpy(out, c->name, sizeof out - 1);
        out[sizeof out - 1] = '\0';
    }
    cf_arena_free(ar);
    return (c != NULL) ? out : NULL;
}

/* ------------------------------------------------------------------ */

/* One file with no extension for each built-in format: the content
   decides, and the value is the one that was written. */
static void one(const char* what, const char* text, const char* format) {
    cf_error    err = CF_ERROR_INIT;
    cf_detect   d;
    const char* got;
    char        msg[96];

    check(spill("cf_detect_noext", text), "wrote a file with no extension");
    sprintf(msg, "%s: read from its content", what);
    check(file_port("cf_detect_noext", &err) == 9443, msg);
    if (err.message[0] != '\0') {
        printf("  %s\n", err.message);
    }
    got = detected(text, NULL, NULL, 0, &d);
    sprintf(msg, "%s: detected as %s", what, format);
    check_text(got, format, msg);
    check(d.how == CF_DETECT_CONTENT, "and said that the content decided");
}

static void test_each_format(void) {
#if CF_WITH_TOML
    cf_detect arrays;
#endif
#if CF_WITH_JSON
    one("json", "{\"server\": {\"port\": 9443}}\n", "json");
    one("json5", "{server: {port: 9443,},}\n", "json5");
#endif
#if CF_WITH_TOML
    one("toml", "[server]\nport = 9443\n", "toml");
    /* A document of arrays only: INI reads `ports = [80, 443]` as text,
       and only TOML says it is a list. */
    check_text(detected("ports = [80, 443]\n", NULL, NULL, 0, &arrays), "toml",
               "a document of one array is TOML's");
#endif
#if CF_WITH_YAML
    one("yaml", "server:\n  port: 9443\n", "yaml");
#endif
#if CF_WITH_INI
    /* TOML would refuse the bare words, so only INI reads it. */
    one("ini", "[server]\nname = hello world\nport = 9443\n", "ini");
#endif
#if CF_WITH_XML
    one("xml", "<config><server><port>9443</port></server></config>\n", "xml");
#endif
    remove("cf_detect_noext");
}

/* A format that was not named and not in the name: said by the source. */
static void test_source_says(void) {
    cf_arena*     ar = cf_arena_new(NULL, 0);
    cf_keyspace   ks;
    cf_source     s;
    void*         st;
    cf_detect_how how = CF_DETECT_NONE;

    check(spill("cf_detect_noext", "[server]\nport = 9443\n"), "wrote a TOML file");
    st = cf_arena_calloc(ar, cf_source_file_state_size());
    check(cf_source_file(&s, "file", 0, "cf_detect_noext", NULL, 0, st,
                         cf_source_file_state_size()),
          "a file source with no format");
    check(cf_source_file_format(&s, &how) == NULL && how == CF_DETECT_NONE,
          "which has no format before it is loaded");
    check(cf_ks_init(&ks, ar, 16) && cf_source_load(&s, &ks), "is loaded");
#if CF_WITH_TOML
    check_text(cf_source_file_format(&s, &how), "toml", "and says the format it read");
    check(how == CF_DETECT_CONTENT, "from its content");
#endif
    cf_arena_free(ar);

#if CF_WITH_JSON
    ar = cf_arena_new(NULL, 0);
    check(spill("cf_detect.json", "{\"server\": {\"port\": 9443}}\n"), "wrote a .json");
    st = cf_arena_calloc(ar, cf_source_file_state_size());
    cf_source_file(&s, "file", 0, "cf_detect.json", NULL, 0, st,
                   cf_source_file_state_size());
    check(cf_ks_init(&ks, ar, 16) && cf_source_load(&s, &ks), "a .json is loaded");
    check_text(cf_source_file_format(&s, &how), "json", "as json");
    check(how == CF_DETECT_EXTENSION, "by its extension");
    cf_source_file(&s, "file", 0, "cf_detect.json", "json5", 0, st,
                   cf_source_file_state_size());
    check(cf_ks_init(&ks, ar, 16) && cf_source_load(&s, &ks), "and again, named");
    check_text(cf_source_file_format(&s, &how), "json5", "as the format named");
    check(how == CF_DETECT_NAMED, "which is said");
    cf_arena_free(ar);
    remove("cf_detect.json");
#endif
    remove("cf_detect_noext");
}

#if CF_WITH_JSON && CF_WITH_YAML
/* The name's format is tried first, and when it cannot read the file the
   error says what the content looks like. */
static void test_renamed(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_detect   d;
    const char* yaml = "server:\n  port: 9443\n";

    check(spill("cf_detect_renamed.json", yaml), "wrote YAML in a .json file");
    check(file_port("cf_detect_renamed.json", &err) == -1,
          "a .json holding YAML is refused, as before");
    has(err.message, "yaml", "and the error names the format the content looks like");
    has(err.message, "looks like", "as a hint");
    check_text(detected(yaml, "cf_detect_renamed.json", NULL, 0, &d), "json",
               "detect takes the name's format as it stands");
    check(d.how == CF_DETECT_EXTENSION, "and says it came from the extension");
    remove("cf_detect_renamed.json");

    /* No hint when the content looks like nothing. */
    check(spill("cf_detect_renamed.json", "{ broken"), "wrote broken JSON");
    check(file_port("cf_detect_renamed.json", &err) == -1, "broken JSON is refused");
    lacks(err.message, "looks like", "with no hint when nothing else reads it");
    remove("cf_detect_renamed.json");
}
#endif

static void test_empty(void) {
    cf_error  err = CF_ERROR_INIT;
    cf_detect d;

    check(spill("cf_detect_empty", ""), "wrote an empty file");
    check(file_port("cf_detect_empty", &err) == -1, "an empty file is refused");
    has(err.message, "cannot tell the format", "saying so");
    has(err.message, "empty", "and why");
    check(detected("", NULL, NULL, 0, &d) == NULL && d.how == CF_DETECT_NONE,
          "and detects nothing");
    check(spill("cf_detect_empty", "just some words\n"),
          "wrote text that is no format");
    check(file_port("cf_detect_empty", &err) == -1, "text no format reads is refused");
    has(err.message, "cannot tell the format", "the same way");
    remove("cf_detect_empty");
}

#if CF_WITH_YAML && CF_WITH_INI
/* Two loose formats that both read it, into different events. */
static void test_ambiguous(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_detect   d;
    const char* text = "a = b: c\n";

    check(detected(text, NULL, NULL, 0, &d) == NULL,
          "a document two loose formats read");
    check(d.first != NULL && d.second != NULL && d.first != d.second,
          "differently is refused, with both candidates");
    check(d.first != NULL && d.second != NULL &&
              ((strcmp(d.first->name, "yaml") == 0 &&
                strcmp(d.second->name, "ini") == 0) ||
               (strcmp(d.first->name, "ini") == 0 &&
                strcmp(d.second->name, "yaml") == 0)),
          "which are yaml and ini, in the order the first bytes put them");
    check(spill("cf_detect_ambig", text), "wrote it to a file");
    check(file_port("cf_detect_ambig", &err) == -1, "and the file is refused");
    has(err.message, "yaml", "naming the first");
    has(err.message, "ini", "and the second");
    has(err.message, "differently", "and why");
    remove("cf_detect_ambig");

    /* The same events from both are no ambiguity: YAML is taken. */
    check_text(detected("a: b\n", NULL, NULL, 0, &d), "yaml",
               "two loose formats that agree are not ambiguous");
}
#endif

/* A document in memory: the format named, the name's, or the content's. */
static void test_document(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b;
    cf_config*  c;
    char        buf[64];
    char        ex[512];

    /* No format: the content decides, and the bytes are copied. */
    strcpy(buf, "[server]\nport = 9443\n");
    b = cf_build(NULL);
    check(
        cf_builder_document(b, "remote", CF_RANK_USER_FILE, buf, strlen(buf), NULL, 0),
        "a document is added with no format");
    memset(buf, 'x', sizeof buf - 1);
    buf[sizeof buf - 1] = '\0';
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    c = cf_builder_create(b, &err);
    check(c != NULL, "and is read from its content");
    if (c == NULL) {
        printf("  %s\n", err.message);
    } else {
        check(cf_get_int(c, "server.port", 0) == 9443,
              "from the copy: the buffer changed after the call");
        cf_explain(c, "server.port", ex, sizeof ex);
        has(ex, "remote:2", "and explain() names the document and the line");
        cf_config_free(c);
    }

#if CF_WITH_JSON
    /* The name's extension. */
    strcpy(buf, "{\"server\": {\"port\": 9443}}");
    b = cf_build(NULL);
    cf_builder_document(b, "remote.json", CF_RANK_USER_FILE, buf, strlen(buf), NULL, 0);
    check(port_of(b, &err) == 9443, "a document named .json is read as JSON");
#endif

#if CF_WITH_YAML
    /* The format named. */
    strcpy(buf, "server:\n  port: 9443\n");
    b = cf_build(NULL);
    cf_builder_document(b, "remote", CF_RANK_USER_FILE, buf, strlen(buf), "yaml", 0);
    check(port_of(b, &err) == 9443, "a document with its format named");
    b = cf_build(NULL);
    cf_builder_document(b, "remote", CF_RANK_USER_FILE, buf, strlen(buf), "nonsense",
                        0);
    check(port_of(b, &err) == -1, "and one naming a format there is not is refused");
    has(err.message, "not compiled", "with the usual error");
#endif

    /* Nothing to read. */
    b = cf_build(NULL);
    check(cf_builder_document(b, "remote", CF_RANK_USER_FILE, "", 0, NULL, 0),
          "an empty document is accepted by the builder");
    check(port_of(b, &err) == -1, "and refused when read");
    has(err.message, "empty", "as an empty one");
    b = cf_build(NULL);
    check(!cf_builder_document(b, NULL, CF_RANK_USER_FILE, "x", 1, NULL, 0),
          "a document without a name is refused");
    cf_builder_abort(b);
}

/* The builder's own codec is a candidate ahead of the built-in ones. */
static void test_own_codec(void) {
    cf_error              err = CF_ERROR_INIT;
    cf_builder*           b   = cf_build(NULL);
    cf_detect             d;
    const cf_codec* const own[1] = {&kv_codec};

    check(spill("cf_detect_kv", "server.port: 9443\n"),
          "wrote a file for a codec of ours");
    check_text(detected("server.port: 9443\n", NULL, own, 1, &d), "kv",
               "the builder's own codec is chosen from content, first");
    check(d.how == CF_DETECT_CONTENT, "as content");
    check_text(detected("server.port: 9443\n", "x.kv", own, 1, &d), "kv",
               "and by extension");
    check(d.how == CF_DETECT_EXTENSION, "as that");

    check(cf_builder_codec(b, &kv_codec), "a codec is registered");
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_detect_kv", NULL, 0);
    check(port_of(b, &err) == 9443, "and a file with no extension is read with it");

    b = cf_build(NULL);
    cf_builder_codec(b, &kv_codec);
    cf_builder_document(b, "held", CF_RANK_USER_FILE, "server.port: 9443\n", 18, NULL,
                        0);
    check(port_of(b, &err) == 9443, "and so is a document");
    remove("cf_detect_kv");
}

#if CF_WITH_DIR && CF_WITH_JSON && CF_WITH_TOML
/* A drop-in directory reads a file whose name gives no format by its
   content, as a file is, and refuses one no format reads. */
static void test_dir(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;
    char        ex[512];

    check(spill("cf_detect.d/10-base", "[server]\nport = 1000\n") &&
              spill("cf_detect.d/50-site", "{\"server\": {\"port\": 9443}}\n"),
          "wrote two files with no extension");
    cf_builder_dir(b, "conf.d", CF_RANK_USER_FILE, "cf_detect.d", NULL, 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    c = cf_builder_create(b, &err);
    check(c != NULL, "a directory of files with no extension is read");
    if (c == NULL) {
        printf("  %s\n", err.message);
    } else {
        check(cf_get_int(c, "server.port", 0) == 9443,
              "each by its content, the later one winning");
        cf_explain(c, "server.port", ex, sizeof ex);
        has(ex, "cf_detect.d/10-base", "and the file it beat is named");
        cf_config_free(c);
    }

    /* A file no format reads is still named, not skipped. */
    check(spill("cf_detect.d/60-junk", "no format reads this\n"), "wrote a junk file");
    b = cf_build(NULL);
    cf_builder_dir(b, "conf.d", CF_RANK_USER_FILE, "cf_detect.d", NULL, 0);
    check(port_of(b, &err) == -1, "a file no format reads refuses the directory");
    has(err.message, "cf_detect.d/60-junk", "naming the file");
    has(err.message, "cannot tell the format", "and why");

    check(spill("cf_detect.d/60-junk", ""), "emptied it");
    b = cf_build(NULL);
    cf_builder_dir(b, "conf.d", CF_RANK_USER_FILE, "cf_detect.d", NULL, 0);
    check(port_of(b, &err) == -1, "and an empty one does too");
    has(err.message, "empty", "as empty");

    /* A format named for the directory is not a guess, even for a file
       that reads as something else. */
    b = cf_build(NULL);
    cf_builder_dir(b, "conf.d", CF_RANK_USER_FILE, "cf_detect.d", "nonsense", 0);
    check(port_of(b, &err) == -1, "a format the build does not have is refused");
    has(err.message, "not compiled", "saying so");

    /* A backup's copy is not read by its content: an extension no format
       claims still refuses the directory, as it did before D-73. */
    remove("cf_detect.d/60-junk");
    check(spill("cf_detect.d/50-site.toml~", "[server]\nport = 1\n"), "wrote a backup");
    b = cf_build(NULL);
    cf_builder_dir(b, "conf.d", CF_RANK_USER_FILE, "cf_detect.d", NULL, 0);
    check(port_of(b, &err) == -1, "a backup's copy refuses the directory");
    has(err.message, "50-site.toml~", "naming it");

    remove("cf_detect.d/10-base");
    remove("cf_detect.d/50-site");
    remove("cf_detect.d/50-site.toml~");
}
#endif

/* A variable's value, by the environment's own lookup. */
static void set_var(const char* name, const char* value) {
#if defined(_WIN32)
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

/* A whole document in one environment variable. */
static void test_env_document(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b;
    cf_config*  c;
    char        ex[512];

    set_var("CF_DETECT_DOC", "[server]\nport = 9443\n");
    b = cf_build(NULL);
    check(cf_builder_env_document(b, "app-doc", CF_RANK_ENV, "CF_DETECT_DOC", NULL, 0),
          "a variable is added as a document");
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    c = cf_builder_create(b, &err);
    check(c != NULL, "and read from its content");
    if (c == NULL) {
        printf("  %s\n", err.message);
    } else {
        check(cf_get_int(c, "server.port", 0) == 9443, "the value is the document's");
        cf_explain(c, "server.port", ex, sizeof ex);
        has(ex, "CF_DETECT_DOC:2", "and explain() names the variable and the line");
        has(ex, "app-doc", "under the source's name");
        cf_config_free(c);
    }

#if CF_WITH_YAML
    set_var("CF_DETECT_DOC", "server:\n  port: 9443\n");
    b = cf_build(NULL);
    cf_builder_env_document(b, NULL, CF_RANK_ENV, "CF_DETECT_DOC", "yaml", 0);
    check(port_of(b, &err) == 9443, "with its format named");
#endif

    /* Unset: an error, unless it may be. */
    b = cf_build(NULL);
    cf_builder_env_document(b, "app-doc", CF_RANK_ENV, "CF_DETECT_UNSET_VAR", NULL, 0);
    check(port_of(b, &err) == -1, "a variable that is not set is an error");
    has(err.message, "CF_DETECT_UNSET_VAR", "naming the variable");
    b = cf_build(NULL);
    cf_builder_env_document(b, "app-doc", CF_RANK_ENV, "CF_DETECT_UNSET_VAR", NULL,
                            CF_OPTIONAL);
    check(port_of(b, &err) == 8080, "and none with CF_OPTIONAL");

    /* Set and empty: nothing to read. */
    set_var("CF_DETECT_DOC", "");
    b = cf_build(NULL);
    cf_builder_env_document(b, "app-doc", CF_RANK_ENV, "CF_DETECT_DOC", NULL, 0);
#if !defined(_WIN32) /* _putenv_s with "" removes the variable */
    check(port_of(b, &err) == -1, "an empty value is refused as an empty document");
    has(err.message, "empty", "as that");
#endif

    b = cf_build(NULL);
    check(!cf_builder_env_document(b, "x", CF_RANK_ENV, "A=B", NULL, 0),
          "a name with = is not a variable");
    cf_builder_abort(b);
    b = cf_build(NULL);
    check(!cf_builder_env_document(b, "x", CF_RANK_ENV, NULL, NULL, 0),
          "and neither is none");
    cf_builder_abort(b);
}

#if CF_WITH_REPORT && CF_WITH_TOML
/* The report says what format a document was read as, and how. */
static void test_report(void) {
    cf_error       err = CF_ERROR_INIT;
    cf_builder*    b   = cf_build(NULL);
    cf_config*     c;
    cf_report_info info = CF_REPORT_INFO_INIT;
    static char    buf[8192];
    const char*    doc = "[server]\nport = 9443\n";

    cf_builder_document(b, "by-content", CF_RANK_USER_FILE, doc, strlen(doc), NULL, 0);
    cf_builder_document(b, "by-name", CF_RANK_SYS_FILE, doc, strlen(doc), "toml", 0);
    cf_builder_document(b, "by-extension.toml", CF_RANK_CLI, doc, strlen(doc), NULL, 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    c = cf_builder_create(b, &err);
    check(c != NULL, "a configuration of three documents");
    if (c == NULL) {
        return;
    }
    info.generated = "2026-11-07T00:00:00Z";
    check(cf_report(c, &info, buf, sizeof buf) > 0, "its report");
    has(buf,
        "\"name\": \"by-content\", \"kind\": \"document\", \"rank\": 20, \"format\": "
        "\"toml\", \"format_how\": \"content\"",
        "says content for a document read by it");
    has(buf, "\"format\": \"toml\", \"format_how\": \"named\"", "named for one named");
    has(buf, "\"format\": \"toml\", \"format_how\": \"extension\"",
        "extension for one by its name");
    cf_config_free(c);

    /* A source of pairs reads no document: no format field. */
    b = cf_build(NULL);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    c = cf_builder_create(b, &err);
    if (c != NULL) {
        cf_report(c, &info, buf, sizeof buf);
        lacks(buf, "format_how", "a configuration with no document has none");
        cf_config_free(c);
    }
}
#endif

int main(void) {
#if CF_WITH_DIR && CF_WITH_JSON && CF_WITH_TOML
    test_dir();
#endif
    test_env_document();
#if CF_WITH_REPORT && CF_WITH_TOML
    test_report();
#endif
    test_each_format();
    test_source_says();
#if CF_WITH_JSON && CF_WITH_YAML
    test_renamed();
#endif
    test_empty();
#if CF_WITH_YAML && CF_WITH_INI
    test_ambiguous();
#endif
    test_document();
    test_own_codec();
    return cf_test_done();
}
