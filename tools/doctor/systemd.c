/*
 * systemd.c - Doctor's adapter for systemd's configuration files. C89.
 *
 *     systemd-analyze cat-config systemd/journald.conf | doctor import-systemd
 *     systemctl cat nginx.service | doctor import-systemd
 *
 * Both print every file systemd reads for one configuration - the main
 * file, then each drop-in - in the order systemd applies them, each
 * under a header naming it:
 *
 *     # /etc/systemd/journald.conf
 *     [Journal]
 *     Storage=persistent
 *
 *     # /etc/systemd/journald.conf.d/50-forward.conf
 *     [Journal]
 *     ForwardToSyslog=yes
 *
 * That order is systemd's precedence - a later file overrides an
 * earlier one, and within a file the later line wins - so the adapter
 * writes it down and decides nothing (Doctor.md, section 12):
 *
 *   precedence   the order printed. Each file is a source, the later one
 *                ranked above the earlier; within one, the later line
 *                wins, which is ConfigForge's rule too.
 *   places       "path:line", the line counted from the file's header,
 *                because the files are printed whole. So CFD001 - a key
 *                set twice in one file - can fire, and does.
 *   lists        "ExecStart=" accumulating, and an empty assignment
 *                resetting the list, are systemd's semantics for some
 *                keys and not others. Recorded as printed: the last
 *                assignment is the winner, which is what a reader of
 *                these files most needs to see, and what systemd does for
 *                every key that is not a list.
 *   keys         "[Journal] Storage" is journal.storage, lowered: a
 *                report key is lowercase, and systemd's own spellings do
 *                not differ only by case. A key a report still cannot
 *                carry is left out and counted.
 *   headers      a "# /..." line is a file's header only at the start or
 *                after a blank line, which is how both commands print
 *                it; anywhere else it is a comment.
 *   values       blind unless --full: Environment= carries credentials.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#include "adapters.h"

#define SYSD_SOURCES CF_MAX_SOURCES

static const cf_source_vtable systemd_vt = {
    sizeof(cf_source_vtable), "systemd", NULL, NULL, NULL, NULL};

static int systemd_usage(void) {
    fputs("doctor import-systemd [--full] [--profile <name>]\n"
          "\n"
          "Reads `systemd-analyze cat-config <file>` or `systemctl cat\n"
          "<unit>` on stdin and writes a provenance report on stdout,\n"
          "value-blind unless --full. The files are ranked in the order\n"
          "printed, the later above the earlier, as systemd applies them.\n",
          stderr);
    return 2;
}

static int systemd_space(char c) {
    return c == ' ' || c == '\t';
}

/* "[Journal]" and "Storage" as journal.storage. */
static size_t systemd_key(const char* sect, size_t sn, const char* name, size_t nn,
                          char* out, size_t cap) {
    size_t i, k = 0;

    if (sn + nn + 2 > cap) {
        return 0;
    }
    for (i = 0; i < sn; ++i) {
        out[k++] = (char)cf_to_lower((unsigned char)sect[i]);
    }
    out[k++] = '.';
    for (i = 0; i < nn; ++i) {
        out[k++] = (char)cf_to_lower((unsigned char)name[i]);
    }
    out[k] = '\0';
    return k;
}

