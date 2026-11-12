/*
 * test_marker.c - the marker-secret gate: a secret value appears in no
 * text the library writes.
 *
 * test_secrets.c proves, one case at a time, that a sensitive value is
 * not shown by the output somebody thought of. Leaks were found by hand
 * that way, one a day: a refusal quoting the value, a reference quoted
 * inside a refusal, a name that was not marked and so showed what the
 * new name hid. This test is the other direction. It takes a marker
 * for a value - "MRK7731XQ", and digits for a number, since a number
 * cannot spell one - and drives it through every way the library has to
 * accept or refuse a value: every refusal reason in validate.c, from
 * every kind of source the test can make, strictly and with fallback, as
 * a winner and as a loser. Then it collects every text the library
 * writes about the configuration - the error, explain() of every key,
 * the blind report whole and one key at a time, inspect(), the diff in
 * both forms, the fingerprint, the schema, the log events, the reloads
 * of a cf_live - and fails if the marker, or its first six characters,
 * appears in any of them. A new output, a new refusal reason or a new
 * source is one table row or one call here, and then it is held to the
 * same rule as the rest.
 *
 * What is not collected, and why: a full report, cf_emit, cf_write and
 * cf_image_write carry values on purpose (they are what a configuration
 * is read back from), and cf_get_text is the program reading its own
 * secret. cf_doctor lives with Doctor and is its own task.
 *
 * Reasons in validate.c and the scenario that reaches each:
 *   "%I is outside %I..%I", "%s is outside"      range int, range real
 *   "shorter than", "longer than", "not one of"  min_len, max_len, allowed
 *   "'${%S' refers to no key"                    malformed reference (a
 *                                                secret's is the sealed one)
 *   "is not a valid %s"                          type int, bool, real
 *   "source supplied %s where %s was declared"   typed JSON number, text key
 *   "no room to unescape"                        not reachable: out of memory
 *   "its references lead back to it"             reference cycle
 *   "${%S} names a key with no value" and the
 *     sealed form                                no-value, undefined key and
 *                                                a defined key without value
 *   "refers to the sensitive %S"                 public key naming a secret,
 *                                                also through an old name
 *   "references nest deeper", "expand to more    not reached: they say only the
 *     than %U bytes", "no room to replace"       key, and need a megabyte
 *   "policy: '%s' may only be set from %s",
 *     "policy puts %s first"                     policy scenarios
 *   "past the end: %s is %s"                     list element past the count
 *   "append list counts must be literal
 *     integers"                                  append list, templated count
 *   "appended list length overflows"             not reached: says only the key
 *   "list elements need a .count value"          list without count
 *   "list count is not its %zu selected"         list, wrong count
 *   "a declared list uses .count and canonical
 *     decimal indices"                           scalar written on a list
 *   "list replacement must be a declared list",
 *     "is required and has no value", "unknown
 *     key"                                       not reached: no value in them
 * validate.c has no pattern constraint.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cf_test.h"

#define MK_V "MRK7731XQ"
#define MK_D "77317731"

/* What must not appear: the marker, its first six characters, and the
   same of the digits. */
static const char* const mk_needles[] = {"MRK7731XQ", "MRK773", "77317731", "773177",
                                         "mrk7731xq", "mrk773", NULL};

typedef enum mk_shape {
    SH_TEXT,
    SH_TEXT_DEFAULT,
    SH_INT,
    SH_REAL,
    SH_BOOL,
    SH_ALLOWED,
    SH_MAXLEN,
    SH_MINLEN,
    SH_LIST_INT,
    SH_LIST_FREE,
    SH_LIST_ALLOWED,
    SH_LIST_APPEND,
    SH_OLD,
    SH_OLD_MAXLEN,
    SH_POLICY,
    SH_POLICY_SHADOW
} mk_shape;

/* The kinds of source a secret can come in. SRC_ALL runs a scenario
   through each of the first eight. */
typedef enum mk_src {
    SRC_MEM,
    SRC_INI,
    SRC_JSON,
    SRC_ENV,
    SRC_CLI,
    SRC_DOTENV,
    SRC_CLI_SPACE, /* --key value, two words                  */
    SRC_CLI_ALIAS, /* --pw value, the alias a definition gives */
    SRC_JSON_NUM,  /* a JSON number, not a string             */
    SRC_ALL
} mk_src;

static const char* const mk_src_name[] = {"mem",       "ini",       "json",
                                          "env",       "cli",       "dotenv",
                                          "cli-space", "cli-alias", "json-number"};

typedef struct mk_scn {
    const char*    name;
    mk_shape       shape;
    mk_src         src;
    const char*    v1;    /* the winning value, or NULL for none     */
    const char*    v2;    /* a losing one, from a memory source      */
    const cf_pair* extra; /* more pairs, key NULL ends them          */
    int            strict_ok;
    int            lenient_ok;
    int            holds;      /* the program reads the value back as is  */
    const char*    ctl;        /* the key that read is of; NULL: the secret */
    const char*    known_leak; /* part of an output's name: a leak the library has */
    const char*    needs;      /* an optional part the row cannot run without */
} mk_scn;

/* Pairs a scenario adds to the memory source, always plain. */
static const cf_pair mk_x_other[]   = {{"sec.other", "MRK7731XQ-other"}, {NULL, NULL}};
static const cf_pair mk_x_pub[]     = {{"pub.url", "https://${sec.v}/x"}, {NULL, NULL}};
static const cf_pair mk_x_pub_old[] = {{"pub.url", "https://${db.pass}/x"},
                                       {NULL, NULL}};
