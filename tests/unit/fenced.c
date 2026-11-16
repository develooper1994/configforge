/* A program does not see the internal tier (D-51): this must not
   compile, and the compiler's message names cf_ks_count. */
#undef CF_INTERNAL_API
#define CF_INTERNAL_API 0
#include "configforge/configforge.h"

size_t (*fenced_count)(const cf_keyspace*) = cf_ks_count;
