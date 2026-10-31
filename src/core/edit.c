/*
 * edit.c - a change to a writable source, gathered and then written as
 * one (cf_edit.h). C89.
 *
 * An edit is a list of cf_write_items in an arena of its own, every
 * string copied into it, and a reference to the configuration it was
 * begun from; commit is cf_write over the list, so a change made here
 * is checked by exactly the rules a direct write is.
 */

#include "configforge/configforge.h"

#include <string.h>

struct cf_edit {
    cf_config*     base;
    cf_arena*      arena;
    const char*    target;
    cf_write_item* items;
    size_t         count;
    size_t         cap;
    cf_error       first; /* the first refused set, reported by commit */
    cf_bool        failed;
};

cf_edit* cf_edit_begin(cf_config* c, const char* target, cf_error* err) {
    cf_arena* ar;
    cf_edit*  e;

    if (c == NULL || target == NULL) {
        cf_error_set(err, CF_E_SOURCE, NULL, NULL,
                     "an edit needs a configuration and a target");
        return NULL;
    }
    ar = cf_arena_new(cf_default_allocator(), 0);
    e  = (ar != NULL) ? (cf_edit*)cf_arena_calloc(ar, sizeof *e) : NULL;
    if (e == NULL) {
        cf_arena_free(ar);
        cf_error_set(err, CF_E_MEMORY, NULL, target, "no room for an edit");
        return NULL;
    }
    e->arena  = ar;
    e->target = cf_arena_dup(ar, target, strlen(target));
    cf_error_clear(&e->first);
    e->first.size = sizeof e->first;
    if (e->target == NULL) {
        cf_arena_free(ar);
        cf_error_set(err, CF_E_MEMORY, NULL, target, "no room for an edit");
        return NULL;
    }
    /* The edit holds the snapshot it was begun from: the caller's
       reference may go first. */
    e->base = c;
    cf_config_ref(e->base);
    return e;
}

/* Note the first failure, for commit to report. */
static cf_bool edit_refuse(cf_edit* e, cf_code code, const char* key, const char* why) {
    if (!e->failed) {
        cf_error_set(&e->first, code, key, e->target, "%s", why);
        e->failed = CF_TRUE;
    }
    return CF_FALSE;
}

/* The item for `key`: the one already set, or a new one. */
static cf_write_item* edit_slot(cf_edit* e, const char* key) {
    size_t         i, n;
    cf_write_item* grown;

    if (e == NULL) {
        return NULL;
    }
    if (key == NULL || !cf_key_valid(key, strlen(key))) {
        (void)edit_refuse(e, CF_E_KEY, key, "not a valid key");
        return NULL;
    }
    for (i = 0; i < e->count; ++i) {
        if (strcmp(e->items[i].key, key) == 0) {
            return &e->items[i];
        }
    }
    if (e->count == e->cap) {
        n     = (e->cap == 0) ? 8 : e->cap * 2;
        grown = (cf_write_item*)cf_arena_calloc(e->arena, n * sizeof *grown);
        if (grown == NULL) {
            (void)edit_refuse(e, CF_E_MEMORY, key, "no room for the change");
            return NULL;
        }
        if (e->count > 0) {
            memcpy(grown, e->items, e->count * sizeof *grown);
        }
        e->items = grown;
        e->cap   = n;
    }
    e->items[e->count].key = cf_arena_dup(e->arena, key, strlen(key));
    if (e->items[e->count].key == NULL) {
        (void)edit_refuse(e, CF_E_MEMORY, key, "no room for the change");
        return NULL;
    }
    return &e->items[e->count++];
}

cf_bool cf_edit_set_text(cf_edit* e, const char* key, const char* value) {
    cf_write_item* it = edit_slot(e, key);
    char*          copy;

    if (it == NULL) {
        return CF_FALSE;
    }
    if (value == NULL) {
        return edit_refuse(e, CF_E_CONVERT, key,
                           "no text to set: unset the key instead");
    }
    copy = cf_arena_dup(e->arena, value, strlen(value));
    if (copy == NULL) {
        return edit_refuse(e, CF_E_MEMORY, key, "no room for the change");
    }
    cf_value_text(&it->value, copy, strlen(copy));
    it->op = CF_WRITE_SET;
    return CF_TRUE;
}

cf_bool cf_edit_set_int(cf_edit* e, const char* key, cf_i64 value) {
    cf_write_item* it = edit_slot(e, key);
    if (it == NULL) {
        return CF_FALSE;
    }
    cf_value_int(&it->value, value);
    it->op = CF_WRITE_SET;
    return CF_TRUE;
}

cf_bool cf_edit_set_bool(cf_edit* e, const char* key, cf_bool value) {
    cf_write_item* it = edit_slot(e, key);
    if (it == NULL) {
        return CF_FALSE;
    }
    cf_value_bool(&it->value, value);
    it->op = CF_WRITE_SET;
    return CF_TRUE;
}

#if CF_WITH_REAL
cf_bool cf_edit_set_real(cf_edit* e, const char* key, double value) {
    cf_write_item* it = edit_slot(e, key);
    if (it == NULL) {
        return CF_FALSE;
    }
    cf_value_real(&it->value, value);
    it->op = CF_WRITE_SET;
    return CF_TRUE;
}
#endif

cf_bool cf_edit_unset(cf_edit* e, const char* key) {
    cf_write_item* it = edit_slot(e, key);
    if (it == NULL) {
        return CF_FALSE;
    }
    cf_value_null(&it->value);
    it->op = CF_WRITE_DELETE;
    return CF_TRUE;
}

size_t cf_edit_count(const cf_edit* e) {
    return (e != NULL) ? e->count : 0;
}

void cf_edit_abort(cf_edit* e) {
    if (e != NULL) {
        cf_config* base = e->base;
        cf_arena_free(e->arena); /* the edit lives in it */
        cf_config_free(base);
    }
}

cf_bool cf_edit_commit(cf_edit* e, const char* note, cf_error* err) {
    cf_bool ok;

    if (e == NULL) {
        return cf_error_set(err, CF_E_SOURCE, NULL, NULL, "no edit to commit");
    }
    if (e->failed) {
        cf_error_copy(err, &e->first);
        cf_edit_abort(e);
        return CF_FALSE;
    }
    ok = cf_write(e->base, e->target, e->items, e->count, note, err);
    cf_edit_abort(e);
    return ok;
}