static const cf_pair mk_x_pub_sec[] = {
    {"pub.url", "a${sec.other}"}, {"sec.other", "MRK7731XQ-other"}, {NULL, NULL}};
static const cf_pair mk_x_cut[] = {
    {"sec.l.count", "1"}, {"sec.l.0", "1"}, {"sec.l.1", "MRK7731XQ"}, {NULL, NULL}};
static const cf_pair mk_x_free[]  = {{"sec.l.count", "2"},
                                     {"sec.l.0", "MRK7731XQ"},
                                     {"sec.l.1", "MRK7731XQ-b"},
                                     {NULL, NULL}};
static const cf_pair mk_x_ltype[] = {
    {"sec.l.count", "1"}, {"sec.l.0", "MRK7731XQ"}, {NULL, NULL}};
static const cf_pair mk_x_lrange[] = {
    {"sec.l.count", "1"}, {"sec.l.0", "77317731"}, {NULL, NULL}};
static const cf_pair mk_x_lallow[] = {
    {"sec.l.count", "1"}, {"sec.l.0", "MRK7731XQ"}, {NULL, NULL}};
static const cf_pair mk_x_lcount[] = {
    {"sec.l.count", "77317731"}, {"sec.l.0", "1"}, {NULL, NULL}};
static const cf_pair mk_x_lcountt[] = {
    {"sec.l.count", "MRK7731XQ"}, {"sec.l.0", "1"}, {NULL, NULL}};
static const cf_pair mk_x_lnocnt[]  = {{"sec.l.0", "MRK7731XQ"}, {NULL, NULL}};
static const cf_pair mk_x_lscalar[] = {{"sec.l", "MRK7731XQ"}, {NULL, NULL}};
static const cf_pair mk_x_lappend[] = {
    {"sec.l.count", "MRK7731XQ${ref.n}"}, {"sec.l.0", "MRK7731XQ"}, {NULL, NULL}};
static const cf_pair mk_x_lappok[] = {
    {"sec.l.count", "1"}, {"sec.l.0", "MRK7731XQ"}, {NULL, NULL}};

