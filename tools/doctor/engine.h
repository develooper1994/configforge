/*
 * engine.h - Doctor's engine, shared by the library (engine.c, behind
 * cf_doctor.h) and the command line (doctor.c). Internal: nothing here
 * is installed.
 */

#ifndef CONFIGFORGE_DOCTOR_ENGINE_H
#define CONFIGFORGE_DOCTOR_ENGINE_H

#include <configforge/cf_doctor.h>
#include <configforge/configforge.h>

/* Both arrays come from the report's own arena rather than sitting
   inside the struct. An earlier version held them inline and put 7.7
   megabytes on the stack, which the build's stack budget caught: that
   budget exists so a tool cannot quietly make the library unusable on
   a small target, and it works on the tools too. */

typedef struct cand {
    const char* source;
    const char* value; /* NULL when redacted            */
    const char* at;
    const char* outcome; /* NULL means selected           */
    const char* reason;
    const char* digest;
    cf_i64      length;
    cf_bool     world_readable; /* a file every user can read; see CFD007 */
} cand;

typedef struct rkey {
    const char* key;
    size_t      keylen;
    cand*       c;
    size_t      nc;
    const char* selected; /* winning source name           */
    cf_bool     required;
    cf_bool     sensitive;
    cf_bool     deprecated;
    const char* replaced_by; /* the new name, when it was renamed */
    cf_bool     defined;
    const char* type;
    /* The definition's policy: the sources permitted to supply this
       key, in decreasing precedence. NULL-terminated, NULL if none. */
    const char** policy;
} rkey;

/* One entry of the report's "sources" list. Rank is the library's own:
   lower wins. */
typedef struct rsrc {
    const char* name;
    const char* kind;
    cf_i64      rank;
} rsrc;

typedef struct report {
    cf_doctor*  D; /* where its findings go */
    cf_arena*   arena;
    cf_keyspace doc;
    const char* path;
    const char* mode;
    const char* profile;
    rkey*       keys;
    size_t      nkeys;
    rsrc*       src;
    size_t      nsrc;
} report;

/* Findings already accepted, "CFD001 server.port" a line. */
#define MAX_BASELINE 512

#define MAX_BASELINE 512

typedef struct baseline {
    char    id[MAX_BASELINE][16];
    char    key[MAX_BASELINE][CF_KEY_MAX];
    cf_bool matched[MAX_BASELINE];
    size_t  count;
    cf_bool loaded;
} baseline;

/* Where a project disagrees with a rule's default. */
#define MAX_SEVERITY 256

#define MAX_SEVERITY 256

typedef struct severity_line {
    char id[16];
    char level[16];
    char profile[64]; /* empty: every profile */
} severity_line;

/* One finding, as recorded. */
typedef struct dfinding {
    char id[16];
    char level[16];
    char key[CF_KEY_MAX];
    char at[256];
    char msg[512];
} dfinding;

/* Everything one run of the rules knows, which the program kept in
   globals while it was only a program: a library has to let two runs
   happen at once. */
struct cf_doctor {
    baseline      base;
    severity_line sev[MAX_SEVERITY];
    size_t        nsev;
    const char*   profile; /* the report being linted, when there is one */
    dfinding*     found;
    size_t        nfound, capfound;
    int           findings, errors, warnings, infos, baselined;
    cf_finding    view; /* what cf_doctor_finding hands out */
};

/* Reading reports. False with `err` set, worded for a person. */
cf_bool doctor_load(report* r, cf_doctor* D, const char* path, cf_error* err);
cf_bool doctor_load_text(report* r, cf_doctor* D, const char* text, size_t n,
                         const char* name, cf_error* err);
void    doctor_release(report* r);

const rkey* doctor_find_key(const report* r, const char* key, size_t n);
const char* doctor_shown(const cand* c);
cf_bool     doctor_place_of(const char* at, size_t* len, long* pos);
void        doctor_key_name(const rkey* k, char* buf, size_t cap);

/* The rules over one report, and over two. */
void doctor_lint_reports(const report* r);
void doctor_diff_reports(const report* a, const report* b);

cf_bool doctor_level_known(const char* l);
int     doctor_level_rank(const char* l);

#endif /* CONFIGFORGE_DOCTOR_ENGINE_H */
