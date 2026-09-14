/*
 * doctor.c - ConfigForge Doctor.
 *
 * Answers "why is this value what it is?" before deployment rather
 * than after an incident, by comparing two provenance reports.
 *
 * The tool owns the report format, not the resolution logic. It never
 * computes a configuration; it reads what a process already resolved.
 * That is the line that keeps it from becoming a reimplementation of
 * every other ecosystem's precedence rules.
 *
 *     doctor dump    <report>
 *     doctor explain <report> <key>
 *     doctor lint    <report>
 *     doctor diff    <a> <b>
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
    cf_bool     defined;
    const char* type;
} rkey;

typedef struct report {
    cf_arena*   arena;
    cf_keyspace doc;
    const char* path;
    const char* mode;
    const char* profile;
    rkey*       keys;
    size_t      nkeys;
} report;

static int g_findings  = 0;
static int g_errors    = 0;
static int g_baselined = 0;

/* ------------------------------------------------------------------ *
 * Reading a report
 *
 * The report is JSON, so it is read with the library's own codec and
 * flattener into a key space. Parsing it by hand would mean a second
 * JSON reader that has to agree with the first.
 * ------------------------------------------------------------------ */

static const char* text_at(const cf_keyspace* ks, const char* fmt, ...) {
    char            key[CF_KEY_MAX];
    va_list         ap;
    const cf_value* v;
    size_t          n;

    va_start(ap, fmt);
    n = cf_vfmt(key, sizeof key, fmt, ap);
    va_end(ap);
    if (n >= sizeof key) {
        return NULL;
    }
    v = cf_ks_get(ks, key, n);
    if (v == NULL) {
        return NULL;
    }
    if (v->type == CF_TYPE_TEXT) {
        return v->u.t.p;
    }
    {
        /* Values in a report may be of any type; the tool prints them,
           so everything is rendered to text once, here. */
        char   tmp[CF_VALUE_TEXT_MAX];
        size_t k = cf_value_to_text(v, tmp, sizeof tmp);
        char*  p = (char*)cf_arena_alloc(cf_ks_arena(ks), k + 1);
        if (p == NULL) {
            return NULL;
        }
        memcpy(p, tmp, k);
        p[k] = '\0';
        return p;
    }
}

/* Reads a boolean-valued field. Written as one lookup rather than a
   null check followed by a second lookup: the two calls are not
   guaranteed to agree, which -fanalyzer pointed out before a caller
   ever did, and one of them was wasted work anyway. */
static cf_bool flag_at(const cf_keyspace* ks, const char* key, size_t klen,
                       const char* field) {
    char            probe[CF_KEY_MAX];
    const cf_value* v;
    size_t n = cf_fmt(probe, sizeof probe, "keys.%S.definition.%s", key, klen, field);

    if (n >= sizeof probe) {
        return CF_FALSE;
    }
    v = cf_ks_get(ks, probe, n);
    return (v != NULL && v->type == CF_TYPE_BOOL && v->u.b) ? CF_TRUE : CF_FALSE;
}

/* Report keys are escaped into the document's own key space, so a
   configuration key "server.port" appears as "keys.server.port.*".
   Recovering the original means stripping the prefix and the field
   suffix, which is what this does. */