/* clang-format off */
static const mk_scn mk_table[] = {
    /* a. accepted */
    {"accepted text", SH_TEXT, SRC_ALL, MK_V, NULL, NULL, 1, 1, 1, NULL, NULL, NULL},
    {"accepted, a loser holds a marker too", SH_TEXT, SRC_ALL, MK_V "-win", MK_V "-lose", NULL, 1, 1, 1, NULL, NULL, NULL},
    {"accepted default", SH_TEXT_DEFAULT, SRC_MEM, NULL, NULL, NULL, 1, 1, 1, NULL, NULL, NULL},
    {"accepted with an escaped $${", SH_TEXT, SRC_ALL, MK_V "$${lit", NULL, NULL, 1, 1, 1, NULL, NULL, NULL},
    {"accepted, names a public key", SH_TEXT, SRC_ALL, MK_V "-${pub.host}", NULL, NULL, 1, 1, 1, NULL, NULL, NULL},
    {"accepted, names another secret", SH_TEXT, SRC_ALL, MK_V "-${sec.other}", NULL, mk_x_other, 1, 1, 1, NULL, NULL, NULL},
    /* b. malformed and unresolvable references in the secret */
    {"malformed reference", SH_TEXT, SRC_ALL, MK_V "${", NULL, NULL, 0, 1, 0, NULL, NULL, NULL},
    {"malformed reference, unclosed", SH_TEXT, SRC_ALL, MK_V "${nokey", NULL, NULL, 0, 1, 0, NULL, NULL, NULL},
    {"malformed reference, empty", SH_TEXT, SRC_ALL, MK_V "${}", NULL, NULL, 0, 1, 0, NULL, NULL, NULL},
    {"malformed reference, the marker after it", SH_TEXT, SRC_ALL, "${" MK_V, NULL, NULL, 0, 1, 0, NULL, NULL, NULL},
    {"malformed reference, a name made of it", SH_TEXT, SRC_ALL, "${" MK_V "}", NULL, NULL, 0, 1, 0, NULL, NULL, NULL},
    {"malformed reference, a name with a dot", SH_TEXT, SRC_ALL, "x${" MK_V ".k}y", NULL, NULL, 0, 1, 0, NULL, NULL, NULL},
    {"malformed reference, a loser holds a marker", SH_TEXT, SRC_ALL, MK_V "${", MK_V "-lose", NULL, 0, 1, 0, NULL, NULL, NULL},
    {"reference to an undefined key", SH_TEXT, SRC_ALL, MK_V "${nokey}", NULL, NULL, 0, 0, 0, NULL, NULL, NULL},
    {"reference to an undefined key named by the marker", SH_TEXT, SRC_ALL, "${mrk7731xq}", NULL, NULL, 0, 0, 0, NULL, NULL, NULL},
    {"reference to an undefined dotted key named by it", SH_TEXT, SRC_ALL, "a${mrk7731xq.k}b", NULL, NULL, 0, 0, 0, NULL, NULL, NULL},
    /* c. a key with no value, and a cycle */
    {"reference to a key with no value", SH_TEXT, SRC_ALL, MK_V "${ref.t}", NULL, NULL, 0, 0, 0, NULL, NULL, NULL},
    {"reference cycle", SH_TEXT, SRC_ALL, MK_V "${sec.v}", NULL, NULL, 0, 0, 0, NULL, NULL, NULL},
    /* d. wrong type */
    {"wrong type, int", SH_INT, SRC_ALL, MK_V, NULL, NULL, 0, 1, 0, NULL, NULL, NULL},
    {"wrong type, real", SH_REAL, SRC_ALL, MK_V, NULL, NULL, 0, 1, 0, NULL, NULL, "real"},
    {"wrong type, bool", SH_BOOL, SRC_ALL, MK_V, NULL, NULL, 0, 1, 0, NULL, NULL, NULL},
    {"wrong type, a loser holds a marker", SH_INT, SRC_ALL, MK_V, MK_V "-lose", NULL, 0, 1, 0, NULL, NULL, NULL},
    {"typed source, a number for text", SH_TEXT, SRC_JSON_NUM, MK_D, NULL, NULL, 0, 1, 0, NULL, NULL, NULL},
    /* e. out of range */
    {"out of range, int", SH_INT, SRC_ALL, MK_D, NULL, NULL, 0, 1, 0, NULL, NULL, NULL},
    {"out of range, real", SH_REAL, SRC_ALL, MK_D, NULL, NULL, 0, 1, 0, NULL, NULL, "real"},
    /* f. the other constraints */
    {"not in the allowed set", SH_ALLOWED, SRC_ALL, MK_V, NULL, NULL, 0, 1, 0, NULL, NULL, NULL},
    {"too long", SH_MAXLEN, SRC_ALL, MK_V, NULL, NULL, 0, 1, 0, NULL, NULL, NULL},
    {"too short", SH_MINLEN, SRC_ALL, MK_V, NULL, NULL, 0, 1, 0, NULL, NULL, NULL},
    /* g. lists */
    {"list, an element past the count", SH_LIST_FREE, SRC_MEM, NULL, NULL, mk_x_cut, 1, 1, 0, NULL, NULL, NULL},
    {"list, accepted", SH_LIST_FREE, SRC_MEM, NULL, NULL, mk_x_free, 1, 1, 1, "sec.l.0", NULL, NULL},
    {"list, element of the wrong type", SH_LIST_INT, SRC_MEM, NULL, NULL, mk_x_ltype, 0, 0, 0, NULL, NULL, NULL},
    {"list, element out of range", SH_LIST_INT, SRC_MEM, NULL, NULL, mk_x_lrange, 0, 0, 0, NULL, NULL, NULL},
    {"list, element not allowed", SH_LIST_ALLOWED, SRC_MEM, NULL, NULL, mk_x_lallow, 0, 0, 0, NULL, NULL, NULL},
    {"list, wrong count", SH_LIST_INT, SRC_MEM, NULL, NULL, mk_x_lcount, 0, 0, 0, NULL, NULL, NULL},
    {"list, the count is the marker", SH_LIST_INT, SRC_MEM, NULL, NULL, mk_x_lcountt, 0, 0, 0, NULL, NULL, NULL},
    {"list, elements without a count", SH_LIST_FREE, SRC_MEM, NULL, NULL, mk_x_lnocnt, 0, 0, 0, NULL, NULL, NULL},
    {"list, a scalar written on it", SH_LIST_FREE, SRC_MEM, NULL, NULL, mk_x_lscalar, 0, 0, 0, NULL, NULL, NULL},
    {"append list, accepted", SH_LIST_APPEND, SRC_MEM, NULL, NULL, mk_x_lappok, 1, 1, 1, "sec.l.0", NULL, NULL},
    {"append list, a templated count", SH_LIST_APPEND, SRC_MEM, NULL, NULL, mk_x_lappend, 0, 0, 0, NULL, NULL, NULL},
    /* h. the secret under a deprecated name */
    {"old name, accepted", SH_OLD, SRC_ALL, MK_V, NULL, NULL, 1, 1, 1, "db.password", NULL, NULL},
    {"old name, a loser holds a marker", SH_OLD, SRC_ALL, MK_V "-win", MK_V "-lose", NULL, 1, 1, 1, "db.password", NULL, NULL},
    {"old name, too long", SH_OLD_MAXLEN, SRC_ALL, MK_V, NULL, NULL, 0, 1, 0, NULL, NULL, NULL},
    {"old name, malformed reference", SH_OLD, SRC_ALL, MK_V "${", NULL, NULL, 0, 1, 0, NULL, NULL, NULL},
    /* i. a public key naming the secret */
    {"public key names the secret", SH_TEXT, SRC_MEM, MK_V, NULL, mk_x_pub, 0, 0, 0, NULL, NULL, NULL},
    {"public key names the old name", SH_OLD, SRC_MEM, MK_V, NULL, mk_x_pub_old, 0, 0, 0, NULL, NULL, NULL},
    {"public key names another secret", SH_TEXT, SRC_MEM, MK_V, NULL, mk_x_pub_sec, 0, 0, 0, NULL, NULL, NULL},
    /* policy: a refused source, and a shadowed one */
    {"policy refuses the source", SH_POLICY, SRC_ALL, MK_V, NULL, NULL, 1, 1, 0, NULL, NULL, NULL},
    {"policy puts another first", SH_POLICY_SHADOW, SRC_ALL, MK_V "-win", MK_V "-lose", NULL, 1, 1, 0, NULL, NULL, NULL}
};
/* clang-format on */

#define MK_COUNT (sizeof mk_table / sizeof mk_table[0])

/* What is being run now, so every check can say where it failed. */
static const mk_scn* mk_s;
static mk_src        mk_kind;
static int           mk_lenient;
static int  mk_variant; /* 0 as written, 1 changed, 2 empty, 3 empty with a store */
static char mk_where[200];
static char mk_tag[96];