int doctor_import_systemd(int argc, char** argv) {
    static char      line[8192];
    static char      joined[16384];
    static char      section[CF_KEY_MAX];
    static char      key[CF_KEY_MAX];
    static char      at[CF_PATH_MAX + 32];
    static char      first_skipped[CF_KEY_MAX];
    static char*     names[SYSD_SOURCES];
    static cf_source src[SYSD_SOURCES];
    const cf_source* view[SYSD_SOURCES];
    cf_arena*        ar = cf_arena_new(NULL, 1 << 16);
    cf_keyspace      ks;
    cf_defset        ds;
    cf_source_view   sv;
    cf_report_info   info;
    cf_bool          full    = CF_FALSE;
    const char*      profile = NULL;
    int              lineno = 0, header = 0, k;
    int              file   = -1;
    cf_bool          blank  = CF_TRUE; /* the start counts as after one */
    size_t           seclen = 0, jlen = 0, i, n;
    unsigned long    skipped = 0;
    char*            out;

    for (k = 0; k < argc; ++k) {
        if (strcmp(argv[k], "--full") == 0) {
            full = CF_TRUE;
        } else if (strcmp(argv[k], "--profile") == 0 && k + 1 < argc) {
            profile = argv[++k];
        } else {
            cf_arena_free(ar);
            return systemd_usage();
        }
    }
    if (ar == NULL || !cf_ks_init(&ks, ar, 256) || !cf_defset_init(&ds, ar, 256)) {
        fprintf(stderr, "doctor: cannot start\n");
        cf_arena_free(ar);
        return 3;
    }
    first_skipped[0] = '\0';

    while (fgets(line, (int)sizeof line, stdin) != NULL) {
        size_t      len = strlen(line);
        const char* p;
        const char* eq;
        size_t      kn, vn, keylen;
        cf_value    v;

        ++lineno;
        if (len > 0 && line[len - 1] != '\n' && !feof(stdin)) {
            fprintf(stderr, "doctor: stdin:%d: longer than %u bytes\n", lineno,
                    (unsigned)sizeof line);
            cf_arena_free(ar);
            return 2;
        }
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        /* A file's header: "# /path", at the start or after a blank. */
        if (jlen == 0 && blank && len > 2 && line[0] == '#' && line[1] == ' ' &&
            line[2] == '/') {
            if (file + 1 >= SYSD_SOURCES) {
                fprintf(stderr, "doctor: stdin:%d: more than %d files\n", lineno,
                        SYSD_SOURCES);
                cf_arena_free(ar);
                return 2;
            }
            names[++file] = cf_arena_dup(ar, line + 2, len - 2);
            if (names[file] == NULL) {
                fprintf(stderr, "doctor: out of memory\n");
                cf_arena_free(ar);
                return 3;
            }
            header = lineno;
            seclen = 0;
            blank  = CF_FALSE;
            continue;
        }
        blank = (len == 0) ? CF_TRUE : CF_FALSE;

        /* A trailing backslash continues the line, as systemd reads it:
           the backslash and line break become a space. */
        if (jlen + len + 2 > sizeof joined) {
            fprintf(stderr, "doctor: stdin:%d: a continued line is too long\n", lineno);
            cf_arena_free(ar);
            return 2;
        }
        memcpy(joined + jlen, line, len);
        jlen += len;
        joined[jlen] = '\0';
        if (jlen > 0 && joined[jlen - 1] == '\\' && line[0] != '#' && line[0] != ';') {
            joined[jlen - 1] = ' ';
            continue;
        }
        p    = joined;
        len  = jlen;
        jlen = 0;

        while (len > 0 && systemd_space(*p)) {
            ++p;
            --len;
        }
        if (len == 0 || *p == '#' || *p == ';') {
            continue;
        }
        if (file < 0) {
            fprintf(stderr,
                    "doctor: stdin:%d: no '# /path' header before it; is it "
                    "systemd-analyze cat-config output?\n",
                    lineno);
            cf_arena_free(ar);
            return 2;
        }
        if (*p == '[') {
            const char* close = (const char*)memchr(p, ']', len);
            if (close == NULL || (size_t)(close - p - 1) >= sizeof section) {
                fprintf(stderr, "doctor: stdin:%d: a section header with no ']'\n",
                        lineno);
                cf_arena_free(ar);
                return 2;
            }
            seclen = (size_t)(close - p - 1);
            memcpy(section, p + 1, seclen);
            section[seclen] = '\0';
            continue;
        }
        eq = (const char*)memchr(p, '=', len);
        if (eq == NULL || seclen == 0) {
            fprintf(stderr, "doctor: stdin:%d: expected Key=Value inside a [Section]\n",
                    lineno);
            cf_arena_free(ar);
            return 2;
        }
        kn = (size_t)(eq - p);
        while (kn > 0 && systemd_space(p[kn - 1])) {
            --kn;
        }
        ++eq;
        vn = len - (size_t)(eq - p);
        while (vn > 0 && systemd_space(*eq)) {
            ++eq;
            --vn;
        }
        while (vn > 0 && systemd_space(eq[vn - 1])) {
            --vn;
        }
        keylen = systemd_key(section, seclen, p, kn, key, sizeof key);
        if (keylen == 0 || !cf_key_valid(key, keylen)) {
            if (skipped++ == 0) {
                cf_fmt(first_skipped, sizeof first_skipped, "[%S] %S", section, seclen,
                       p, kn);
            }
            continue;
        }
        cf_fmt(at, sizeof at, "%s:%d", names[file], lineno - header);
        cf_value_text(&v, eq, vn);
        if (!cf_ks_add(&ks, key, keylen, (SYSD_SOURCES - file) * 10, &v, at)) {
            fprintf(stderr, "doctor: stdin:%d: cannot record it\n", lineno);
            cf_arena_free(ar);
            return 3;
        }
    }
    if (file < 0) {
        fprintf(stderr,
                "doctor: stdin names no file; is it systemd-analyze cat-config?\n");
        cf_arena_free(ar);
        return 2;
    }
    if (!cf_ks_resolve(&ks)) {
        fprintf(stderr, "doctor: out of memory resolving\n");
        cf_arena_free(ar);
        return 3;
    }
    if (!full) {
        for (i = 0; i < cf_ks_count(&ks); ++i) {
            const cf_entry* e    = cf_ks_at(&ks, i);
            cf_error        err  = CF_ERROR_INIT;
            const char*     kept = cf_arena_dup(ar, e->key, e->keylen);
            if (kept == NULL || !cf_def_text(&ds, kept, NULL, CF_SENSITIVE, &err)) {
                fprintf(stderr, "doctor: %s\n", err.message);
                cf_arena_free(ar);
                return 3;
            }
        }
    }

    /* The files as printed, the later ranked above the earlier. */
    for (i = 0; i <= (size_t)file; ++i) {
        memset(&src[i], 0, sizeof src[i]);
        src[i].vt   = &systemd_vt;
        src[i].name = names[i];
        src[i].rank = (SYSD_SOURCES - (int)i) * 10;
        view[i]     = &src[i];
    }
    sv.items = view;
    sv.count = (size_t)file + 1;

    memset(&info, 0, sizeof info);
    info.mode        = full ? CF_REPORT_FULL : CF_REPORT_BLIND;
    info.profile     = profile;
    info.application = "systemd";

    n   = cf_report_write(&ks, full ? NULL : &ds, &sv, &info, NULL, 0);
    out = (char*)cf_arena_alloc(ar, n + 1);
    if (out == NULL) {
        fprintf(stderr, "doctor: out of memory writing\n");
        cf_arena_free(ar);
        return 3;
    }
    cf_report_write(&ks, full ? NULL : &ds, &sv, &info, out, n + 1);
    fwrite(out, 1, n, stdout);
    if (skipped > 0) {
        fprintf(stderr,
                "doctor: %lu key(s) left out because a report key cannot carry "
                "them, e.g. %s\n",
                skipped, first_skipped);
    }
    cf_arena_free(ar);
    return 0;
}
