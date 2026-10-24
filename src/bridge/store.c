/*
 * store.c - a SQLite store as a source, and the one place a running
 * program writes configuration. C89.
 *
 * A store is not a document. It has no comments to keep and no layout
 * to lose, and it has transactions, which is why Design.md (section 39)
 * makes it the answer to the write problem: hand-maintained files stay
 * read-only, and what a program changes at run time goes to a store,
 * atomically. One table, the flat key space as it is:
 *
 *     CREATE TABLE configforge (
 *         key   TEXT PRIMARY KEY NOT NULL,
 *         type  TEXT NOT NULL,   -- null | bool | int | real | text
 *         value TEXT,
 *         meta  TEXT             -- who set it, when, why
 *     );
 *
 * The type travels with the value, so a store is typed as JSON is; and
 * `meta` is the value's own provenance, shown by explain() as where it
 * was set, which makes a store the one source that can say why a value
 * is what it is across a restart.
 *
 * Through sqlite3 (D-26): the dependency is the feature. Its memory is
 * its own, as expat's and libyaml's are.
 */

#include "configforge/cf_keyspace.h"
#include "configforge/cf_numeric.h"
#include "configforge/cf_source.h"

#include <string.h>

typedef struct store_state {
    const char* path;
    unsigned    flags;
    cf_error    err;
    cf_bool     failed;
} store_state;

#if CF_WITH_SQLITE

#include <sqlite3.h>

static const char store_schema[] = "CREATE TABLE IF NOT EXISTS configforge ("
                                   "key TEXT PRIMARY KEY NOT NULL, "
                                   "type TEXT NOT NULL, "
                                   "value TEXT, "
                                   "meta TEXT)";

/* One stored row as a value, by its type column. */
static cf_bool store_value(const char* type, const char* text, size_t n, cf_value* v) {
    if (strcmp(type, "null") == 0) {
        cf_value_null(v);
        return CF_TRUE;
    }
    if (strcmp(type, "text") == 0) {
        cf_value_text(v, text, n);
        return CF_TRUE;
    }
    if (strcmp(type, "bool") == 0) {
        if (n == 4 && memcmp(text, "true", 4) == 0) {
            cf_value_bool(v, CF_TRUE);
            return CF_TRUE;
        }
        if (n == 5 && memcmp(text, "false", 5) == 0) {
            cf_value_bool(v, CF_FALSE);
            return CF_TRUE;
        }
        return CF_FALSE;
    }
    if (strcmp(type, "int") == 0) {
        cf_i64 i;
        size_t end = 0;
        if (!cf_text_to_i64(text, n, &i, &end) || end != n) {
            return CF_FALSE;
        }
        cf_value_int(v, i);
        return CF_TRUE;
    }
#if CF_WITH_REAL
    if (strcmp(type, "real") == 0) {
        double r;
        size_t end = 0;
        if (!cf_text_to_f64(text, n, &r, &end) || end != n) {
            return CF_FALSE;
        }
        cf_value_real(v, r);
        return CF_TRUE;
    }
#endif
    return CF_FALSE;
}