static char mk_buf[65536];
static char mk_log[32768];

static void mk_context(char* out, size_t cap) {
    cf_fmt(out, cap, "%s [%s, %s]", mk_s->name, mk_src_name[mk_kind],
           mk_lenient ? "fallback" : "strict");
}

/* The check: no needle in the text. On failure the scenario, the output
   and the sixty characters around the first hit. */
static void marker_absent(const char* where, const char* text) {
    size_t i;

    ++g_run;
    if (text == NULL) {
        return;
    }
    for (i = 0; mk_needles[i] != NULL; ++i) {
        const char* at = strstr(text, mk_needles[i]);
        if (at != NULL) {
            char   around[64];
            char   who[200];
            size_t from = (size_t)(at - text);
            size_t k;
            int    known =
                (mk_s->known_leak != NULL && strstr(where, mk_s->known_leak) != NULL);

            from = (from > 30) ? from - 30 : 0;
            strncpy(around, text + from, 60);
            around[60] = '\0';
            for (k = 0; around[k] != '\0'; ++k) {
                if (around[k] == '\n' || around[k] == '\r') {
                    around[k] = ' ';
                }
            }
            mk_context(who, sizeof who);
            printf("%s: %s: %s shows \"%s\" ...%s...\n", known ? "KNOWN LEAK" : "FAIL",
                   who, where, mk_needles[i], around);
            if (!known) {
                ++g_fail;
            }
            return;
        }
    }
}

/* Text a renderer made: the check, after seeing that it all fitted. */
static void mk_text(const char* where, size_t n) {
    ++g_run;
    if (n >= sizeof mk_buf) {
        char who[200];
        mk_context(who, sizeof who);
        ++g_fail;
        printf("FAIL: %s: %s needs %lu bytes, the buffer is %lu\n", who, where,
               (unsigned long)n, (unsigned long)sizeof mk_buf);
        return;
    }
    marker_absent(where, mk_buf);
}

static void mk_error(const char* what, const cf_error* err) {
    char where[160];

    cf_fmt(where, sizeof where, "%s: error message", what);
    marker_absent(where, err->message);
    cf_fmt(where, sizeof where, "%s: error key", what);
    marker_absent(where, err->key);
    cf_fmt(where, sizeof where, "%s: error source", what);
    marker_absent(where, err->source);
}

/* What the log was handed, kept: an event's strings live for the call. */
static size_t mk_log_at;

static void mk_sink(void* user, const cf_log_event* e) {
    size_t room;

    (void)user;
    if (e->size < sizeof *e || mk_log_at + 1 >= sizeof mk_log) {
        return;
    }
    room = sizeof mk_log - mk_log_at;
    mk_log_at += cf_fmt(mk_log + mk_log_at, room, "%s|%s|%s|%s\n", e->event,
                        (e->key != NULL) ? e->key : "",
                        (e->source != NULL) ? e->source : "", e->message);
    if (mk_log_at >= sizeof mk_log) {
        mk_log_at = sizeof mk_log - 1;
    }
}

static void mk_log_check(const char* what) {
    char where[160];

    cf_fmt(where, sizeof where, "%s: log events", what);
    marker_absent(where, mk_log);
    mk_log_at = 0;
    mk_log[0] = '\0';
}

/* ------------------------------------------------------------------ *
 * The configuration of a scenario
 * ------------------------------------------------------------------ */

static const char* mk_secret_key(mk_shape shape) {
    if (shape >= SH_LIST_INT && shape <= SH_LIST_APPEND) {
        return "sec.l";
    }
    if (shape == SH_OLD || shape == SH_OLD_MAXLEN) {
        return "db.pass";
    }
    return "sec.v";
}

static cf_bool mk_is_list(mk_shape shape) {
    return (shape >= SH_LIST_INT && shape <= SH_LIST_APPEND) ? CF_TRUE : CF_FALSE;
}

static const char* const mk_alias[] = {"pw", NULL};

