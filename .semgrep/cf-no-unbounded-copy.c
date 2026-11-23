/* Fixture for cf-no-unbounded-copy: not compiled, not formatted. */
void fixture(char* dst, const char* src, unsigned n) {
    // ruleid: cf-no-unbounded-copy
    strcpy(dst, src);
    // ruleid: cf-no-unbounded-copy
    strncat(dst, src, n);
    // ruleid: cf-no-unbounded-copy
    gets(dst);
    // ruleid: cf-no-unbounded-copy
    strtok(dst, ",");
    // ok: cf-no-unbounded-copy
    memcpy(dst, src, n);
    // ok: cf-no-unbounded-copy
    cf_strlcpy_like(dst, src, n);
    /* strcpy(dst, src) in a comment */
    // ok: cf-no-unbounded-copy
    n = strlen(src);
}