static cf_bool store_load(const cf_source* s, cf_keyspace* ks) {
    store_state*  st = (store_state*)s->ext;
    sqlite3*      db = NULL;
    sqlite3_stmt* q  = NULL;
    int           rc;
    cf_bool       ok = CF_TRUE;

    st->failed = CF_FALSE;
    cf_error_clear(&st->err);

    /* Read only, and never created: a store that is not there is a
       missing file, optional or not, exactly as a document is. */
    rc = sqlite3_open_v2(st->path, &db, SQLITE_OPEN_READONLY, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        if ((st->flags & CF_FILE_OPTIONAL) != 0) {
            return CF_TRUE;
        }
        st->failed = CF_TRUE;
        return cf_error_set(&st->err, CF_E_SOURCE, NULL, st->path,
                            "cannot open the store '%s'", st->path);
    }
    rc = sqlite3_prepare_v2(
        db, "SELECT key, type, value, meta FROM configforge ORDER BY key", -1, &q,
        NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        st->failed = CF_TRUE;
        return cf_error_set(
            &st->err, CF_E_SOURCE, NULL, st->path,
            "'%s' is not a ConfigForge store: it has no configforge table "
            "(key, type, value, meta)",
            st->path);
    }
    while (ok && (rc = sqlite3_step(q)) == SQLITE_ROW) {
        const char* key  = (const char*)sqlite3_column_text(q, 0);
        const char* type = (const char*)sqlite3_column_text(q, 1);
        const char* text = (const char*)sqlite3_column_text(q, 2);
        size_t      n    = (size_t)sqlite3_column_bytes(q, 2);
        const char* meta = (const char*)sqlite3_column_text(q, 3);
        char        at[CF_PATH_MAX + 160];
        size_t      klen = (key != NULL) ? strlen(key) : 0;
        cf_value    v;

        if (key == NULL || !cf_key_valid(key, klen)) {
            ok = cf_error_set(&st->err, CF_E_KEY, key, st->path,
                              "'%s' holds a key that is not valid: '%s'", st->path,
                              (key != NULL) ? key : "");
            break;
        }
        if (type == NULL || !store_value(type, (text != NULL) ? text : "", n, &v)) {
            ok = cf_error_set(&st->err, CF_E_CONVERT, key, st->path,
                              "%s: '%s' is stored as %s, and its value is not one",
                              st->path, key, (type != NULL) ? type : "no type");
            break;
        }
        /* Where it was set: the store, and what the writer said. */
        if (meta != NULL && meta[0] != '\0') {
            cf_fmt(at, sizeof at, "%s: %s", st->path, meta);
        } else {
            cf_fmt(at, sizeof at, "%s", st->path);
        }
        if (!cf_ks_add(ks, key, klen, s->rank, &v, at)) {
            ok = cf_error_set(&st->err, CF_E_MEMORY, key, st->path,
                              "no room to read '%s'", st->path);
        }
    }
    if (ok && rc != SQLITE_DONE) {
        ok = cf_error_set(&st->err, CF_E_SOURCE, NULL, st->path, "reading '%s': %s",
                          st->path, sqlite3_errmsg(db));
    }
    sqlite3_finalize(q);
    sqlite3_close(db);
    st->failed = ok ? CF_FALSE : CF_TRUE;
    return ok;
}

/* ------------------------------------------------------------------ */

static const char* store_type(cf_type t) {
    return (t == CF_TYPE_NULL)   ? "null"
           : (t == CF_TYPE_BOOL) ? "bool"
           : (t == CF_TYPE_INT)  ? "int"
           : (t == CF_TYPE_TEXT) ? "text"
                                 : "real";
}

static cf_bool store_failed(sqlite3* db, const char* path, cf_error* err) {
    cf_bool r =
        cf_error_set(err, CF_E_SOURCE, NULL, path, "writing the store '%s': %s", path,
                     (db != NULL) ? sqlite3_errmsg(db) : "cannot open it");
    if (db != NULL) {
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        sqlite3_close(db);
    }
    return r;
}