static void mk_define(cf_builder* b, const mk_scn* s, cf_bool lenient) {
    static const char* const permitted[]    = {"alpha", "beta", NULL};
    static const char* const policy_only[]  = {"loser", NULL};
    static const char* const policy_first[] = {"loser", "mem", "ini",    "json",
                                               "env",   "cli", "dotenv", NULL};
    cf_def                   d;

    cf_builder_text(b, "pub.host", "localhost", 0);
    cf_builder_text(b, "pub.url", NULL, 0);
    cf_builder_text(b, "ref.t", NULL, 0);
    cf_builder_text(b, "ref.n", "plain", 0);
    cf_builder_text(b, "sec.other", NULL, CF_SENSITIVE);

    if (s->shape == SH_OLD || s->shape == SH_OLD_MAXLEN) {
        cf_def_init(&d, CF_TYPE_TEXT, "db.password");
        d.flags      = CF_SENSITIVE;
        d.on_invalid = lenient ? CF_INVALID_FALLBACK : CF_INVALID_STRICT;
        cf_builder_define(b, &d);
        cf_def_init(&d, CF_TYPE_TEXT, "db.pass");
        d.flags       = CF_DEPRECATED;
        d.replaced_by = "db.password";
        d.on_invalid  = lenient ? CF_INVALID_FALLBACK : CF_INVALID_STRICT;
        if (s->shape == SH_OLD_MAXLEN) {
            d.max_len = 4;
        }
        cf_builder_define(b, &d);
        return;
    }

    cf_def_init(&d, CF_TYPE_TEXT, mk_secret_key(s->shape));
    d.flags      = CF_SENSITIVE;
    d.on_invalid = lenient ? CF_INVALID_FALLBACK : CF_INVALID_STRICT;
    switch ((int)s->shape) {
    case SH_TEXT_DEFAULT:
        d.has_default = CF_TRUE;
        cf_value_text(&d.default_value, MK_V "-dflt", strlen(MK_V "-dflt"));
        break;
    case SH_INT:
        d.type        = CF_TYPE_INT;
        d.has_range   = CF_TRUE;
        d.imin        = 0;
        d.imax        = 9;
        d.has_default = CF_TRUE;
        cf_value_int(&d.default_value, 1);
        break;
#if CF_WITH_REAL
    case SH_REAL:
        d.type        = CF_TYPE_REAL;
        d.has_range   = CF_TRUE;
        d.rmin        = 0.0;
        d.rmax        = 9.0;
        d.has_default = CF_TRUE;
        cf_value_real(&d.default_value, 1.0);
        break;
#endif
    case SH_BOOL:
        d.type        = CF_TYPE_BOOL;
        d.has_default = CF_TRUE;
        cf_value_bool(&d.default_value, CF_FALSE);
        break;
    case SH_ALLOWED:
        d.allowed     = permitted;
        d.has_default = CF_TRUE;
        cf_value_text(&d.default_value, "alpha", 5);
        break;
    case SH_MAXLEN:
        d.max_len     = 4;
        d.has_default = CF_TRUE;
        cf_value_text(&d.default_value, "ok", 2);
        break;
    case SH_MINLEN:
        d.min_len = 64;
        break;
    case SH_LIST_INT:
        d.type      = CF_TYPE_INT;
        d.has_range = CF_TRUE;
        d.imin      = 0;
        d.imax      = 9;
        d.list_mode = CF_LIST_REPLACE;
        break;
    case SH_LIST_FREE:
        d.list_mode      = CF_LIST_REPLACE;
        d.list_separator = ',';
        break;
    case SH_LIST_ALLOWED:
        d.allowed   = permitted;
        d.list_mode = CF_LIST_REPLACE;
        break;
    case SH_LIST_APPEND:
        d.list_mode = CF_LIST_APPEND;
        break;
    case SH_POLICY:
        d.policy = policy_only;
        break;
    case SH_POLICY_SHADOW:
        d.policy = policy_first;
        break;
    default:
        break;
    }
    if (!mk_is_list(s->shape)) {
        d.aliases = mk_alias;
    }
    cf_builder_define(b, &d);
}

/* The files and arguments a source is made of, alive until create. */
static char  mk_alt[256];
static char  mk_text_buf[512];
static char  mk_env_line[320];
static char* mk_env[2];
static char  mk_a0[] = "prog";
static char  mk_arg[320];
static char  mk_arg2[320];
static char* mk_argv[4];

static cf_pair mk_pairs[16];

#define MK_INI "cf_marker.ini"
#define MK_JSON "cf_marker.json"
#define MK_ENV "cf_marker.env"
#define MK_DB "cf_marker.db"

/* "sec.v" as the environment names it: MK_SEC__V. */
static void mk_env_name(const char* key, char* out, size_t cap) {
    size_t at = 0;

    at += cf_fmt(out + at, cap - at, "MK_");
    for (; *key != '\0' && at + 3 < cap; ++key) {
        if (*key == '.') {
            out[at++] = '_';
            out[at++] = '_';
        } else {
            out[at++] = (*key >= 'a' && *key <= 'z') ? (char)(*key - 'a' + 'A') : *key;
        }
    }
    out[at] = '\0';
}

static cf_bool mk_have(mk_src kind) {
    if (kind == SRC_INI) {
        return cf_has_feature("ini") ? CF_TRUE : CF_FALSE;
    }
    if (kind == SRC_JSON || kind == SRC_JSON_NUM) {
        return cf_has_feature("json") ? CF_TRUE : CF_FALSE;
    }
    if (kind == SRC_DOTENV) {
        return cf_has_feature("dotenv") ? CF_TRUE : CF_FALSE;
    }
    return CF_TRUE;
}

