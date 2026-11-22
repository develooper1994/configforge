/*
 * convert_core.h - a document read through one codec and written through
 * another, without a process and without a message to parse (D-94,
 * D-109). `configforge codec convert` prints from the result; the pairs
 * test and the chain fuzzer read it. C89.
 */

#ifndef CONFIGFORGE_CODEC_CONVERT_CORE_H
#define CONFIGFORGE_CODEC_CONVERT_CORE_H

#include "configforge/cf_serialize.h"

typedef enum cv_kind {
    CV_NONE = 0, /* no finding */
    CV_NAME,
    CV_EMPTY_NAME,
    CV_ARRAY,
    CV_EMPTY_ARRAY,
    CV_TABLE_ARRAY,
    CV_NESTED_ARRAY,
    CV_EMPTY_TABLE,
    CV_LONG_PATH,
    CV_LONG_NAME,
    CV_NULL,
    CV_LINE_BREAK,
    CV_NUL,
    CV_DEEP, /* nested deeper than the target's reader accepts */
    CV_OTHER /* a refusal the table did not predict */
} cv_kind;

typedef struct cv_finding {
    cv_kind     kind;
    const char* path; /* as the message prints it, in the arena */
    const char* name; /* the node's own name, escaped, for CV_NAME; else NULL */
} cv_finding;

typedef struct cv_result {
    const cv_finding* findings;
    size_t            nfindings;
    size_t            as_text; /* values written as their text */
    cf_blob           out;     /* the target's bytes, in the arena */
} cv_result;

/* `in` read through `from`, written through `to`: the tree, the passes,
   the replay. 0 when written, with *res set; else the exit code convert
   would give (1 a document `from` refuses, 2 a bound or no memory, 3 a
   refusal the passes let through) with `err` set; for 3, *res holds the
   findings made before it. `to` must write and its state must fit.
   Nothing is printed. */
int cv_convert(cf_arena* ar, const cf_blob* in, const cf_codec* from,
               const cf_codec* to, cv_result* res, cf_error* err);

#endif /* CONFIGFORGE_CODEC_CONVERT_CORE_H */