cf_bool cf_store_set(const char* path, const cf_store_item* items, size_t count,
                     const char* meta, cf_error* err) {
    sqlite3*      db = NULL;
    sqlite3_stmt* q  = NULL;
    size_t        i;

    if (path == NULL || (items == NULL && count > 0)) {
        return cf_error_set(err, CF_E_SOURCE, NULL, NULL, "no store to write");
    }
    /* Every key and value checked before anything is written: one bad
       item refuses the whole change rather than half of it. */
    for (i = 0; i < count; ++i) {
        if (items[i].key == NULL || !cf_key_valid(items[i].key, strlen(items[i].key))) {
            return cf_error_set(err, CF_E_KEY, items[i].key, path,
                                "'%s' is not a valid key",
                                (items[i].key != NULL) ? items[i].key : "");
        }
    }
    if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) !=
            SQLITE_OK ||
        sqlite3_exec(db, store_schema, NULL, NULL, NULL) != SQLITE_OK ||
        sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK ||
        sqlite3_prepare_v2(
            db,
            "INSERT OR REPLACE INTO configforge (key, type, value, meta) "
            "VALUES (?1, ?2, ?3, ?4)",
            -1, &q, NULL) != SQLITE_OK) {
        return store_failed(db, path, err);
    }
    for (i = 0; i < count; ++i) {
        const cf_value* v = &items[i].value;
        char            tmp[CF_VALUE_TEXT_MAX];
        const char*     text;
        size_t          n;

        if (v->type == CF_TYPE_TEXT) {
            text = v->u.t.p;
            n    = v->u.t.n;
        } else {
            n    = cf_value_to_text(v, tmp, sizeof tmp);
            text = tmp;
        }
        sqlite3_reset(q);
        if (sqlite3_bind_text(q, 1, items[i].key, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
            sqlite3_bind_text(q, 2, store_type(v->type), -1, SQLITE_STATIC) !=
                SQLITE_OK ||
            (v->type == CF_TYPE_NULL
                 ? sqlite3_bind_null(q, 3)
                 : sqlite3_bind_text(q, 3, text, (int)n, SQLITE_TRANSIENT)) !=
                SQLITE_OK ||
            (meta != NULL ? sqlite3_bind_text(q, 4, meta, -1, SQLITE_TRANSIENT)
                          : sqlite3_bind_null(q, 4)) != SQLITE_OK ||
            sqlite3_step(q) != SQLITE_DONE) {
            sqlite3_finalize(q);
            return store_failed(db, path, err);
        }
    }
    sqlite3_finalize(q);
    if (sqlite3_exec(db, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
        return store_failed(db, path, err);
    }
    sqlite3_close(db);
    return CF_TRUE;
}

#else /* !CF_WITH_SQLITE */

/* Present, and failing by name: a store a build cannot read is never
   read as nothing. */
static cf_bool store_load(const cf_source* s, cf_keyspace* ks) {
    store_state* st = (store_state*)s->ext;

    (void)ks;
    st->failed = CF_TRUE;
    return cf_error_set(&st->err, CF_E_CAPABILITY, NULL, st->path,
                        "'%s' is a SQLite store, and this build was compiled without "
                        "CF_WITH_SQLITE",
                        st->path);
}

cf_bool cf_store_set(const char* path, const cf_store_item* items, size_t count,
                     const char* meta, cf_error* err) {
    (void)items;
    (void)count;
    (void)meta;
    return cf_error_set(err, CF_E_CAPABILITY, NULL, path,
                        "writing a store needs a build with CF_WITH_SQLITE");
}

#endif /* CF_WITH_SQLITE */

static void store_destroy(cf_source* s) {
    (void)s;
}

static const char* store_path(const cf_source* s) {
    const store_state* st = (const store_state*)s->ext;
    return (st != NULL) ? st->path : NULL;
}

static const cf_source_vtable cf_store_vt = {"store", store_load, store_destroy,
                                             store_path};

size_t cf_source_store_state_size(void) {
    return sizeof(store_state);
}

cf_bool cf_source_store(cf_source* s, const char* name, int rank, const char* path,
                        unsigned flags, void* state, size_t state_size) {
    store_state* st = (store_state*)state;

    if (s == NULL || path == NULL || st == NULL || state_size < sizeof(store_state)) {
        return CF_FALSE;
    }
    memset(st, 0, sizeof *st);
    st->path     = path;
    st->flags    = flags;
    st->err.size = sizeof(cf_error);

    s->vt   = &cf_store_vt;
    s->name = (name != NULL) ? name : path;
    s->rank = rank;
    s->ext  = st;
    s->ref  = NULL;
    return CF_TRUE;
}

const cf_error* cf_source_store_error(const cf_source* s) {
    const store_state* st;

    if (s == NULL || s->vt != &cf_store_vt || s->ext == NULL) {
        return NULL;
    }
    st = (const store_state*)s->ext;
    return st->failed ? &st->err : NULL;
}
