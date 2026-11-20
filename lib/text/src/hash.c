/*
 * hash.c - the seeded hash of a reader's name table, once (D-102). C89.
 */

#include "configforge/cf_text.h"
#include "configforge/cf_port.h"

#include <stddef.h>

/* The seed every hash of a name in a decode starts from, so that a
   document's author cannot choose names that all land in one bucket: the
   hash was FNV-1a from its fixed offset basis, which anyone can compute
   against. The library has no clock and no random source, so the seed is
   made of two addresses, the table's and a local's, which differ from run to
   run where the system randomises addresses and which a document cannot
   know. Where it does not (no address randomisation), the seed is the
   same every run: no worse than the unseeded hash was, and no better.
   Nothing observable may depend on it: no table is walked in bucket
   order. The high bits of an address are the random ones and a bucket
   index is the low bits of a hash, so they are folded down first. The
   two addresses are often both on the stack, sharing their random high
   bits, so the second is multiplied before the XOR lest they cancel. */
size_t cf_text_seed(const void* where) {
    char   here;
    size_t x = (size_t)where ^ ((size_t)&here * 2654435761u);

    x ^= x >> 12;
    x ^= x >> 24;
    x *= 16777619u;
    x = (size_t)(cf_u32)(x ^ (x >> 16 >> 16));
    return (x == 0) ? (size_t)2166136261u : x;
}

size_t cf_text_hash(size_t seed, const char* s, size_t n) {
    size_t h = seed, i;
    for (i = 0; i < n; ++i) {
        h = (h ^ (size_t)(unsigned char)s[i]) * 16777619u;
    }
    return h;
}
