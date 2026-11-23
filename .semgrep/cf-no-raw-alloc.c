/* Fixture for cf-no-raw-alloc: not compiled, not formatted. */
void fixture(void* p, unsigned n) {
    // ruleid: cf-no-raw-alloc
    p = malloc(n);
    // ruleid: cf-no-raw-alloc
    p = realloc(p, n * 2);
    // ruleid: cf-no-raw-alloc
    free(p);
    // ruleid: cf-no-raw-alloc
    p = calloc(n, 4);
    // ok: cf-no-raw-alloc
    p = cf_alloc(n);
    // ok: cf-no-raw-alloc
    cf_free_all(p);
    /* malloc(n) in a comment */
    // ok: cf-no-raw-alloc
    n = my_freelist_size(n);
}
