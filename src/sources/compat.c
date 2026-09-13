/*
 * compat.c - the two platform capabilities under their old names. C89.
 *
 * cf_environ and cf_cli_args were the public spelling before there was
 * a platform layer. They forward rather than being removed: the names
 * are in the interface, and a rename that buys nothing a caller can see
 * costs every caller a change.
 */

#include "configforge/cf_entry.h"
#include "configforge/cf_platform.h"
#include "configforge/cf_source.h"

const char* cf_source_path(const cf_source* s) {
    if (s == NULL || s->vt == NULL || s->vt->path == NULL) {
        return NULL;
    }
    return s->vt->path(s);
}

char* const* cf_environ(void) {
    return cf_platform_environ();
}

char* const* cf_cli_args(int argc, char* const* argv, cf_arena* arena, int* out_argc) {
    return cf_platform_args(argc, argv, arena, out_argc);
}
