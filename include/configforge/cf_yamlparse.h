/*
 * cf_yamlparse.h - the built-in YAML parser's interface to yaml.c (D-69).
 * Internal: no function here is exported, and a program has no use for it.
 *
 * The parser reads YAML 1.2 (yaml.org/spec/1.2.2) and calls back with
 * events: a node - a mapping or a sequence opening, a scalar, or the end
 * of a collection - with its tag resolved and its anchor; an alias; the
 * start of a document; the end of the stream. What a node means to a
 * configuration, and anchors' replay, are yaml.c's, as they are for
 * libfyaml's and libyaml's events.
 */

#ifndef CONFIGFORGE_CF_YAMLPARSE_H
#define CONFIGFORGE_CF_YAMLPARSE_H

#include "configforge/cf_serialize.h"

/* Which parser reads YAML (D-62, D-69): the build defines one of these for
   libfyaml or libyaml; with neither, the built-in one does. */
#ifndef CF_YAML_LIBFYAML
#define CF_YAML_LIBFYAML 0
#endif
#ifndef CF_YAML_LIBYAML
#define CF_YAML_LIBYAML 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum { CF_YP_MAP, CF_YP_SEQ, CF_YP_SCALAR, CF_YP_END };

typedef struct cf_yp_node {
    int         kind;
    const char* text; /* a scalar's value, decoded; lives until the call returns */
    size_t      n;
    const char* tag; /* resolved: NULL for none, "!" for the non-specific one */
    cf_bool     plain;
    int         style;  /* ':' plain, a quote, '|' or '>' block, '[' or '{' flow */
    const char* anchor; /* in the document, or NULL */
    size_t      alen;
    unsigned    line, col; /* 1-based */
} cf_yp_node;

typedef struct cf_yp_events {
    void* ctx;
    cf_bool (*document)(void* ctx, unsigned line, unsigned col);
    cf_bool (*node)(void* ctx, const cf_yp_node* n);
    cf_bool (*alias)(void* ctx, const char* name, size_t n, unsigned line,
                     unsigned col);
    cf_bool (*stream_end)(void* ctx, unsigned line, unsigned col);
    /* Room the parser keeps until it returns: a scalar's decoded text.
       NULL when there is none to lend. */
    void* (*work)(void* ctx, size_t n);
} cf_yp_events;

/* Reads the whole document. False with `err` set - "file:line:col: what"
   - at the first thing that is not YAML, or when a callback refuses. */
cf_bool cf_yaml_parse(const cf_blob* in, const char* file, const cf_yp_events* ev,
                      cf_error* err);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_YAMLPARSE_H */