/* The winning value, in the kind of source being run. */
static void mk_primary(cf_builder* b, const char* key, const char* v, mk_src kind) {
    const char* dot = strchr(key, '.');
    char        sec[32];
    char        name[32];
    size_t      n = (dot != NULL) ? (size_t)(dot - key) : 0;

    cf_fmt(sec, sizeof sec, "%S", key, n);
    cf_fmt(name, sizeof name, "%s", (dot != NULL) ? dot + 1 : key);
    switch ((int)kind) {
    case SRC_MEM:
        mk_pairs[0].key   = key;
        mk_pairs[0].value = v;
        cf_builder_memory(b, "mem", CF_RANK_USER_FILE, mk_pairs, 1);
        break;
    case SRC_INI:
        cf_fmt(mk_text_buf, sizeof mk_text_buf, "[%s]\n%s = %s\n", sec, name, v);
        spill(MK_INI, mk_text_buf);
        cf_builder_file(b, "ini", CF_RANK_USER_FILE, MK_INI, NULL, 0);
        break;
    case SRC_JSON:
        cf_fmt(mk_text_buf, sizeof mk_text_buf, "{\"%s\": {\"%s\": \"%s\"}}\n", sec,
               name, v);
        spill(MK_JSON, mk_text_buf);
        cf_builder_file(b, "json", CF_RANK_USER_FILE, MK_JSON, NULL, 0);
        break;
    case SRC_JSON_NUM:
        cf_fmt(mk_text_buf, sizeof mk_text_buf, "{\"%s\": {\"%s\": %s}}\n", sec, name,
               v);
        spill(MK_JSON, mk_text_buf);
        cf_builder_file(b, "json", CF_RANK_USER_FILE, MK_JSON, NULL, 0);
        break;
    case SRC_ENV:
        mk_env_name(key, mk_text_buf, sizeof mk_text_buf);
        cf_fmt(mk_env_line, sizeof mk_env_line, "%s=%s", mk_text_buf, v);
        mk_env[0] = mk_env_line;
        mk_env[1] = NULL;
        cf_builder_env(b, "env", CF_RANK_ENV, "MK_", mk_env);
        break;
    case SRC_CLI:
        cf_fmt(mk_arg, sizeof mk_arg, "--%s=%s", key, v);
        mk_argv[0] = mk_a0;
        mk_argv[1] = mk_arg;
        mk_argv[2] = NULL;
        cf_builder_cli(b, "cli", CF_RANK_CLI, 2, mk_argv);
        break;
    case SRC_CLI_SPACE:
    case SRC_CLI_ALIAS:
        cf_fmt(mk_arg, sizeof mk_arg, "--%s", (kind == SRC_CLI_ALIAS) ? "pw" : key);
        cf_fmt(mk_arg2, sizeof mk_arg2, "%s", v);
        mk_argv[0] = mk_a0;
        mk_argv[1] = mk_arg;
        mk_argv[2] = mk_arg2;
        mk_argv[3] = NULL;
        cf_builder_cli(b, "cli", CF_RANK_CLI, 3, mk_argv);
        break;
    case SRC_DOTENV:
        mk_env_name(key, mk_text_buf, sizeof mk_text_buf);
        cf_fmt(mk_env_line, sizeof mk_env_line, "%s=%s\n", mk_text_buf, v);
        spill(MK_ENV, mk_env_line);
        cf_builder_dotenv(b, "dotenv", CF_RANK_ENV, MK_ENV, "MK_", 0);
        break;
    default:
        break;
    }
}

/* variant 0: the scenario as written. 1: the winning value changed, so
   a diff has something to say. 2: no value from anywhere. 3: as 2, with
   a store the program may write. */
static cf_builder* mk_builder(const mk_scn* s, int variant, cf_bool lenient,
                              cf_bool with_log) {
    cf_builder* b   = cf_build(NULL);
    const char* key = mk_secret_key(s->shape);
    const char* v1  = s->v1;
    size_t      n   = 0;
    cf_bool     is_policy =
        (s->shape == SH_POLICY || s->shape == SH_POLICY_SHADOW) ? CF_TRUE : CF_FALSE;

    if (with_log) {
        cf_builder_log(b, mk_sink, NULL, CF_LOG_DEBUG);
    }
    if (variant >= 2 && is_policy) {
        /* A policy naming a source the build lacks is refused, so the
           empty configuration has the one it names. */
        static cf_pair dummy[1];
        dummy[0].key   = "pub.host";
        dummy[0].value = "h";
        cf_builder_memory(b, "loser", CF_RANK_SYS_FILE, dummy, 1);
    }
    if (variant <= 1) {
        if (v1 != NULL && variant == 1) {
            cf_fmt(mk_alt, sizeof mk_alt, "%sB", v1);
            v1 = mk_alt;
        }
        if (v1 != NULL) {
            mk_primary(b, key, v1, mk_kind);
        }
        if (s->v2 != NULL || is_policy) {
            static cf_pair loser[1];
            loser[0].key   = (s->v2 != NULL) ? key : "pub.host";
            loser[0].value = (s->v2 != NULL) ? s->v2 : "h";
            cf_builder_memory(b, "loser", CF_RANK_SYS_FILE, loser, 1);
        }
        while (s->extra != NULL && s->extra[n].key != NULL) {
            ++n;
        }
        if (n > 0) {
            cf_builder_memory(b, "extra", CF_RANK_USER_FILE, s->extra, n);
        }
    }
#if CF_WITH_SQLITE
    if (variant == 3) {
        cf_builder_store(b, "store", CF_RANK_USER_FILE, MK_DB, CF_OPTIONAL);
        cf_builder_writable(b, "store");
    }
#endif
    mk_define(b, s, lenient);
    return b;
}

/* ------------------------------------------------------------------ *
 * Every text about a configuration
 * ------------------------------------------------------------------ */

static void mk_key_texts(const cf_config* c, const char* key) {
    cf_fmt(mk_where, sizeof mk_where, "%s: explain(%s)", mk_tag, key);
    mk_text(mk_where, cf_explain(c, key, mk_buf, sizeof mk_buf));
    cf_fmt(mk_where, sizeof mk_where, "%s: blind explain_json(%s)", mk_tag, key);
    mk_text(mk_where, cf_explain_json(c, key, CF_REPORT_BLIND, mk_buf, sizeof mk_buf));
}

