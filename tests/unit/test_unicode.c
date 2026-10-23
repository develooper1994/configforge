/*
 * test_unicode.c - paths that are not ASCII.
 *
 * Paths are UTF-8 at the interface on every system (D-15). On Windows
 * the library once called the code-page ("A") functions, so a file
 * under a name the code page could not spell - a Turkish user's home,
 * say - could not be opened, stamped or listed. This builds such a tree
 * and reads it through every path the library has: a file, a drop-in
 * directory, and a change check.
 *
 * The tree is made with the platform's own wide or byte calls, because
 * that is the part a test may not take from the library it is testing.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <wchar.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

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

/* "cf_" + c-cedilla and Greek alpha: a directory, and in it "ayar" with
   a dotless i, ".ini". The same names in UTF-8 and in UTF-16.

   The alpha is the point. The machine this was written on runs code
   page 1254, which spells the Turkish letters - so a test using only
   those passed against the old code-page functions and proved nothing.
   No Western or Turkish code page has an alpha. */
#define DIR_U8 "cf_\xc3\xa7\xce\xb1"
#define FILE_U8 DIR_U8 "/ayar\xc4\xb1.ini"
#ifdef _WIN32
#define DIR_W L"cf_\x00e7\x03b1"
#define FILE_W DIR_W L"\\ayar\x0131.ini"
#endif

static const char content[] = "[server]\nport = 4242\n";

static cf_bool make_tree(void) {
    FILE* f;
#ifdef _WIN32
    (void)_wmkdir(DIR_W);
    f = _wfopen(FILE_W, L"wb");
#else
    (void)mkdir(DIR_U8, 0755);
    f = fopen(FILE_U8, "wb");
#endif
    if (f == NULL) {
        return CF_FALSE;
    }
    fputs(content, f);
    fclose(f);
    return CF_TRUE;
}

static void drop_tree(void) {
#ifdef _WIN32
    (void)_wremove(FILE_W);
    (void)_wrmdir(DIR_W);
#else
    (void)remove(FILE_U8);
    (void)rmdir(DIR_U8);
#endif
}

static void test_file(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;
    static char ex[4096];

    cf_builder_file(b, "file", CF_RANK_USER_FILE, FILE_U8, NULL, 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    c = cf_builder_create(b, &err);
    check(c != NULL, "a file under a non-ASCII directory opens");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_get_int(c, "server.port", 0) == 4242, "and is read");
    cf_explain(c, "server.port", ex, sizeof ex);
    has(ex, FILE_U8, "explain() names it in UTF-8");

    /* A stamp that could not be taken would report "absent" forever,
       which cf_changed would call a change on every call. */
    check(!cf_changed(c), "its stamp is taken, so nothing has changed");
    cf_config_free(c);
}

#if CF_WITH_DIR
static void test_directory(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;
    static char ex[4096];

    cf_builder_dir(b, "conf.d", CF_RANK_SYS_FILE, DIR_U8, NULL, 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    c = cf_builder_create(b, &err);
    check(c != NULL, "a non-ASCII drop-in directory is listed");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_get_int(c, "server.port", 0) == 4242, "and the file in it is read");
    cf_explain(c, "server.port", ex, sizeof ex);
    has(ex, "ayar\xc4\xb1.ini", "and the listed name came back in UTF-8");
    cf_config_free(c);
}
#endif

int main(void) {
    if (!make_tree()) {
        /* Not the library's failure: say so and stop, rather than
           reporting the library broken on a filesystem that cannot hold
           the name. */
        printf("skipped: this filesystem would not create the test tree\n");
        return 0;
    }
    test_file();
#if CF_WITH_DIR
    test_directory();
#endif
    drop_tree();

    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