static cf_bool load(report* r, const char* path) {
    const cf_codec* json = cf_codec_by_name("json");
    cf_error        err  = CF_ERROR_INIT;
    cf_flatten      fl;
    cf_blob         blob;
    size_t          i;

    r->arena = cf_arena_new(NULL, 1 << 16);
    r->path  = path;
    if (r->arena == NULL || json == NULL) {
        fprintf(stderr, "doctor: cannot start\n");
        return CF_FALSE;
    }
    if (!cf_blob_file(&blob, r->arena, path, &err)) {
        fprintf(stderr, "doctor: %s\n", err.message);
        return CF_FALSE;
    }
    cf_ks_init(&r->doc, r->arena, 1024);
    cf_flatten_init(&fl, &r->doc, 0);
    if (!json->decode(&blob, cf_flatten_sink(&fl), &err)) {
        fprintf(stderr, "doctor: %s\n", err.message);
        return CF_FALSE;
    }
    cf_ks_resolve(&r->doc);

    {
        const cf_value* schema = cf_ks_get(&r->doc, "configforge_report", 18);
        if (schema == NULL || schema->type != CF_TYPE_INT) {
            fprintf(stderr, "doctor: '%s' is not a provenance report\n", path);
            return CF_FALSE;
        }
        if (schema->u.i != CF_REPORT_SCHEMA) {
            /* Refusing beats comparing two schemas that only look
               alike. A wrong answer about configuration is worse than
               no answer. */
            fprintf(stderr,
                    "doctor: '%s' uses report schema %d, this build "
                    "speaks %d\n",
                    path, (int)schema->u.i, CF_REPORT_SCHEMA);
            return CF_FALSE;
        }
    }
    r->mode    = text_at(&r->doc, "mode");
    r->profile = text_at(&r->doc, "profile");

    /* Every "keys.<key>.selected.source" names one configuration key.
       Counted first so the array is allocated at exactly the size it
       needs, which is also the only way to have no arbitrary limit. */
    {
        size_t count = 0;
        for (i = 0; i < cf_ks_count(&r->doc); ++i) {
            const cf_entry* e = cf_ks_at(&r->doc, i);
            if (e->keylen >= 21 && memcmp(e->key, "keys.", 5) == 0 &&
                memcmp(e->key + e->keylen - 16, ".selected.source", 16) == 0) {
                ++count;
            }
        }
        if (count == 0) {
            fprintf(stderr, "doctor: '%s' describes no keys\n", path);
            return CF_FALSE;
        }
        r->keys = (rkey*)cf_arena_calloc(r->arena, count * sizeof(rkey));
        if (r->keys == NULL) {
            fprintf(stderr, "doctor: '%s' is too large to read\n", path);
            return CF_FALSE;
        }
    }

    for (i = 0; i < cf_ks_count(&r->doc); ++i) {
        const cf_entry* e = cf_ks_at(&r->doc, i);
        const char*     k = e->key;
        size_t          n = e->keylen;
        rkey*           rk;
        size_t          j, ncand;

        if (n < 21 || memcmp(k, "keys.", 5) != 0) {
            continue;
        }
        if (memcmp(k + n - 16, ".selected.source", 16) != 0) {
            continue;
        }
        rk = &r->keys[r->nkeys++];
        memset(rk, 0, sizeof *rk);
        rk->key      = k + 5;
        rk->keylen   = n - 5 - 16;
        rk->selected = (e->selected != NULL && e->selected->value.type == CF_TYPE_TEXT)
                           ? e->selected->value.u.t.p
                           : NULL;

        rk->type     = text_at(&r->doc, "keys.%S.definition.type", rk->key, rk->keylen);
        rk->required = flag_at(&r->doc, rk->key, rk->keylen, "required");
        rk->sensitive  = flag_at(&r->doc, rk->key, rk->keylen, "sensitive");
        rk->deprecated = flag_at(&r->doc, rk->key, rk->keylen, "deprecated");
        /* A key with no definition has a null one in the report, which
           is how an undefined key is told from a defined one without a
           type. */
        rk->defined =
            (text_at(&r->doc, "keys.%S.definition.type", rk->key, rk->keylen) != NULL)
                ? CF_TRUE
                : CF_FALSE;

        /* Candidates counted the same way, so a chain of any length
           fits and none is silently dropped. Dropping one would lose
           exactly the evidence the report exists to carry. */
        ncand = 0;
        while (text_at(&r->doc, "keys.%S.candidates.%zu.source", rk->key, rk->keylen,
                       ncand) != NULL) {
            ++ncand;
        }
        if (ncand == 0) {
            continue;
        }
        rk->c = (cand*)cf_arena_calloc(r->arena, ncand * sizeof(cand));
        if (rk->c == NULL) {
            fprintf(stderr, "doctor: '%s' is too large to read\n", path);
            return CF_FALSE;
        }

        for (j = 0; j < ncand; ++j) {
            const char* src = text_at(&r->doc, "keys.%S.candidates.%zu.source", rk->key,
                                      rk->keylen, j);
            if (src == NULL) {
                break;
            }
            rk->c[j].source = src;
            rk->c[j].value  = text_at(&r->doc, "keys.%S.candidates.%zu.value", rk->key,
                                      rk->keylen, j);
            rk->c[j].at =
                text_at(&r->doc, "keys.%S.candidates.%zu.at", rk->key, rk->keylen, j);
            rk->c[j].outcome = text_at(&r->doc, "keys.%S.candidates.%zu.outcome",
                                       rk->key, rk->keylen, j);
            rk->c[j].reason = text_at(&r->doc, "keys.%S.candidates.%zu.reason", rk->key,
                                      rk->keylen, j);
            rk->c[j].digest = text_at(&r->doc, "keys.%S.candidates.%zu.digest", rk->key,
                                      rk->keylen, j);
            ++rk->nc;
        }
    }
    return CF_TRUE;
}