static void mk_config_texts(const char* tag, const cf_config* c) {
    const cf_keyspace* ks = cf_keys(c);
    const cf_defset*   ds = cf_defs(c);
    size_t             i;
    static char        key[CF_KEY_MAX];
    static char        fp[32];

    cf_fmt(mk_tag, sizeof mk_tag, "%s", tag);
    for (i = 0; i < cf_ks_count(ks); ++i) {
        const cf_entry* e = cf_ks_at(ks, i);
        cf_fmt(key, sizeof key, "%S", e->key, e->keylen);
        mk_key_texts(c, key);
    }
    for (i = 0; i < cf_defset_count(ds); ++i) {
        mk_key_texts(c, cf_defset_at(ds, i)->key);
    }
    cf_fmt(mk_where, sizeof mk_where, "%s: inspect()", tag);
    mk_text(mk_where, cf_inspect(c, mk_buf, sizeof mk_buf));
    if (cf_has_feature("report")) {
        cf_report_info info = CF_REPORT_INFO_INIT;

        info.mode        = CF_REPORT_BLIND;
        info.application = "marker";
        info.profile     = "test";
        cf_fmt(mk_where, sizeof mk_where, "%s: blind report", tag);
        mk_text(mk_where, cf_report(c, &info, mk_buf, sizeof mk_buf));
        cf_fmt(mk_where, sizeof mk_where, "%s: blind report, no info", tag);
        mk_text(mk_where, cf_report(c, NULL, mk_buf, sizeof mk_buf));
        cf_fmt(mk_where, sizeof mk_where, "%s: schema", tag);
        mk_text(mk_where, cf_schema(c, "marker", mk_buf, sizeof mk_buf));
    }
    cf_fingerprint_format(cf_fingerprint(c), fp, sizeof fp);
    cf_fmt(mk_where, sizeof mk_where, "%s: fingerprint", tag);
    marker_absent(mk_where, fp);
}

static void mk_diff_texts(const char* tag, const cf_config* before,
                          const cf_config* after) {
    cf_fmt(mk_where, sizeof mk_where, "diff %s: text", tag);
    mk_text(mk_where, cf_diff_text(before, after, mk_buf, sizeof mk_buf));
    cf_fmt(mk_where, sizeof mk_where, "diff %s: json", tag);
    mk_text(mk_where, cf_diff_json(before, after, mk_buf, sizeof mk_buf));
}

/* The program's own read, to show the scenario put the marker in: a test
   that proves nothing is hiding is not a test of hiding. */
static void mk_control(const cf_config* c) {
    const char* key = (mk_s->ctl != NULL) ? mk_s->ctl : mk_secret_key(mk_s->shape);

    if (mk_s->holds && !mk_lenient) {
        const char* got = cf_get_text(c, key, "");
        check(strstr(got, "MRK7731XQ") != NULL,
              "the control: the program reads the marker back");
    }
}

