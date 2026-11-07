/*
 * test_bom.c - a document that starts with a UTF-8 byte order mark is read
 * as the document without it, by every format whose reader skips one: from
 * a file by its extension, and from memory with the format chosen by the
 * content. The mark is not counted in a column, and a mark with nothing
 * after it is still no document.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#include "cf_test.h"

#define BOM "\xEF\xBB\xBF"

/* server.port, and where explain() says it is, read from `text` as a file
   called `path` (the format from its extension); -1 when refused. */
static int from_file(const char* path, const char* text, char* ex, size_t cap) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;
    int         port;

    ex[0] = '\0';
    check(spill(path, text), "wrote the file");
    cf_builder_file(b, "file", CF_RANK_USER_FILE, path, NULL, 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    c = cf_builder_create(b, &err);
    remove(path);
    if (c == NULL) {
        return -1;
    }
    port = (int)cf_get_int(c, "server.port", 0);
    cf_explain(c, "server.port", ex, cap);
    cf_config_free(c);
    return port;
}

/* The same from memory, with no format named. */
static int from_memory(const char* text, size_t n, char* ex, size_t cap) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;
    int         port;

    ex[0] = '\0';
    cf_builder_document(b, "bomdoc", CF_RANK_USER_FILE, text, n, NULL, 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        return -1;
    }
    port = (int)cf_get_int(c, "server.port", 0);
    cf_explain(c, "server.port", ex, cap);
    cf_config_free(c);
    return port;
}

/* `body` is a document with server.port on its second line. */
static void one(const char* what, const char* path, const char* body) {
    char msg[96];
    char ex[512];
    char with[256];

    sprintf(with, "%s%s", BOM, body);

    sprintf(msg, "%s: a file with a mark is read", what);
    check(from_file(path, with, ex, sizeof ex) == 9443, msg);
    sprintf(msg, "%s: and its key is on line 2", what);
    has(ex, ":2:", msg);

    sprintf(msg, "%s: a document with a mark is read, its format from the content",
            what);
    check(from_memory(with, strlen(with), ex, sizeof ex) == 9443, msg);
    sprintf(msg, "%s: and its key is on line 2", what);
    has(ex, "bomdoc:2:", msg);

    sprintf(msg, "%s: without a mark it is as before", what);
    check(from_memory(body, strlen(body), ex, sizeof ex) == 9443, msg);
    has(ex, "bomdoc:2:", msg);
}

static void test_each_format(void) {
#if CF_WITH_TOML
    one("toml", "cf_bom.toml", "[server]\nport = 9443\n");
#endif
#if CF_WITH_INI
    one("ini", "cf_bom.ini", "[server]\nport = 9443\nname = hello world\n");
#endif
#if CF_WITH_JSON
    one("json", "cf_bom.json", "{\n\"server\": {\"port\": 9443}}\n");
    one("json5", "cf_bom.json5", "{\nserver: {port: 9443,},}\n");
#endif
}

/* A mark and nothing after it holds no document, and a mark that is not
   the first bytes is not skipped. */
static void test_not_a_document(void) {
    char ex[64];

    check(from_memory(BOM, 3, ex, sizeof ex) == -1,
          "a mark alone is refused, as an empty document is");
#if CF_WITH_JSON
    check(from_file("cf_bom_empty.json", BOM, ex, sizeof ex) == -1,
          "a .json holding only a mark is refused");
    check(from_file("cf_bom_twice.json", BOM BOM "{\"server\": {\"port\": 9443}}", ex,
                    sizeof ex) == -1,
          "a second mark is not skipped");
#endif
}

int main(void) {
    test_each_format();
    test_not_a_document();
    return cf_test_done();
}
