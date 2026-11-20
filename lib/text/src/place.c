/*
 * place.c - the line and column of an offset, in the original's units (D-108).
 * C89.
 *
 * A byte with no character is refused where it stands, and "where" is counted
 * as the reader of that encoding would: bytes for the single-byte encodings,
 * two-byte code units for UTF-16, a line ending at each LF unit, columns from
 * 1.
 */

#include "configforge/cf_port.h"
#include "configforge/cf_encoding.h"
#include "configforge/cf_text.h"
#include <stddef.h>

void cf_text_place(const char* p, size_t at, unsigned unit, cf_u32* line, cf_u32* col) {
    const unsigned char* u    = (const unsigned char*)p;
    cf_u32               ln   = 1;
    cf_u32               cl   = 1;
    size_t               step = 1;
    size_t               i;

    if (unit == (unsigned)CF_ENCODING_UTF16LE ||
        unit == (unsigned)CF_ENCODING_UTF16BE) {
        step = 2;
    }
    for (i = 0; i + step <= at; i += step) {
        cf_bool lf;

        if (step == 1) {
            lf = (u[i] == 0x0Au) ? CF_TRUE : CF_FALSE;
        } else if (unit == (unsigned)CF_ENCODING_UTF16LE) {
            lf = (u[i] == 0x0Au && u[i + 1] == 0) ? CF_TRUE : CF_FALSE;
        } else {
            lf = (u[i] == 0 && u[i + 1] == 0x0Au) ? CF_TRUE : CF_FALSE;
        }
        if (lf) {
            ++ln;
            cl = 1;
        } else {
            ++cl;
        }
    }
    *line = ln;
    *col  = cl;
}