/* A tool that exits immediately could skip this, and every tool that
   skips it teaches LeakSanitizer to be ignored. One arena, one call. */
static void release(report* r) {
    if (r->arena != NULL) {
        cf_arena_free(r->arena);
        r->arena = NULL;
    }
}

static const rkey* find_key(const report* r, const char* key, size_t n) {
    size_t i;
    for (i = 0; i < r->nkeys; ++i) {
        if (r->keys[i].keylen == n && memcmp(r->keys[i].key, key, n) == 0) {
            return &r->keys[i];
        }
    }
    return NULL;
}

static const char* shown(const cand* c) {
    return (c->value != NULL) ? c->value : "<redacted>";
}

/* ------------------------------------------------------------------ *
 * Findings
 * ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ *
 * Baseline
 *
 * The commonest way a lint tool dies is two hundred findings on its
 * first run against a repository that already exists. Nobody fixes two
 * hundred things, so everybody turns the tool off, and the fifteen
 * findings that mattered go with the rest.
 *
 * A baseline records what was already there as accepted, so only new
 * findings fail. A stale entry - one that no longer matches anything -
 * is reported rather than dropped silently, because a rule that stopped
 * firing is either a fix worth noticing or a rule that quietly broke.
 *
 * One finding per line, "CFD001 server.port". Not JSON, on purpose:
 * this file is read in code review, and a line somebody can delete by
 * hand is a baseline somebody will actually shrink.
 * ------------------------------------------------------------------ */

#define MAX_BASELINE 512

typedef struct baseline {
    char    id[MAX_BASELINE][16];
    char    key[MAX_BASELINE][CF_KEY_MAX];
    cf_bool matched[MAX_BASELINE];
    size_t  count;
    cf_bool loaded;
} baseline;

static baseline g_baseline;

static void baseline_load(const char* path) {
    FILE*       f;
    static char line[CF_KEY_MAX + 32];

    g_baseline.loaded = CF_TRUE;
    f                 = fopen(path, "rb");
    if (f == NULL) {
        /* A missing baseline is an empty one: the first run of a tool
           should not need a file to exist before it says anything. */
        return;
    }
    while (fgets(line, (int)sizeof line, f) != NULL &&
           g_baseline.count < MAX_BASELINE) {
        char*  key;
        size_t n = strlen(line);

        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
            line[--n] = '\0';
        }
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        key = strchr(line, ' ');
        if (key == NULL) {
            continue;
        }
        *key++ = '\0';
        while (*key == ' ') {
            ++key;
        }
        cf_fmt(g_baseline.id[g_baseline.count], 16, "%s", line);
        cf_fmt(g_baseline.key[g_baseline.count], CF_KEY_MAX, "%s", key);
        g_baseline.matched[g_baseline.count] = CF_FALSE;
        ++g_baseline.count;
    }
    fclose(f);
}