static cf_bool mk_has_needle(const char* text) {
    size_t i;

    for (i = 0; mk_needles[i] != NULL; ++i) {
        if (strstr(text, mk_needles[i]) != NULL) {
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}

static void mk_build_expect(const char* tag, int expect, cf_config* c,
                            const cf_error* err) {
    char who[200];

    mk_context(who, sizeof who);
    ++g_run;
    if ((c != NULL) != (expect != 0)) {
        ++g_fail;
        printf("FAIL: %s: %s %s, expected it to %s\n", who, tag,
               (c != NULL) ? "built" : "was refused", expect ? "build" : "be refused");
        if (c == NULL) {
            /* A refusal that holds the marker is a leak, reported as one
               by mk_error; here it is only said when it is safe to. */
            printf("  error: %s\n", mk_has_needle(err->message)
                                        ? "(it holds the marker)"
                                        : err->message);
        }
    }
}

/* ------------------------------------------------------------------ *
 * cf_live: the reloads' events and errors
 * ------------------------------------------------------------------ */

static cf_builder* mk_live_build(void* user) {
    (void)user;
    return mk_builder(mk_s, mk_variant, mk_lenient ? CF_TRUE : CF_FALSE, CF_FALSE);
}

static void mk_live(void) {
    static const int order[] = {0, 1, 2};
    static cf_error  err;
    static cf_error  last;
    cf_live_options  o = CF_LIVE_OPTIONS_INIT;
    cf_live*         l;
    size_t           i;

    cf_error_clear(&err);
    cf_error_clear(&last);
    o.build    = mk_live_build;
    o.log      = mk_sink;
    o.log_min  = CF_LOG_DEBUG;
    mk_variant = 2;
    mk_log_at  = 0;
    mk_log[0]  = '\0';
    l          = cf_live_open_with(&o, &err);
    check(l != NULL, "live: opens on the empty configuration");
    if (l == NULL) {
        return;
    }
    for (i = 0; i < sizeof order / sizeof order[0]; ++i) {
        cf_live_result  r;
        static cf_error e;

        cf_error_clear(&e);
        mk_variant = order[i];
        r          = cf_live_reload(l, &e);
        mk_error("live reload", &e);
        if (r == CF_LIVE_SWAPPED) {
            cf_config* c = cf_live_get(l);
            mk_config_texts("live snapshot", c);
            cf_config_free(c);
        }
        if (cf_live_last_error(l, &last)) {
            mk_error("live last error", &last);
        }
        mk_buf[0] = '\0';
        mk_text("live restarts", cf_live_restarts(l, mk_buf, sizeof mk_buf));
    }
    cf_live_close(l);
    mk_log_check("live");
}

/* ------------------------------------------------------------------ *
 * cf_edit: a secret written through a store
 * ------------------------------------------------------------------ */

#if CF_WITH_SQLITE
static void mk_edit(void) {
    static cf_error err;
    cf_config*      c;
    cf_edit*        e;
    cf_bool         done;

    cf_error_clear(&err);
    remove(MK_DB);
    mk_variant = 3;
    c = cf_builder_create(mk_builder(mk_s, 3, (cf_bool)mk_lenient, CF_TRUE), &err);
    if (c == NULL) {
        mk_error("edit: base", &err);
        return;
    }
    e = cf_edit_begin(c, "store", &err);
    if (e == NULL) {
        mk_error("edit: begin", &err);
        cf_config_free(c);
        return;
    }
    cf_edit_set_text(e, mk_secret_key(mk_s->shape), mk_s->v1);
    done = cf_edit_commit(e, "marker test", &err);
    if (!done) {
        mk_error("edit: refused", &err);
    }
    mk_log_check("edit");
    cf_config_free(c);
    if (done) {
        static cf_error again;
        cf_error_clear(&again);
        c = cf_builder_create(mk_builder(mk_s, 3, (cf_bool)mk_lenient, CF_TRUE),
                              &again);
        if (c == NULL) {
            mk_error("edit: rebuilt", &again);
        } else {
            mk_config_texts("edit: rebuilt", c);
            cf_config_free(c);
        }
        mk_log_check("edit rebuild");
    }
    remove(MK_DB);
}
#else
static void mk_edit(void) {
}
#endif

/* ------------------------------------------------------------------ *
 * One scenario, one source kind, strict or with fallback
 * ------------------------------------------------------------------ */

static void mk_builds(void) {
    static cf_error err;
    static cf_error err_b;
    static cf_error err_e;
    cf_config*      a;
    cf_config*      b      = NULL;
    cf_config*      e      = NULL;
    int             expect = mk_lenient ? mk_s->lenient_ok : mk_s->strict_ok;

    cf_error_clear(&err);
    cf_error_clear(&err_b);
    cf_error_clear(&err_e);

    mk_log_at  = 0;
    mk_log[0]  = '\0';
    mk_variant = 0;
    a = cf_builder_create(mk_builder(mk_s, 0, (cf_bool)mk_lenient, CF_TRUE), &err);
    mk_build_expect("the build", expect, a, &err);
    if (a == NULL) {
        mk_error("build", &err);
    } else {
        mk_config_texts("config", a);
        mk_control(a);
    }
    mk_log_check("build");

    mk_variant = 1;
    b = cf_builder_create(mk_builder(mk_s, 1, (cf_bool)mk_lenient, CF_TRUE), &err_b);
    if (b == NULL) {
        mk_error("changed build", &err_b);
    } else {
        mk_config_texts("changed config", b);
    }
    mk_log_check("changed build");
    mk_variant = 2;
    e = cf_builder_create(mk_builder(mk_s, 2, (cf_bool)mk_lenient, CF_TRUE), &err_e);
    check(e != NULL, "the configuration with no value builds");
    if (e == NULL) {
        mk_error("empty build", &err_e);
    }
    mk_log_check("empty build");

    if (a != NULL && b != NULL) {
        mk_diff_texts("config -> changed", a, b);
        mk_diff_texts("changed -> config", b, a);
    }
    if (a != NULL && e != NULL) {
        mk_diff_texts("config -> empty", a, e);
        mk_diff_texts("empty -> config", e, a);
    }
    if (b != NULL && e != NULL) {
        mk_diff_texts("changed -> empty", b, e);
    }
    cf_config_free(a);
    cf_config_free(b);
    cf_config_free(e);
}

static void mk_edit_stage(void) {
    if (mk_kind == SRC_MEM && !mk_is_list(mk_s->shape) && mk_s->v1 != NULL) {
        mk_edit();
    }
}

/* The stages are called through a table so that an optimising compiler
   does not fold them into one frame, which would add their buffers up
   past the stack budget. */
typedef void (*mk_stage_fn)(void);

static const mk_stage_fn mk_stages[] = {mk_builds, mk_live, mk_edit_stage};

static void mk_run(void) {
    size_t i;

    for (i = 0; i < sizeof mk_stages / sizeof mk_stages[0]; ++i) {
        mk_stages[i]();
    }
}

static void mk_scenarios(void) {
    size_t i;

    for (i = 0; i < MK_COUNT; ++i) {
        mk_src first = mk_table[i].src;
        mk_src last  = mk_table[i].src;
        mk_src k;
        int    lenient;

        if (mk_table[i].src == SRC_ALL) {
            first = SRC_MEM;
            last  = SRC_CLI_ALIAS;
        }
        mk_s = &mk_table[i];
        if (mk_s->needs != NULL && !cf_has_feature(mk_s->needs)) {
            continue;
        }
        for (k = first; k <= last; k = (mk_src)(k + 1)) {
            if (!mk_have(k)) {
                continue;
            }
            if ((k == SRC_CLI_SPACE || k == SRC_CLI_ALIAS) &&
                mk_table[i].shape == SH_BOOL) {
                continue; /* a bool flag takes no word after it: the word is an argument
                           */
            }
            if (k == SRC_CLI_ALIAS && (mk_is_list(mk_table[i].shape) ||
                                       mk_secret_key(mk_table[i].shape)[0] == 'd')) {
                continue; /* the alias is on the one secret key, not a list or old name
                           */
            }
            mk_kind = k;
            for (lenient = 0; lenient < 2; ++lenient) {
                mk_lenient = lenient;
                mk_run();
            }
        }
    }
}

int main(void) {
    mk_scenarios();
    remove(MK_INI);
    remove(MK_JSON);
    remove(MK_ENV);
    remove(MK_DB);
    return cf_test_done();
}
