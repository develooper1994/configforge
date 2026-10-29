/*
 * dir.c - a drop-in directory as a source. C89 where it can be.
 *
 * conf.d is how systemd, sudo, apt and a dozen others let a package add
 * configuration without editing a file it does not own. Each file is
 * read in lexical order, so 10-base.ini losing to 90-site.ini is
 * precedence rather than a merge rule, and explain() names the file
 * that won.
 *
 * The ordering falls out of a decision made elsewhere: within one
 * source the later candidate wins. Every file here shares a rank, so
 * reading them in order is all it takes.
 *
 * Directory listing is the one thing C89 has no answer for, so it comes
 * from the platform layer and this file never names a platform. A
 * target with no listing gets an error that says so rather than an
 * empty directory, which looks exactly like an empty configuration.
 */

#include "configforge/cf_flatten.h"
#include "configforge/cf_platform.h"
#include "configforge/cf_serialize.h"
#include "configforge/cf_source.h"

#include <string.h>

#define CF_DIR_MAX_FILES 256
#define CF_DIR_PATH_MAX CF_PLATFORM_NAME_MAX

typedef struct dir_state {
    const char* path;
    const char* format; /* explicit override, or NULL for by-extension */
    unsigned    flags;
    cf_error    err;
    cf_bool     failed;
} dir_state;

#if CF_WITH_DIR

/* Insertion sort over the names. A drop-in directory holds tens of
   files and the comparison is byte-wise, so this is both the fastest
   and the least code. Byte-wise and not strcoll: the order decides
   which file wins, and an order that depends on the reader's locale is
   a configuration that depends on it too. */
/* One name, as a type. A pointer to an array spelled out in place is
   the one declarator clang-format 18 and 22 space differently, and a
   format gate that depends on the formatter's version is a gate one of
   the two CI jobs fails. */
typedef char dir_name[CF_DIR_PATH_MAX];

static void dir_sort(dir_name* names, int n) {
    int i, j;

    for (i = 1; i < n; ++i) {
        char tmp[CF_DIR_PATH_MAX];
        memcpy(tmp, names[i], CF_DIR_PATH_MAX);
        j = i;
        while (j > 0 &&
               cf_cmp_bytes(names[j - 1], strlen(names[j - 1]), tmp, strlen(tmp)) > 0) {
            memcpy(names[j], names[j - 1], CF_DIR_PATH_MAX);
            --j;
        }
        memcpy(names[j], tmp, CF_DIR_PATH_MAX);
    }
}

/* ------------------------------------------------------------------ *
 * Loading
 * ------------------------------------------------------------------ */