/* Already accepted? Marks it seen, so what is left unmatched afterwards
   is the stale part of the file. */
static cf_bool baseline_accepts(const char* id, const char* key) {
    size_t i;

    for (i = 0; i < g_baseline.count; ++i) {
        if (strcmp(g_baseline.id[i], id) == 0 && strcmp(g_baseline.key[i], key) == 0) {
            g_baseline.matched[i] = CF_TRUE;
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}

static int baseline_stale(void) {
    size_t i;
    int    stale = 0;

    for (i = 0; i < g_baseline.count; ++i) {
        if (!g_baseline.matched[i]) {
            printf("%-7s %-7s %-28s %s\n", g_baseline.id[i], "stale", g_baseline.key[i],
                   "in the baseline and no longer found: either it was fixed "
                   "or the rule stopped working");
            ++stale;
        }
    }
    return stale;
}

static void finding(const char* id, const char* sev, const char* key, const char* fmt,
                    ...) {
    char    msg[512];
    va_list ap;

    va_start(ap, fmt);
    cf_vfmt(msg, sizeof msg, fmt, ap);
    va_end(ap);

    if (g_baseline.loaded && baseline_accepts(id, key)) {
        /* Counted but not printed. A baselined finding that printed on
           every run would be the noise the baseline exists to remove. */
        ++g_baselined;
        return;
    }
    printf("%-7s %-7s %-28s %s\n", id, sev, key, msg);
    ++g_findings;
    if (strcmp(sev, "error") == 0) {
        ++g_errors;
    }
}

static void key_name(const rkey* k, char* buf, size_t cap) {
    size_t n = (k->keylen + 1 < cap) ? k->keylen : cap - 1;
    memcpy(buf, k->key, n);
    buf[n] = '\0';
}

/* CFD010, dead source. A configured source wins for no key at all, so
   somebody is editing a file that has no effect.
   This was labelled CFD001 for a while, which is a different rule: an
   unreachable *value* is one key set where it can never win, and a dead
   *source* wins nothing anywhere. The implementation was right and the
   label was not.
   Note what this is still not: a single shadowed candidate. Being
   shadowed is precedence working as designed, and reporting it would
   bury the real finding under one line per key. */
static void rule_dead_source(const report* r) {
    const char* names[CF_MAX_SOURCES];
    int         supplied[CF_MAX_SOURCES];
    int         won[CF_MAX_SOURCES];
    const char* first_at[CF_MAX_SOURCES];
    size_t      nsrc = 0;
    size_t      i, j, s;

    memset(supplied, 0, sizeof supplied);
    memset(won, 0, sizeof won);
    memset(first_at, 0, sizeof first_at);

    for (i = 0; i < r->nkeys; ++i) {
        const rkey* k = &r->keys[i];
        for (j = 0; j < k->nc; ++j) {
            const char* nm = k->c[j].source;
            if (nm == NULL) {
                continue;
            }
            for (s = 0; s < nsrc; ++s) {
                if (strcmp(names[s], nm) == 0) {
                    break;
                }
            }
            if (s == nsrc) {
                if (nsrc >= CF_MAX_SOURCES) {
                    continue;
                }
                names[nsrc] = nm;
                ++nsrc;
            }
            ++supplied[s];
            if (k->c[j].at != NULL && first_at[s] == NULL) {
                first_at[s] = k->c[j].at;
            }
            if (k->c[j].outcome == NULL) {
                ++won[s];
            }
        }
    }

    for (s = 0; s < nsrc; ++s) {
        if (supplied[s] == 0 || won[s] > 0) {
            continue;
        }
        /* The default source losing everything is the normal, healthy
           case: it means the configuration is fully specified. */
        if (strcmp(names[s], "default") == 0) {
            continue;
        }
        finding("CFD010", "error", names[s],
                "supplies %d value(s) and wins none; everything it sets is "
                "shadowed%s%s",
                supplied[s], (first_at[s] != NULL) ? ", e.g. " : "",
                (first_at[s] != NULL) ? first_at[s] : "");
    }
}

/* CFD005, undefined key present. A source supplied a key the program
   has no definition for. At Tier 0 that is normal and this rule says
   nothing; it fires only where the report shows definitions, because
   then a key outside them is either a typo or a setting the program
   stopped reading. */
static void rule_undefined(const report* r) {
    size_t i, defined = 0;

    for (i = 0; i < r->nkeys; ++i) {
        if (r->keys[i].defined) {
            ++defined;
        }
    }
    if (defined == 0) {
        return; /* Tier 0: there is no contract to be outside of. */
    }
    for (i = 0; i < r->nkeys; ++i) {
        char name[CF_KEY_MAX];
        if (r->keys[i].defined) {
            continue;
        }
        key_name(&r->keys[i], name, sizeof name);
        finding("CFD005", "warning", name,
                "no definition for this key; either it is a typo or the "
                "program stopped reading it");
    }
}

/* CFD009, a deprecated key still set. Not that it exists - a definition
   may keep one for years - but that something is still supplying it, so
   somebody has work to do before it goes. */
static void rule_deprecated(const report* r) {
    size_t i, j;

    for (i = 0; i < r->nkeys; ++i) {
        const rkey* k = &r->keys[i];
        char        name[CF_KEY_MAX];

        if (!k->deprecated || k->selected == NULL) {
            continue;
        }
        if (strcmp(k->selected, "default") == 0) {
            continue; /* nothing is setting it; only the default is */
        }
        key_name(k, name, sizeof name);
        for (j = 0; j < k->nc; ++j) {
            if (k->c[j].outcome != NULL) {
                continue;
            }
            finding("CFD009", "warning", name,
                    "deprecated and still supplied by %s%s%s", k->c[j].source,
                    (k->c[j].at != NULL) ? " at " : "",
                    (k->c[j].at != NULL) ? k->c[j].at : "");
            break;
        }
    }
}

/* CFD011, present but empty. Empty and absent are different states and
   conflating them is a recurring defect elsewhere; a key set to nothing
   is usually a variable that expanded to nothing rather than a decision
   to set it to nothing. */
static void rule_empty(const report* r) {
    size_t i, j;

    for (i = 0; i < r->nkeys; ++i) {
        const rkey* k = &r->keys[i];
        char        name[CF_KEY_MAX];

        key_name(k, name, sizeof name);
        for (j = 0; j < k->nc; ++j) {
            if (k->c[j].outcome != NULL || k->c[j].value == NULL) {
                continue;
            }
            if (k->c[j].value[0] != '\0') {
                continue;
            }
            finding("CFD011", "warning", name,
                    "set to an empty value by %s%s%s, which is not the same "
                    "as being unset",
                    k->c[j].source, (k->c[j].at != NULL) ? " at " : "",
                    (k->c[j].at != NULL) ? k->c[j].at : "");
        }
    }
}

/* CFD004. A required parameter resolved to nothing. */
static void rule_required(const report* r) {
    size_t i;

    for (i = 0; i < r->nkeys; ++i) {
        const rkey* k = &r->keys[i];
        char        name[CF_KEY_MAX];
        if (!k->required || k->selected != NULL) {
            continue;
        }
        key_name(k, name, sizeof name);
        finding("CFD004", "error", name, "required and has no value");
    }
}

/* CFD006. A candidate was refused by its own contract. */
static void rule_refused(const report* r) {
    size_t i, j;

    for (i = 0; i < r->nkeys; ++i) {
        const rkey* k = &r->keys[i];
        char        name[CF_KEY_MAX];
        key_name(k, name, sizeof name);
        for (j = 0; j < k->nc; ++j) {
            if (k->c[j].outcome != NULL && strcmp(k->c[j].outcome, "rejected") == 0) {
                finding("CFD006", "error", name, "%s refused %s: %s", k->c[j].source,
                        shown(&k->c[j]),
                        (k->c[j].reason != NULL) ? k->c[j].reason : "invalid");
            }
        }
    }
}

/* CFD016. A source outside the key's policy supplied it. The library
   rejects the candidate rather than refusing to start - see D-33 - so
   the loud part is here, where a deployment pipeline can fail on it.
   For a secret this is the finding that matters: the value reached a
   place it was not allowed to be, and the rejection did not un-leak
   it. */
static void rule_policy(const report* r) {
    size_t i, j;

    for (i = 0; i < r->nkeys; ++i) {
        const rkey* k = &r->keys[i];
        char        name[CF_KEY_MAX];
        key_name(k, name, sizeof name);
        for (j = 0; j < k->nc; ++j) {
            if (k->c[j].reason == NULL || strncmp(k->c[j].reason, "policy:", 7) != 0) {
                continue;
            }
            finding("CFD016", "error", name, "%s supplied this from %s, and %s",
                    k->c[j].source,
                    (k->c[j].at != NULL) ? k->c[j].at : "an unnamed place",
                    k->c[j].reason);
        }
    }
}

/* CFD008. Nothing configured it; the built-in default is the answer.
   Benign in development, worth knowing in production. */
static void rule_default_only(const report* r) {
    size_t i;

    for (i = 0; i < r->nkeys; ++i) {
        const rkey* k = &r->keys[i];
        char        name[CF_KEY_MAX];
        if (k->selected == NULL || strcmp(k->selected, "default") != 0) {
            continue;
        }
        if (k->nc > 1) {
            continue;
        }
        key_name(k, name, sizeof name);
        finding("CFD008", "info", name,
                "no source configured this; the built-in default applies");
    }
}

/* CFD015. A lower-precedence source supplied more elements than the
   winning length, so the extras were discarded. Inherited from the
   flat model and worth saying out loud. */
static void rule_truncated(const report* r) {
    size_t i, j;

    for (i = 0; i < r->nkeys; ++i) {
        const rkey* k = &r->keys[i];
        char        name[CF_KEY_MAX];
        key_name(k, name, sizeof name);
        for (j = 0; j < k->nc; ++j) {
            if (k->c[j].outcome != NULL && strcmp(k->c[j].outcome, "truncated") == 0) {
                finding("CFD015", "warning", name,
                        "discarded: beyond the winning array length");
            }
        }
    }
}

/* ------------------------------------------------------------------ *
 * Comparing two reports
 * ------------------------------------------------------------------ */

/* CFD002. The two agree on the value and disagree on where it came
   from. A plain value diff shows nothing here, which is exactly why it
   is dangerous: the day the environment variable is missing, the two
   environments stop behaving the same. */
static void rule_origin_drift(const report* a, const report* b) {
    size_t i;

    for (i = 0; i < a->nkeys; ++i) {
        const rkey* ka = &a->keys[i];
        const rkey* kb = find_key(b, ka->key, ka->keylen);
        char        name[CF_KEY_MAX];

        if (kb == NULL || ka->selected == NULL || kb->selected == NULL) {
            continue;
        }
        if (strcmp(ka->selected, kb->selected) == 0) {
            continue;
        }

        key_name(ka, name, sizeof name);
        finding("CFD002", "warning", name,
                "same key resolved from '%s' here and '%s' there", ka->selected,
                kb->selected);
    }
}

/* CFD003. Overridden in one environment and not the other, so the code
   path exercised in testing is not the one that runs. */
static void rule_shadow_divergence(const report* a, const report* b) {
    size_t i, j;

    for (i = 0; i < a->nkeys; ++i) {
        const rkey* ka = &a->keys[i];
        const rkey* kb = find_key(b, ka->key, ka->keylen);
        char        name[CF_KEY_MAX];
        int         da = 0, db = 0;

        if (kb == NULL) {
            continue;
        }
        for (j = 0; j < ka->nc; ++j) {
            if (ka->c[j].outcome != NULL) {
                ++da;
            }
        }
        for (j = 0; j < kb->nc; ++j) {
            if (kb->c[j].outcome != NULL) {
                ++db;
            }
        }
        if (da == db) {
            continue;
        }
        key_name(ka, name, sizeof name);
        finding("CFD003", "warning", name, "%d source(s) overridden here, %d there", da,
                db);
    }
}

/* CFD012. A key exists on one side and not the other. */
static void rule_parity(const report* a, const report* b) {
    size_t i;

    for (i = 0; i < a->nkeys; ++i) {
        char name[CF_KEY_MAX];
        if (find_key(b, a->keys[i].key, a->keys[i].keylen) != NULL) {
            continue;
        }
        key_name(&a->keys[i], name, sizeof name);
        finding("CFD012", "warning", name, "present in %s, absent in %s", a->path,
                b->path);
    }
}

static void rule_value_diff(const report* a, const report* b) {
    size_t i;

    for (i = 0; i < a->nkeys; ++i) {
        const rkey* ka = &a->keys[i];
        const rkey* kb = find_key(b, ka->key, ka->keylen);
        char        name[CF_KEY_MAX];
        const cand *ca = NULL, *cb = NULL;
        size_t      j;

        if (kb == NULL || ka->selected == NULL || kb->selected == NULL) {
            continue;
        }
        for (j = 0; j < ka->nc; ++j) {
            if (ka->c[j].outcome == NULL) {
                ca = &ka->c[j];
                break;
            }
        }
        for (j = 0; j < kb->nc; ++j) {
            if (kb->c[j].outcome == NULL) {
                cb = &kb->c[j];
                break;
            }
        }
        if (ca == NULL || cb == NULL) {
            continue;
        }

        key_name(ka, name, sizeof name);

        /* A redacted pair is compared by digest, which is how a
           production report can be audited without the secret being in
           it at all. */
        if (ca->value == NULL || cb->value == NULL) {
            if (ca->digest != NULL && cb->digest != NULL &&
                strcmp(ca->digest, cb->digest) != 0) {
                finding("CFD014", "info", name, "sensitive value differs (by digest)");
            }
            continue;
        }
        if (strcmp(ca->value, cb->value) != 0) {
            finding("VALUE", "info", name, "%s here, %s there", ca->value, cb->value);
        }
    }
}

/* ------------------------------------------------------------------ *
 * Commands
 * ------------------------------------------------------------------ */

static void cmd_dump(const report* r) {
    size_t i, j;

    printf("%s  mode=%s profile=%s  %u keys\n\n", r->path,
           (r->mode != NULL) ? r->mode : "?", (r->profile != NULL) ? r->profile : "-",
           (unsigned)r->nkeys);

    for (i = 0; i < r->nkeys; ++i) {
        const rkey* k = &r->keys[i];
        char        name[CF_KEY_MAX];
        key_name(k, name, sizeof name);

        printf("%s", name);
        if (k->type != NULL) {
            printf("  (%s)", k->type);
        }
        if (k->sensitive) {
            printf("  [sensitive]");
        }
        printf("\n");
        for (j = 0; j < k->nc; ++j) {
            printf("  %-12s %-22s %s%s%s\n", k->c[j].source, shown(&k->c[j]),
                   (k->c[j].outcome != NULL) ? k->c[j].outcome : "selected",
                   (k->c[j].at != NULL) ? "   " : "",
                   (k->c[j].at != NULL) ? k->c[j].at : "");
        }
    }
}

static int cmd_explain(const report* r, const char* key) {
    const rkey* k = find_key(r, key, strlen(key));
    size_t      j;

    if (k == NULL) {
        fprintf(stderr, "doctor: '%s' is not in %s\n", key, r->path);
        return 2;
    }
    printf("%s", key);
    if (k->type != NULL) {
        printf("  (%s)", k->type);
    }
    printf("\n");
    for (j = 0; j < k->nc; ++j) {
        printf("  %-12s %-22s %s%s%s%s%s\n", k->c[j].source, shown(&k->c[j]),
               (k->c[j].outcome != NULL) ? k->c[j].outcome : "selected",
               (k->c[j].at != NULL) ? "   " : "",
               (k->c[j].at != NULL) ? k->c[j].at : "",
               (k->c[j].reason != NULL) ? "   " : "",
               (k->c[j].reason != NULL) ? k->c[j].reason : "");
    }
    return 0;
}

static void cmd_lint(const report* r) {
    rule_dead_source(r);
    rule_required(r);
    rule_refused(r);
    rule_policy(r);
    rule_undefined(r);
    rule_deprecated(r);
    rule_empty(r);
    rule_truncated(r);
    rule_default_only(r);
}

static void cmd_diff(const report* a, const report* b) {
    rule_parity(a, b);
    rule_parity(b, a);
    rule_origin_drift(a, b);
    rule_shadow_divergence(a, b);
    rule_value_diff(a, b);
}

static int usage(void) {
    /* Split across several calls: C90 requires support for string
       literals only up to 509 characters, and one long one is the sort
       of limit that is invisible until a toolchain enforces it. */
    fputs("ConfigForge Doctor\n"
          "\n"
          "  doctor dump    <report>\n"
          "  doctor explain <report> <key>\n"
          "  doctor lint    <report> [--baseline <file>]\n"
          "  doctor diff    <report-a> <report-b>\n"
          "\n",
          stderr);
    fputs("Reads provenance reports and never resolves configuration\n"
          "itself: it reads what a process already resolved, which is what\n"
          "keeps it from becoming a second implementation of every\n"
          "ecosystem's precedence rules.\n"
          "\n",
          stderr);
    fputs("A baseline records findings already accepted, one per line as\n"
          "\"CFD001 server.port\", so that only new ones fail. The\n"
          "commonest way a lint tool dies is two hundred findings on its\n"
          "first run: nobody fixes two hundred things, so everybody turns\n"
          "the tool off and the fifteen that mattered go with the rest.\n"
          "\n"
          "Exit codes: 0 clean, 1 findings at error severity, 2 unusable\n"
          "input.\n",
          stderr);
    return 2;
}

int main(int argc, char** argv) {
    report a, b;

    if (argc < 3) {
        return usage();
    }
    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);

    if (strcmp(argv[1], "dump") == 0) {
        if (!load(&a, argv[2])) {
            release(&a);
            return 2;
        }
        cmd_dump(&a);
        release(&a);
        return 0;
    }
    if (strcmp(argv[1], "explain") == 0) {
        int rc;
        if (argc < 4) {
            return usage();
        }
        if (!load(&a, argv[2])) {
            release(&a);
            return 2;
        }
        rc = cmd_explain(&a, argv[3]);
        release(&a);
        return rc;
    }
    if (strcmp(argv[1], "lint") == 0) {
        int stale = 0;

        /* Read before anything runs, so a finding is matched as it is
           produced rather than filtered afterwards - filtering
           afterwards would mean printing and then unprinting. */
        if (argc >= 5 && strcmp(argv[3], "--baseline") == 0) {
            baseline_load(argv[4]);
        }
        if (!load(&a, argv[2])) {
            release(&a);
            return 2;
        }
        cmd_lint(&a);
        release(&a);
        if (g_baseline.loaded) {
            stale = baseline_stale();
        }
        printf("\n%d new finding(s), %d at error severity", g_findings, g_errors);
        if (g_baselined > 0) {
            printf("; %d accepted by the baseline", g_baselined);
        }
        if (stale > 0) {
            printf("; %d baseline entry(s) no longer match", stale);
        }
        printf("\n");
        /* A stale entry does not fail the build. It says the file has
           drifted, and failing on it would make people stop keeping the
           file rather than start pruning it. */
        return (g_errors > 0) ? 1 : 0;
    }
    if (strcmp(argv[1], "diff") == 0) {
        if (argc < 4) {
            return usage();
        }
        if (!load(&a, argv[2]) || !load(&b, argv[3])) {
            release(&a);
            release(&b);
            return 2;
        }
        cmd_diff(&a, &b);
        release(&a);
        release(&b);
        printf("\n%d finding(s), %d at error severity\n", g_findings, g_errors);
        return (g_errors > 0) ? 1 : 0;
    }
    return usage();
}
