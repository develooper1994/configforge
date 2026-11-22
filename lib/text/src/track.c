/*
 * track.c - running line and byte-column counts. C89.
 */

#include "configforge/cf_text.h"

void cf_text_track_init(cf_text_track* t, const char* start) {
    t->line_start = start;
    t->line       = 1;
}

void cf_text_track_line(cf_text_track* t, const char* after) {
    t->line_start = after;
    ++t->line;
}

unsigned int cf_text_track_col(const cf_text_track* t, const char* at) {
    return (unsigned int)(at - t->line_start) + 1u;
}