static cf_bool dir_load(const cf_source* s, cf_keyspace* ks) {
    dir_state* st = (dir_state*)s->ext;
    cf_arena*  ar = cf_ks_arena(ks);
    dir_name*  names;
    char       full[CF_DIR_PATH_MAX];
    int        n, i;

    st->failed = CF_FALSE;
    cf_error_clear(&st->err);

    names = (dir_name*)cf_arena_alloc(ar, (size_t)CF_DIR_MAX_FILES * CF_DIR_PATH_MAX);
    if (names == NULL) {
        st->failed = CF_TRUE;
        return cf_error_set(&st->err, CF_E_MEMORY, NULL, st->path,
                            "no room to list '%s'", st->path);
    }

    n = cf_platform_dir_list(st->path, names, CF_DIR_MAX_FILES, &st->err);
    if (n < 0) {
        if ((st->flags & CF_OPTIONAL) != 0) {
            cf_error_clear(&st->err);
            return CF_TRUE;
        }
        st->failed = CF_TRUE;
        return CF_FALSE;
    }
    dir_sort(names, n);

    for (i = 0; i < n; ++i) {
        const cf_codec* codec;
        cf_blob         blob;
        cf_flatten      fl;

        if (cf_fmt(full, sizeof full, "%s/%s", st->path, names[i]) >= sizeof full) {
            continue;
        }
        codec = (st->format != NULL) ? cf_codec_by_name(st->format)
                                     : cf_codec_by_path(full);
        if (codec == NULL) {
            /* A file the build cannot read is named, not skipped: a
               directory that silently ignores half its contents is the
               worst possible place for a build option to hide. */
            const char* fmt = cf_format_of_path(full);
            st->failed      = CF_TRUE;
            if (fmt != NULL) {
                return cf_error_set(&st->err, CF_E_CAPABILITY, NULL, full,
                                    "'%s' requires %s support; this build "
                                    "was compiled without %s",
                                    full, fmt, cf_format_option(fmt));
            }
            return cf_error_set(&st->err, CF_E_CAPABILITY, NULL, full,
                                "cannot tell the format of '%s' from its "
                                "name; set one for the whole directory",
                                full);
        }
        if (!cf_blob_file(&blob, ar, full, &st->err)) {
            st->failed = CF_TRUE;
            return CF_FALSE;
        }
        /* Every file shares the source's rank, so the later one wins by
           the same rule that makes --port=2 beat --port=1. */
        if (!cf_flatten_init(&fl, ks, s->rank)) {
            st->failed = CF_TRUE;
            return cf_error_set(&st->err, CF_E_MEMORY, NULL, full,
                                "no room to read '%s'", full);
        }
        if (!codec->decode(&blob, cf_flatten_sink(&fl), &st->err)) {
            st->failed = CF_TRUE;
            return CF_FALSE;
        }
        if (fl.failed || fl.overflow) {
            st->failed = CF_TRUE;
            return cf_error_set(&st->err, CF_E_KEY, NULL, full,
                                "'%s' contains a key this build cannot hold", full);
        }
    }
    return CF_TRUE;
}

#else /* !CF_WITH_DIR */

/* Compiled without the drop-in directory source. The source still
   exists, so a program that adds one fails when its configuration is
   built and says why: not at link time, which would make the option a
   breaking change, and never by reading nothing, which looks exactly
   like an empty directory. */
static cf_bool dir_load(const cf_source* s, cf_keyspace* ks) {
    dir_state* st = (dir_state*)s->ext;

    (void)ks;
    st->failed = CF_TRUE;
    return cf_error_set(&st->err, CF_E_CAPABILITY, NULL, st->path,
                        "'%s' is a drop-in directory, and this build was compiled "
                        "without CF_WITH_DIR",
                        st->path);
}

#endif /* CF_WITH_DIR */

static void dir_destroy(cf_source* s) {
    (void)s;
}

static const char* dir_path(const cf_source* s) {
    const dir_state* st = (const dir_state*)s->ext;
    return (st != NULL) ? st->path : NULL;
}

static const cf_source_vtable cf_dir_vt = {"dir", dir_load, dir_destroy, dir_path,
                                           NULL};

size_t cf_source_dir_state_size(void) {
    return sizeof(dir_state);
}

cf_bool cf_source_dir(cf_source* s, const char* name, int rank, const char* path,
                      const char* format, unsigned flags, void* state,
                      size_t state_size) {
    dir_state* st = (dir_state*)state;

    if (s == NULL || path == NULL || st == NULL || state_size < sizeof(dir_state)) {
        return CF_FALSE;
    }
    memset(st, 0, sizeof *st);
    st->path     = path;
    st->format   = format;
    st->flags    = flags;
    st->err.size = sizeof(cf_error);

    s->vt   = &cf_dir_vt;
    s->name = (name != NULL) ? name : path;
    s->rank = rank;
    s->ext  = st;
    s->ref  = NULL;
    return CF_TRUE;
}

const cf_error* cf_source_dir_error(const cf_source* s) {
    const dir_state* st;

    if (s == NULL || s->vt != &cf_dir_vt || s->ext == NULL) {
        return NULL;
    }
    st = (const dir_state*)s->ext;
    return st->failed ? &st->err : NULL;
}
