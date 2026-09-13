/*
 * options.c - does a consumer see the same build this library is?
 *
 * Compiled with a plain -I against the install prefix, deliberately not
 * through CMake: that is the path where nothing hands over the feature
 * definitions, and it is the path where the defaults used to win.
 *
 * A mismatch here is not a link error in general. The public headers
 * change shape with these: CF_WITH_REAL decides whether cf_value's
 * union has a double in it, so a consumer who disagrees has a different
 * sizeof and a corruption that shows up somewhere else entirely. The
 * link error that first revealed this was the lucky outcome.
 */

#include <configforge/configforge.h>

#include <stdio.h>

int main(void) {
    int wrong = 0;

#if CF_WITH_REAL != CF_EXPECT_REAL
#error "the installed headers disagree with the library about CF_WITH_REAL"
#endif
#if CF_WITH_JSON != CF_EXPECT_JSON
#error "the installed headers disagree with the library about CF_WITH_JSON"
#endif

    /* And the same at run time, because a header can say one thing
       while the archive was built from another. cf_has_feature reports
       what the compiled objects were compiled with. */
    if (cf_has_feature("real") != (CF_EXPECT_REAL ? CF_TRUE : CF_FALSE)) {
        printf("options: header says real=%d, library says %d\n", (int)CF_WITH_REAL,
               (int)cf_has_feature("real"));
        wrong = 1;
    }
    if (cf_has_feature("json") != (CF_EXPECT_JSON ? CF_TRUE : CF_FALSE)) {
        printf("options: json disagrees\n");
        wrong = 1;
    }
    if (!wrong) {
        printf("options ok: real=%d json=%d, header and library agree\n",
               (int)CF_WITH_REAL, (int)CF_WITH_JSON);
    }
    return wrong;
}
