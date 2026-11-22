/*
 * fail.c - the common form of a reader's refusal. C89.
 */

#include "configforge/cf_text.h"
#include "configforge/cf_build_options.h"
#include "configforge/cf_error.h"
#include "configforge/cf_port.h"

#include <stdarg.h>
#include <stddef.h>

cf_bool cf_text_fail(cf_error* err, cf_code code, const char* file, unsigned int line,
                     unsigned int col, const char* fmt, ...) {
    char    body[CF_MESSAGE_MAX];
    va_list ap;

    if (err == NULL) {
        return CF_FALSE;
    }
    va_start(ap, fmt);
    cf_vfmt(body, sizeof body, fmt, ap);
    va_end(ap);
    if (col == 0) {
        return cf_error_set(err, code, NULL, file, "%s:%u: %s", file, line, body);
    }
    return cf_error_set(err, code, NULL, file, "%s:%u:%u: %s", file, line, col, body);
}
