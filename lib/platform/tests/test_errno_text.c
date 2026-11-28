/*
 * test_errno_text.c - the text of a system error (K-9k, D-239): a fixed
 * English table of our own, not the C library's shared buffer (CERT CON33-C).
 *
 * cfi_errno_text is private to lib/platform/src, so the test reads its
 * source whole, as a file of the test.
 */

#include <errno.h>
#include <string.h>

#include "../src/errno_text.c"

#include "cf_test.h"

int main(void) {
    char buf[64];
    char small[8];

    check(cfi_errno_text(ENOENT, buf, sizeof buf) == buf, "returns its buffer");
    check_text(buf, "no such file or directory", "ENOENT");
    (void)cfi_errno_text(EACCES, buf, sizeof buf);
    check_text(buf, "permission denied", "EACCES");
    (void)cfi_errno_text(EINVAL, buf, sizeof buf);
    check_text(buf, "invalid argument", "EINVAL");

    /* Any other code: `error <n>`. */
    (void)cfi_errno_text(99999, buf, sizeof buf);
    check_text(buf, "error 99999", "an unknown code");
    (void)cfi_errno_text(-3, buf, sizeof buf);
    check_text(buf, "error -3", "a negative code");

    /* A buffer of 8 bytes: cut and terminated. */
    memset(small, 'x', sizeof small);
    check(cfi_errno_text(ENOENT, small, sizeof small) == small,
          "returns the small buffer");
    check_text(small, "no such", "cut to 7 and a terminator");

    /* No capacity: nothing is written. */
    buf[0] = 'q';
    (void)cfi_errno_text(ENOENT, buf, 0u);
    check(buf[0] == 'q', "capacity 0 writes nothing");

    return cf_test_done();
}
