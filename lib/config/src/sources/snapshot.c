/*
 * snapshot.c - an existing configuration as a source. C89.
 *
 * What deriving needs: replay everything a key space holds into another
 * one, at the ranks the candidates originally had.
 *
 * It takes a key space rather than a configuration on purpose. A source
 * has no business knowing what a cf_config is - the layering gate says
 * so - and replaying a key space is all this actually does.
 *
 * Every candidate, not only the winners. Dropping the losers would make
 * explain() on the derived configuration say less than on the one it
 * came from, and a library whose premise is explaining itself must not
 * lose provenance when a value changes - that is exactly the moment
 * somebody asks.
 */

#include "configforge/cf_explain.h"
#include "configforge/cf_source.h"

#include <string.h>

static cf_bool snapshot_load(const cf_source* s, cf_keyspace* ks) {
    const cf_keyspace* base = (const cf_keyspace*)s->ref;
    return cf_ks_replay(ks, base);
}

static void snapshot_destroy(cf_source* s) {
    (void)s;
}

static const cf_source_vtable cf_snapshot_vt = {
    sizeof(cf_source_vtable), "snapshot", snapshot_load, snapshot_destroy, NULL, NULL};

cf_bool cf_source_snapshot(cf_source* s, const char* name, const cf_keyspace* ks) {
    if (s == NULL || ks == NULL) {
        return CF_FALSE;
    }
    s->vt   = &cf_snapshot_vt;
    s->name = (name != NULL) ? name : "snapshot";
    /* Rank is ignored: every candidate carries the rank it had. */
    s->rank = CF_RANK_DEFAULT;
    s->ext  = NULL;
    s->ref  = ks;
    return CF_TRUE;
}
