/*
 * misread.c - the log of reads the contract does not have (D-121). C89.
 *
 * A read takes no lock (D-14), so the log is a fixed table whose slots are
 * claimed with an atomic counter, written, and published by raising the
 * slot's `ready` count; a reader of the log looks only at ready slots.
 * Two threads recording one misread at the same moment may fill two
 * slots, so the count and the indexed reader skip a ready slot equal to
 * an earlier ready one: what they say is exact.
 */

#include "configforge/cf_report.h"
#include "configforge/cf_port.h"

#include <string.h>

static size_t claimed(const cf_misread_log* log) {
    cf_i32 n = cf_ref_get(&log->next);

    if (n < 0) {
        return 0;
    }
    return ((size_t)n < CF_MISREAD_SLOTS) ? (size_t)n : (size_t)CF_MISREAD_SLOTS;
}

static cf_bool slot_ready(const cf_misread_slot* s) {
    return (cf_ref_get(&s->ready) != 0) ? CF_TRUE : CF_FALSE;
}

static cf_bool same(const cf_misread_slot* s, unsigned kind, unsigned wanted,
                    const char* key, size_t keylen) {
    return (s->kind == kind && s->wanted == wanted && strlen(s->key) == keylen &&
            memcmp(s->key, key, keylen) == 0)
               ? CF_TRUE
               : CF_FALSE;
}

/* The slot is a repeat of one earlier in the table. */
static cf_bool is_repeat(const cf_misread_log* log, size_t i) {
    size_t j;

    for (j = 0; j < i; ++j) {
        if (slot_ready(&log->slot[j]) &&
            same(&log->slot[j], log->slot[i].kind, log->slot[i].wanted,
                 log->slot[i].key, strlen(log->slot[i].key))) {
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}

size_t cfi_misread_count(const cf_misread_log* log) {
    size_t n, i, count = 0;

    if (log == NULL) {
        return 0;
    }
    n = claimed(log);
    for (i = 0; i < n; ++i) {
        if (slot_ready(&log->slot[i]) && !is_repeat(log, i)) {
            ++count;
        }
    }
    return count;
}

const cf_misread_slot* cfi_misread_at(const cf_misread_log* log, size_t i) {
    size_t n, j, seen = 0;

    if (log == NULL) {
        return NULL;
    }
    n = claimed(log);
    for (j = 0; j < n; ++j) {
        if (slot_ready(&log->slot[j]) && !is_repeat(log, j)) {
            if (seen == i) {
                return &log->slot[j];
            }
            ++seen;
        }
    }
    return NULL;
}

void cfi_misread_record(cf_misread_log* log, const char* key, size_t keylen,
                        unsigned kind, cf_type wanted, cf_type found) {
    size_t           n, i;
    cf_misread_slot* s;

    if (log == NULL || key == NULL) {
        return;
    }
    if (keylen > CF_KEY_MAX - 1) {
        keylen = CF_KEY_MAX - 1;
    }
    n = claimed(log);
    for (i = 0; i < n; ++i) {
        if (slot_ready(&log->slot[i]) &&
            same(&log->slot[i], kind, (unsigned)wanted, key, keylen)) {
            return;
        }
    }
    if (cf_ref_get(&log->next) >= CF_MISREAD_SLOTS) {
        return;
    }
    i = (size_t)cf_ref_inc(&log->next) - 1;
    if (i >= CF_MISREAD_SLOTS) {
        return;
    }
    s         = &log->slot[i];
    s->kind   = (unsigned char)kind;
    s->wanted = (unsigned char)wanted;
    s->found  = (unsigned char)found;
    memcpy(s->key, key, keylen);
    s->key[keylen] = '\0';
    (void)cf_ref_inc(&s->ready);
}
