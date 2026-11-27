/*
 * nodiscard_ignored.c - drops the result of a create. Built as C23 with
 * -Werror it must be refused, which is what CF_NODISCARD is for
 * (cmake/gates/ConsumerNodiscard.cmake). Never part of a build.
 */

#include <configforge/configforge.h>

int main(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);

    if (b == NULL) {
        return 1;
    }
    cf_builder_create(b, &err);
    return 0;
}
