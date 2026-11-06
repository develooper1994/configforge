/*
 * engine.c - Doctor's rules: reading reports, the findings they make,
 * the baseline and the severities. The library behind cf_doctor.h, and
 * the whole of the `doctor` program but its command line.
 *
 * Everything a run knows lives in a cf_doctor, which each report it
 * reads points at, so two runs - two threads, or a program linting
 * itself while a test lints something else - never share anything.
 * Nothing here prints: what went wrong is returned in a cf_error, and
 * what was found is recorded for the caller to read.
 */

#include "engine.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* The key's "read": a boolean only where the program tracked reads. */
static int read_at(const cf_keyspace* ks, const char* key, size_t klen) {
    char            probe[CF_KEY_MAX];
    const cf_value* v;
    size_t          n = cf_fmt(probe, sizeof probe, "keys.%S.read", key, klen);

    if (n >= sizeof probe) {
        return -1;
    }
    v = cf_ks_get(ks, probe, n);
    if (v == NULL || v->type != CF_TYPE_BOOL) {
        return -1;
    }
    return v->u.b ? 1 : 0;
}

/* The length of the configuration key a report entry names, or 0 when it
   names none. A key is written as "selected.source" when a candidate won
   it and as a "selected" that is null when none did: an optional key
   with no default, whose only value came from a source its policy
   refuses. Looking for the first alone left that key out of every rule,
   which is how a secret passed on the command line to one was never
   found. */
static size_t entry_key_length(const cf_entry* e) {
    static const char won[]  = ".selected.source";
    static const char none[] = ".selected";
    size_t            n      = e->keylen;

    if (n < 5 || memcmp(e->key, "keys.", 5) != 0) {
        return 0;
    }
    n -= 5;
    if (n > sizeof won - 1 &&
        memcmp(e->key + 5 + n - (sizeof won - 1), won, sizeof won - 1) == 0) {
        return n - (sizeof won - 1);
    }
    if (n > sizeof none - 1 &&
        memcmp(e->key + 5 + n - (sizeof none - 1), none, sizeof none - 1) == 0 &&
        e->selected != NULL && e->selected->value.type == CF_TYPE_NULL) {
        return n - (sizeof none - 1);
    }
    return 0;
}

/* A report from before candidates said which one is a default marks
   none, and there the source does: a source of the kind "default", or,
   absent from the table of sources, one named so. A caller's own source
   at the defaults' rank is not told apart this way, which is why the
   mark exists. */
static void defaults_by_source(report* r) {
    size_t i, j, s;

    for (i = 0; i < r->nkeys; ++i) {
        for (j = 0; j < r->keys[i].nc; ++j) {
            cand*       c    = &r->keys[i].c[j];
            const char* kind = c->source;

            for (s = 0; s < r->nsrc; ++s) {
                if (r->src[s].name != NULL && strcmp(r->src[s].name, c->source) == 0) {
                    kind = (r->src[s].kind != NULL) ? r->src[s].kind : "";
                    break;
                }
            }
            c->is_default =
                (!c->synthesized && strcmp(kind, "default") == 0) ? CF_TRUE : CF_FALSE;
        }
    }
}

/* Report keys are escaped into the document's own key space, so a
   configuration key "server.port" appears as "keys.server.port.*".
   Recovering the original means stripping the prefix and the field
   suffix, which is what this does. */
static int key_order(const void* a, const void* b) {
    const rkey* x = ((const rkey_ref*)a)->k;
    const rkey* y = ((const rkey_ref*)b)->k;
    return cf_key_cmp(x->key, x->keylen, y->key, y->keylen);
}

/* The keys by name (report.byname); without room, none, and the scan. */
static void index_keys(report* r) {
    size_t i;
    r->byname = NULL;
    if (r->nkeys < 2 || r->nkeys > (size_t)-1 / sizeof *r->byname) {
        return;
    }
    r->byname = (rkey_ref*)cf_arena_alloc(r->arena, r->nkeys * sizeof *r->byname);
    if (r->byname == NULL) {
        return;
    }
    for (i = 0; i < r->nkeys; ++i) {
        r->byname[i].k = &r->keys[i];
    }
    qsort(r->byname, r->nkeys, sizeof *r->byname, key_order);
}

static cf_bool parse(report* r, const cf_blob* blob, cf_error* err) {
    const cf_codec* json = cf_codec_by_name("json");
    const char*     path = r->path;
    cf_flatten      fl;
    size_t          i;
    cf_bool         marked = CF_FALSE;

    if (json == NULL) {
        return cf_error_set(err, CF_E_CAPABILITY, NULL, path,
                            "reading a report needs CF_WITH_JSON");
    }
    cf_ks_init(&r->doc, r->arena, 1024);
    cf_flatten_init(&fl, &r->doc, 0);
    if (!json->decode(blob, cf_flatten_sink(&fl), err)) {
        return CF_FALSE;
    }
    cf_ks_resolve(&r->doc);

    {
        const cf_value* schema = cf_ks_get(&r->doc, "configforge_report", 18);
        if (schema == NULL || schema->type != CF_TYPE_INT) {
            return cf_error_set(err, CF_E_SOURCE, NULL, path,
                                "'%s' is not a provenance report", path);
        }
        if (schema->u.i != CF_REPORT_SCHEMA) {
            /* Refusing beats comparing two schemas that only look
               alike. A wrong answer about configuration is worse than
               no answer. */
            return cf_error_set(err, CF_E_SOURCE, NULL, path,
                                "'%s' uses report schema %d, this build speaks %d",
                                path, (int)schema->u.i, CF_REPORT_SCHEMA);
        }
    }
    r->mode        = text_at(&r->doc, "mode");
    r->profile     = text_at(&r->doc, "profile");
    r->fingerprint = text_at(&r->doc, "fingerprint");

    /* The sources, so a rule can ask what kind a source is and where it
       ranks. Counted first, like everything else here, so there is no
       limit to exceed. */
    {
        size_t count = 0;
        while (text_at(&r->doc, "sources.%zu.name", count) != NULL) {
            ++count;
        }
        if (count > 0) {
            r->src = (rsrc*)cf_arena_calloc(r->arena, count * sizeof(rsrc));
            if (r->src == NULL) {
                return cf_error_set(err, CF_E_SOURCE, NULL, path,
                                    "'%s' is too large to read", path);
            }
        }
        for (i = 0; i < count; ++i) {
            const char* rank     = text_at(&r->doc, "sources.%zu.rank", i);
            r->src[i].name       = text_at(&r->doc, "sources.%zu.name", i);
            r->src[i].kind       = text_at(&r->doc, "sources.%zu.kind", i);
            r->src[i].format     = text_at(&r->doc, "sources.%zu.format", i);
            r->src[i].format_how = text_at(&r->doc, "sources.%zu.format_how", i);
            r->src[i].rank       = 0;
            if (rank != NULL) {
                cf_text_to_i64(rank, strlen(rank), &r->src[i].rank, NULL);
            }
        }
        r->nsrc = count;
    }

    /* Every entry that entry_key_length() finds names one configuration
       key. Counted first so the array is allocated at exactly the size
       it needs, which is also the only way to have no arbitrary limit. */
    {
        size_t count = 0;
        for (i = 0; i < cf_ks_count(&r->doc); ++i) {
            if (entry_key_length(cf_ks_at(&r->doc, i)) > 0) {
                ++count;
            }
        }
        if (count == 0) {
            return cf_error_set(err, CF_E_SOURCE, NULL, path, "'%s' describes no keys",
                                path);
        }
        r->keys = (rkey*)cf_arena_calloc(r->arena, count * sizeof(rkey));
        if (r->keys == NULL) {
            return cf_error_set(err, CF_E_SOURCE, NULL, path,
                                "'%s' is too large to read", path);
        }
    }

    for (i = 0; i < cf_ks_count(&r->doc); ++i) {
        const cf_entry* e  = cf_ks_at(&r->doc, i);
        size_t          kl = entry_key_length(e);
        rkey*           rk;
        size_t          j, ncand;

        if (kl == 0) {
            continue;
        }
        rk = &r->keys[r->nkeys++];
        memset(rk, 0, sizeof *rk);
        rk->key      = e->key + 5;
        rk->keylen   = kl;
        rk->selected = (e->selected != NULL && e->selected->value.type == CF_TYPE_TEXT)
                           ? e->selected->value.u.t.p
                           : NULL;

        rk->type     = text_at(&r->doc, "keys.%S.definition.type", rk->key, rk->keylen);
        rk->required = flag_at(&r->doc, rk->key, rk->keylen, "required");
        rk->sensitive  = flag_at(&r->doc, rk->key, rk->keylen, "sensitive");
        rk->deprecated = flag_at(&r->doc, rk->key, rk->keylen, "deprecated");
        rk->replaced_by =
            text_at(&r->doc, "keys.%S.definition.replaced_by", rk->key, rk->keylen);
        rk->read = read_at(&r->doc, rk->key, rk->keylen);
        /* A key with no definition has a null one in the report, which
           is how an undefined key is told from a defined one without a
           type. */
        rk->defined =
            (text_at(&r->doc, "keys.%S.definition.type", rk->key, rk->keylen) != NULL)
                ? CF_TRUE
                : CF_FALSE;

        {
            size_t np = 0;
            while (text_at(&r->doc, "keys.%S.definition.policy.%zu", rk->key,
                           rk->keylen, np) != NULL) {
                ++np;
            }
            if (np > 0) {
                rk->policy =
                    (const char**)cf_arena_calloc(r->arena, (np + 1) * sizeof(char*));
                if (rk->policy == NULL) {
                    return cf_error_set(err, CF_E_SOURCE, NULL, path,
                                        "'%s' is too large to read", path);
                }
                for (j = 0; j < np; ++j) {
                    rk->policy[j] = text_at(&r->doc, "keys.%S.definition.policy.%zu",
                                            rk->key, rk->keylen, j);
                }
            }
        }

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
            return cf_error_set(err, CF_E_SOURCE, NULL, path,
                                "'%s' is too large to read", path);
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
            {
                const char* wr =
                    text_at(&r->doc, "keys.%S.candidates.%zu.world_readable", rk->key,
                            rk->keylen, j);
                rk->c[j].world_readable =
                    (wr != NULL && strcmp(wr, "true") == 0) ? CF_TRUE : CF_FALSE;
                wr = text_at(&r->doc, "keys.%S.candidates.%zu.synthesized", rk->key,
                             rk->keylen, j);
                rk->c[j].synthesized =
                    (wr != NULL && strcmp(wr, "true") == 0) ? CF_TRUE : CF_FALSE;
                wr = text_at(&r->doc, "keys.%S.candidates.%zu.declared_default",
                             rk->key, rk->keylen, j);
                rk->c[j].is_default =
                    (wr != NULL && strcmp(wr, "true") == 0) ? CF_TRUE : CF_FALSE;
                if (rk->c[j].is_default) {
                    marked = CF_TRUE;
                }
            }
            ++rk->nc;
        }
    }
    if (!marked) {
        defaults_by_source(r);
    }
    index_keys(r);
    return CF_TRUE;
}

/* A tool that exits immediately could skip this, and every tool that
   skips it teaches LeakSanitizer to be ignored. One arena, one call. */

cf_bool doctor_load(report* r, cf_doctor* D, const char* path, cf_error* err) {
    cf_blob blob;

    memset(r, 0, sizeof *r);
    r->D     = D;
    r->path  = path;
    r->arena = cf_arena_new(NULL, 1 << 16);
    if (r->arena == NULL) {
        return cf_error_set(err, CF_E_MEMORY, NULL, path, "no room to read '%s'", path);
    }
    if (!cf_blob_file(&blob, r->arena, path, err)) {
        return CF_FALSE;
    }
    return parse(r, &blob, err);
}

cf_bool doctor_load_text(report* r, cf_doctor* D, const char* text, size_t n,
                         const char* name, cf_error* err) {
    cf_blob blob;

    memset(r, 0, sizeof *r);
    r->D     = D;
    r->path  = (name != NULL) ? name : "<report>";
    r->arena = cf_arena_new(NULL, 1 << 16);
    if (r->arena == NULL) {
        return cf_error_set(err, CF_E_MEMORY, NULL, r->path, "no room to read '%s'",
                            r->path);
    }
    blob.p    = text;
    blob.n    = n;
    blob.name = r->path;
    return parse(r, &blob, err);
}

void doctor_release(report* r) {
    if (r->arena != NULL) {
        cf_arena_free(r->arena);
        r->arena = NULL;
    }
}

const rkey* doctor_find_key(const report* r, const char* key, size_t n) {
    size_t i;
    if (r->byname != NULL) {
        size_t lo = 0, hi = r->nkeys;
        while (lo < hi) {
            size_t      mid = lo + (hi - lo) / 2;
            const rkey* k   = r->byname[mid].k;
            int         c   = cf_key_cmp(k->key, k->keylen, key, n);
            if (c == 0) {
                return k;
            }
            if (c < 0) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        return NULL;
    }
    for (i = 0; i < r->nkeys; ++i) {
        if (r->keys[i].keylen == n && memcmp(r->keys[i].key, key, n) == 0) {
            return &r->keys[i];
        }
    }
    return NULL;
}

const char* doctor_shown(const cand* c) {
    return (c->value != NULL) ? c->value : "<redacted>";
}

const rsrc* doctor_find_source(const report* r, const char* name) {
    size_t i;
    if (name == NULL) {
        return NULL;
    }
    for (i = 0; i < r->nsrc; ++i) {
        if (r->src[i].name != NULL && strcmp(r->src[i].name, name) == 0) {
            return &r->src[i];
        }
    }
    return NULL;
}

/* Where inside its origin a candidate was set: "app.ini:14:3" is
   app.ini at line 14, "argv[3]" is the command line at position 3. The
   origin's length is returned through `len`. False for an origin with
   no position in it, such as an environment variable, which can occur
   only once. Read from the right, so "C:\app.ini:14:3" keeps its
   drive letter. */
cf_bool doctor_place_of(const char* at, size_t* len, long* pos) {
    size_t n, i, colon = 0, seen = 0;

    if (at == NULL) {
        return CF_FALSE;
    }
    n = strlen(at);
    if (n > 6 && memcmp(at, "argv[", 5) == 0 && at[n - 1] == ']') {
        *len = 4;
        *pos = 0;
        for (i = 5; i + 1 < n; ++i) {
            if (!cf_is_digit(at[i])) {
                return CF_FALSE;
            }
            *pos = *pos * 10 + (at[i] - '0');
        }
        return CF_TRUE;
    }
    /* Up to two numeric fields at the end, line and then column. The
       line is the leftmost of them. */
    i = n;
    while (seen < 2) {
        size_t end = i;
        while (i > 0 && cf_is_digit(at[i - 1])) {
            --i;
        }
        if (i == end || i == 0 || at[i - 1] != ':') {
            break;
        }
        --i;
        colon = i;
        ++seen;
    }
    if (seen == 0) {
        return CF_FALSE;
    }
    *len = colon;
    *pos = 0;
    for (i = colon + 1; i < n && cf_is_digit(at[i]); ++i) {
        *pos = *pos * 10 + (at[i] - '0');
    }
    return CF_TRUE;
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

static void baseline_load(cf_doctor* D, const char* path) {
    FILE* f;
    char  line[CF_KEY_MAX + 32];

    D->base.loaded = CF_TRUE;
    f              = fopen(path, "rb");
    if (f == NULL) {
        /* A missing baseline is an empty one: the first run of a tool
           should not need a file to exist before it says anything. */
        return;
    }
    while (fgets(line, (int)sizeof line, f) != NULL && D->base.count < MAX_BASELINE) {
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
        cf_fmt(D->base.id[D->base.count], 16, "%s", line);
        cf_fmt(D->base.key[D->base.count], CF_KEY_MAX, "%s", key);
        D->base.matched[D->base.count] = CF_FALSE;
        ++D->base.count;
    }
    fclose(f);
}

/* Already accepted? Marks it seen, so what is left unmatched afterwards
   is the stale part of the file. */
static cf_bool baseline_accepts(cf_doctor* D, const char* id, const char* key) {
    size_t i;

    for (i = 0; i < D->base.count; ++i) {
        if (strcmp(D->base.id[i], id) == 0 && strcmp(D->base.key[i], key) == 0) {
            D->base.matched[i] = CF_TRUE;
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}

/* ------------------------------------------------------------------ *
 * Severity
 *
 * Every rule has a default, and a project can disagree with it. The
 * disagreement is written down in a file read in code review, one line
 * per rule, in the same spirit as the baseline:
 *
 *     CFD010 warning              this rule, everywhere
 *     CFD008 error production     this rule, in reports with this profile
 *     CFD011 off                  not this rule at all
 *
 * A profile line wins over a plain one, a plain one over the built-in
 * profile defaults, and those over the rule's own default. The built-in
 * profile defaults are the rules whose catalogue entry names a profile:
 * CFD008 is "default-only in production", benign anywhere else.
 * ------------------------------------------------------------------ */

static const severity_line g_sev_builtin[] = {{"CFD008", "warning", "production"}};

cf_bool doctor_level_known(const char* l) {
    return (strcmp(l, "error") == 0 || strcmp(l, "warning") == 0 ||
            strcmp(l, "info") == 0 || strcmp(l, "off") == 0)
               ? CF_TRUE
               : CF_FALSE;
}

/* False, having said why, when the file cannot be used: a severity file
   that is half understood is worse than none, because the half that was
   not would be silently at its default. */
static cf_bool severity_load(cf_doctor* D, const char* path, cf_error* err) {
    FILE* f = fopen(path, "rb");
    char  line[256];
    int   n = 0;

    if (f == NULL) {
        return cf_error_set(err, CF_E_SOURCE, NULL, path,
                            "cannot read severity file '%s'", path);
    }
    while (fgets(line, (int)sizeof line, f) != NULL) {
        char        id[16], level[16], profile[64];
        const char* p = line;
        int         field;

        ++n;
        id[0] = level[0] = profile[0] = '\0';
        for (field = 0; field < 3; ++field) {
            char*  dst = (field == 0) ? id : (field == 1) ? level : profile;
            size_t cap = (field == 0)   ? sizeof id
                         : (field == 1) ? sizeof level
                                        : sizeof profile;
            size_t k   = 0;
            CF_ANALYZER_FILLED; /* line is fgets's: filled and terminated */
            while (*p == ' ' || *p == '\t') {
                ++p;
            }
            while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
                if (k + 1 < cap) {
                    dst[k++] = *p;
                }
                ++p;
            }
            dst[k] = '\0';
        }
        if (id[0] == '\0' || id[0] == '#') {
            continue;
        }
        if (!doctor_level_known(level)) {
            fclose(f);
            return cf_error_set(
                err, CF_E_SOURCE, NULL, path,
                "%s:%d: '%s' is not a severity; use error, warning, info "
                "or off",
                path, n, level);
        }
        if (D->nsev >= MAX_SEVERITY) {
            fclose(f);
            return cf_error_set(err, CF_E_SOURCE, NULL, path, "%s: more than %d lines",
                                path, MAX_SEVERITY);
        }
        memcpy(D->sev[D->nsev].id, id, sizeof id);
        memcpy(D->sev[D->nsev].level, level, sizeof level);
        memcpy(D->sev[D->nsev].profile, profile, sizeof profile);
        ++D->nsev;
    }
    fclose(f);
    return CF_TRUE;
}

static const char* severity_of(const cf_doctor* D, const char* id, const char* dflt) {
    size_t i;
    /* Most specific first: this project, this profile. */
    for (i = 0; i < D->nsev; ++i) {
        if (D->sev[i].profile[0] != '\0' && D->profile != NULL &&
            strcmp(D->sev[i].id, id) == 0 &&
            strcmp(D->sev[i].profile, D->profile) == 0) {
            return D->sev[i].level;
        }
    }
    for (i = 0; i < D->nsev; ++i) {
        if (D->sev[i].profile[0] == '\0' && strcmp(D->sev[i].id, id) == 0) {
            return D->sev[i].level;
        }
    }
    for (i = 0; i < sizeof g_sev_builtin / sizeof g_sev_builtin[0]; ++i) {
        if (D->profile != NULL && strcmp(g_sev_builtin[i].id, id) == 0 &&
            strcmp(g_sev_builtin[i].profile, D->profile) == 0) {
            return g_sev_builtin[i].level;
        }
    }
    return dflt;
}

int doctor_level_rank(const char* l) {
    if (strcmp(l, "error") == 0) {
        return 3;
    }
    if (strcmp(l, "warning") == 0) {
        return 2;
    }
    if (strcmp(l, "info") == 0) {
        return 1;
    }
    return 0; /* "never", as a threshold: nothing reaches it */
}

/* ------------------------------------------------------------------ *
 * Findings
 *
 * Collected rather than printed as they are found, because SARIF is one
 * JSON document and has to be written whole. Each carries where it was
 * set when the report knows, which is what lets a forge put it on the
 * line rather than on the pull request.
 * ------------------------------------------------------------------ */

static cf_bool found_push(cf_doctor* D) {
    const cf_allocator* a = cf_default_allocator();
    if (D->nfound < D->capfound) {
        return CF_TRUE;
    }
    {
        size_t    ncap = (D->capfound == 0) ? 32 : D->capfound * 2;
        dfinding* nv   = (dfinding*)a->alloc(a->ctx, ncap * sizeof(dfinding));
        if (nv == NULL) {
            return CF_FALSE;
        }
        if (D->nfound > 0) {
            memcpy(nv, D->found, D->nfound * sizeof(dfinding));
        }
        if (D->found != NULL) {
            a->release(a->ctx, D->found);
        }
        D->found    = nv;
        D->capfound = ncap;
    }
    return CF_TRUE;
}

/* Room for `extra` more findings, made before any is recorded, by a
   caller that counted them first: those findings cannot then be lost
   to an allocation that fails half way through. */
static cf_bool found_reserve(cf_doctor* D, size_t extra) {
    const cf_allocator* a = cf_default_allocator();
    size_t              need;
    dfinding*           nv;

    if (extra > (size_t)-1 / sizeof(dfinding) - D->nfound) {
        return CF_FALSE;
    }
    need = D->nfound + extra;
    if (need <= D->capfound) {
        return CF_TRUE;
    }
    nv = (dfinding*)a->alloc(a->ctx, need * sizeof(dfinding));
    if (nv == NULL) {
        return CF_FALSE;
    }
    if (D->nfound > 0) {
        memcpy(nv, D->found, D->nfound * sizeof(dfinding));
    }
    if (D->found != NULL) {
        a->release(a->ctx, D->found);
    }
    D->found    = nv;
    D->capfound = need;
    return CF_TRUE;
}

static void found_release(cf_doctor* D) {
    const cf_allocator* a = cf_default_allocator();
    if (D->found != NULL) {
        a->release(a->ctx, D->found);
    }
    D->found  = NULL;
    D->nfound = D->capfound = 0;
}

/* ------------------------------------------------------------------ *
 * --only-changed
 *
 * What a change touched: a key whose candidates - each one's source,
 * outcome, value and digest, in order - are not the base's, or that only
 * one side has. Where a value was set is not compared: moving a line
 * changes no configuration. A finding about a source rather than a key
 * is kept when a touched key came from that source, on either side.
 * ------------------------------------------------------------------ */

static cf_bool same_text(const char* a, const char* b) {
    if (a == NULL || b == NULL) {
        return (a == b) ? CF_TRUE : CF_FALSE;
    }
    return (strcmp(a, b) == 0) ? CF_TRUE : CF_FALSE;
}

static cf_bool same_candidates(const rkey* a, const rkey* b) {
    size_t i;
    if (a->nc != b->nc) {
        return CF_FALSE;
    }
    for (i = 0; i < a->nc; ++i) {
        if (!same_text(a->c[i].source, b->c[i].source) ||
            !same_text(a->c[i].outcome, b->c[i].outcome) ||
            !same_text(a->c[i].value, b->c[i].value) ||
            !same_text(a->c[i].digest, b->c[i].digest)) {
            return CF_FALSE;
        }
    }
    return CF_TRUE;
}

static void source_note(cf_doctor* D, const char* name) {
    size_t i;
    if (name == NULL) {
        return;
    }
    for (i = 0; i < D->nsources; ++i) {
        if (strcmp(D->sources[i], name) == 0) {
            return;
        }
    }
    D->sources[D->nsources++] = name;
}

/* Marks what `r` touched against D->only_base; false without memory, and
   then nothing is left out - a filter that could not be built does not
   silently drop findings. */
static cf_bool only_prepare(cf_doctor* D, const report* r) {
    const cf_allocator* a   = cf_default_allocator();
    size_t              cap = 0, i, j;

    for (i = 0; i < r->nkeys; ++i) {
        cap += r->keys[i].nc;
    }
    for (i = 0; i < D->only_base->nkeys; ++i) {
        cap += D->only_base->keys[i].nc;
    }
    D->touched = (cf_bool*)a->alloc(a->ctx, (r->nkeys + 1) * sizeof *D->touched);
    D->sources = (const char**)a->alloc(a->ctx, (cap + 1) * sizeof *D->sources);
    if (D->touched == NULL || D->sources == NULL) {
        if (D->touched != NULL) {
            a->release(a->ctx, D->touched);
        }
        if (D->sources != NULL) {
            a->release(a->ctx, (void*)D->sources);
        }
        D->touched = NULL;
        D->sources = NULL;
        return CF_FALSE;
    }
    D->nsources = 0;
    for (i = 0; i < r->nkeys; ++i) {
        const rkey* k    = &r->keys[i];
        const rkey* base = doctor_find_key(D->only_base, k->key, k->keylen);
        D->touched[i] =
            (base == NULL || !same_candidates(base, k)) ? CF_TRUE : CF_FALSE;
        for (j = 0; D->touched[i] && j < k->nc; ++j) {
            source_note(D, k->c[j].source);
        }
        for (j = 0; D->touched[i] && base != NULL && j < base->nc; ++j) {
            source_note(D, base->c[j].source);
        }
    }
    /* a key the change removed leaves its sources touched too */
    for (i = 0; i < D->only_base->nkeys; ++i) {
        const rkey* k = &D->only_base->keys[i];
        if (doctor_find_key(r, k->key, k->keylen) == NULL) {
            for (j = 0; j < k->nc; ++j) {
                source_note(D, k->c[j].source);
            }
        }
    }
    D->linting = r;
    return CF_TRUE;
}

static void only_done(cf_doctor* D) {
    const cf_allocator* a = cf_default_allocator();
    if (D->touched != NULL) {
        a->release(a->ctx, D->touched);
    }
    if (D->sources != NULL) {
        a->release(a->ctx, (void*)D->sources);
    }
    D->touched  = NULL;
    D->sources  = NULL;
    D->nsources = 0;
    D->linting  = NULL;
}

/* Whether a finding about `key` - a key of the report, or a source's
   name - is within what the change touched. */
static cf_bool only_touched(const cf_doctor* D, const char* key) {
    const rkey* k = doctor_find_key(D->linting, key, strlen(key));
    size_t      i;
    if (k != NULL) {
        return D->touched[k - D->linting->keys];
    }
    for (i = 0; i < D->nsources; ++i) {
        if (strcmp(D->sources[i], key) == 0) {
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}

static void vfinding(cf_doctor* D, const char* id, const char* sev, const char* key,
                     const char* at, const char* fmt, va_list ap) {
    const char* level = severity_of(D, id, sev);
    dfinding*   f;

    if (strcmp(level, "off") == 0) {
        return; /* a project said so, in a file somebody reviewed */
    }
    if (D->touched != NULL && !only_touched(D, key)) {
        ++D->unchanged; /* --only-changed: a key the change left alone */
        return;
    }
    if (D->base.loaded && baseline_accepts(D, id, key)) {
        /* Counted but not printed. A baselined finding that printed on
           every run would be the noise the baseline exists to remove. */
        ++D->baselined;
        return;
    }
    if (!found_push(D)) {
        return; /* no room to record it: the count stays honest about what was */
    }
    f = &D->found[D->nfound++];
    cf_fmt(f->id, sizeof f->id, "%s", id);
    cf_fmt(f->level, sizeof f->level, "%s", level);
    cf_fmt(f->key, sizeof f->key, "%s", key);
    cf_fmt(f->at, sizeof f->at, "%s", (at != NULL) ? at : "");
    cf_vfmt(f->msg, sizeof f->msg, fmt, ap);
    ++D->findings;
    if (strcmp(level, "error") == 0) {
        ++D->errors;
    } else if (strcmp(level, "warning") == 0) {
        ++D->warnings;
    } else {
        ++D->infos;
    }
}

static void finding(cf_doctor* D, const char* id, const char* sev, const char* key,
                    const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfinding(D, id, sev, key, NULL, fmt, ap);
    va_end(ap);
}

/* The same, knowing where the value was set: "app.ini:14:3", "argv[2]". */
static void finding_at(cf_doctor* D, const char* id, const char* sev, const char* key,
                       const char* at, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfinding(D, id, sev, key, at, fmt, ap);
    va_end(ap);
}

/* ------------------------------------------------------------------ *
 * SARIF 2.1.0
 *
 * The format most forges read natively, which puts a finding on the
 * line of the pull request that caused it without an integration per
 * forge. Doctor.md asked for it from the start for exactly that reason.
 * ------------------------------------------------------------------ */

static const struct {
    const char* id;
    const char* text;
} g_rules[] = {
    {"CFD001",
     "Unreachable value: set where a later line of the same source always wins"},
    {"CFD002", "Origin drift: same value, different source kind"},
    {"CFD003", "Shadow divergence: overridden in one environment, not the other"},
    {"CFD004", "Missing required parameter"},
    {"CFD005", "Undefined key present"},
    {"CFD006", "Contract violation"},
    {"CFD007", "Sensitive value from an unsafe source"},
    {"CFD008", "Default-only in production"},
    {"CFD009", "Deprecated key still set"},
    {"CFD010", "Dead source: wins for no key at all"},
    {"CFD011", "Present but empty"},
    {"CFD012", "Parity gap between environments"},
    {"CFD013", "Precedence inversion in a policy"},
    {"CFD014", "Sensitive value differs"},
    {"CFD015", "Array truncation hazard"},
    {"CFD016", "Policy violation"},
    {"CFD017", "Default drift: the default a key falls back to differs"},
    {"CFD018", "Set but never read"},
    {"CFD019", "Fingerprint does not match the report's keys"},
    {"VALUE", "Value differs between environments"},
    {"HISTORY", "Changed between one report of a sequence and the next"},
    {"BLAME", "The report a key's present state dates from"}};

void doctor_key_name(const rkey* k, char* buf, size_t cap) {
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
    /* A loop rather than memset: all-bits-zero is not a null pointer
       everywhere, and MSVC rejects handing an array of const pointers
       to a void*. */
    for (s = 0; s < CF_MAX_SOURCES; ++s) {
        first_at[s] = NULL;
    }

    for (i = 0; i < r->nkeys; ++i) {
        const rkey* k = &r->keys[i];
        for (j = 0; j < k->nc; ++j) {
            const char* nm = k->c[j].source;
            if (nm == NULL) {
                continue;
            }
            /* Only what lost to precedence. A rejected or truncated
               candidate did not lose, it was refused, and CFD006,
               CFD015 and CFD016 already say so: counting it here made a
               secret on the command line three errors at once. */
            if (k->c[j].outcome != NULL && strcmp(k->c[j].outcome, "shadowed") != 0) {
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
        finding_at(r->D, "CFD010", "warning", names[s], first_at[s],
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
        doctor_key_name(&r->keys[i], name, sizeof name);
        finding_at(r->D, "CFD005", "warning", name,
                   (r->keys[i].nc > 0) ? r->keys[i].c[0].at : NULL,
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
        doctor_key_name(k, name, sizeof name);
        for (j = 0; j < k->nc; ++j) {
            if (k->c[j].outcome != NULL) {
                continue;
            }
            finding_at(r->D, "CFD009", "warning", name, k->c[j].at,
                       "deprecated and still supplied by %s%s%s%s%s", k->c[j].source,
                       (k->c[j].at != NULL) ? " at " : "",
                       (k->c[j].at != NULL) ? k->c[j].at : "",
                       (k->replaced_by != NULL) ? "; rename it to " : "",
                       (k->replaced_by != NULL) ? k->replaced_by : "");
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

        doctor_key_name(k, name, sizeof name);
        for (j = 0; j < k->nc; ++j) {
            if (k->c[j].outcome != NULL || k->c[j].value == NULL) {
                continue;
            }
            if (k->c[j].value[0] != '\0') {
                continue;
            }
            finding_at(r->D, "CFD011", "warning", name, k->c[j].at,
                       "set to an empty value by %s%s%s, which is not the same "
                       "as being unset",
                       k->c[j].source, (k->c[j].at != NULL) ? " at " : "",
                       (k->c[j].at != NULL) ? k->c[j].at : "");
        }
    }
}

/* CFD001, unreachable value. Set where it can never win: the same
   origin sets the key again later, and within one source the later
   occurrence wins, so nothing about the environment can change the
   outcome. Editing the earlier line has no effect, which is the thing
   somebody spends an afternoon discovering.

   Why this narrowly. A candidate shadowed by another *source* is
   precedence working, and the same file may win in another environment
   where the variable is unset - one report cannot prove "never" about
   that, and saying it anyway would be the noise that gets a tool turned
   off. A source that wins nothing anywhere is CFD010. Two files in one
   drop-in directory are precedence too, so the origin has to match, not
   only the source. */
static void rule_unreachable(const report* r) {
    size_t i, j, m;

    for (i = 0; i < r->nkeys; ++i) {
        const rkey* k = &r->keys[i];
        char        name[CF_KEY_MAX];

        for (j = 0; j < k->nc; ++j) {
            const cand* c = &k->c[j];
            size_t      clen;
            long        cpos;

            if (c->synthesized || c->outcome == NULL ||
                strcmp(c->outcome, "shadowed") != 0 ||
                !doctor_place_of(c->at, &clen, &cpos)) {
                continue;
            }
            for (m = 0; m < k->nc; ++m) {
                const cand* d = &k->c[m];
                size_t      dlen;
                long        dpos;
                if (m == j || d->synthesized || d->source == NULL ||
                    c->source == NULL || strcmp(d->source, c->source) != 0 ||
                    !doctor_place_of(d->at, &dlen, &dpos) || dlen != clen ||
                    memcmp(d->at, c->at, clen) != 0 || dpos <= cpos) {
                    continue;
                }
                doctor_key_name(k, name, sizeof name);
                finding_at(r->D, "CFD001", "error", name, c->at,
                           "%s always loses to %s, set later by the same source; "
                           "editing it has no effect",
                           c->at, d->at);
                break;
            }
        }
    }
}

/* CFD007, a sensitive value from an unsafe source. A command line is
   readable by every user of the machine for as long as the process
   runs, and lands in shell history besides, so a secret passed there
   has leaked whether or not it won. Policy can refuse it, and then
   CFD016 is the finding; this is the case where nothing refused it.

   The other unsafe place is a file every user of the machine can read,
   which the report records when its platform can tell. Whether a file
   is committed to version control it does not record and will not:
   that is the repository's business rather than the configuration's,
   and guessing from a path would be wrong in both directions. */
static void rule_sensitive_source(const report* r) {
    size_t i, j;

    for (i = 0; i < r->nkeys; ++i) {
        const rkey* k = &r->keys[i];
        char        name[CF_KEY_MAX];

        if (!k->sensitive) {
            continue;
        }
        for (j = 0; j < k->nc; ++j) {
            const rsrc* s = doctor_find_source(r, k->c[j].source);
            cf_bool cli = (s != NULL && s->kind != NULL && strcmp(s->kind, "cli") == 0)
                              ? CF_TRUE
                              : CF_FALSE;
            const char* where = (k->c[j].at != NULL) ? k->c[j].at : "an unnamed place";

            if (k->c[j].synthesized || (!cli && !k->c[j].world_readable)) {
                continue;
            }
            if (k->c[j].reason != NULL && strncmp(k->c[j].reason, "policy:", 7) == 0) {
                continue; /* CFD016's */
            }
            doctor_key_name(k, name, sizeof name);
            if (cli) {
                finding_at(r->D, "CFD007", "error", name, k->c[j].at,
                           "sensitive, and supplied on the command line at %s, which "
                           "every user of the machine can read",
                           where);
            } else {
                finding_at(r->D, "CFD007", "error", name, k->c[j].at,
                           "sensitive, and supplied by %s, a file every user of the "
                           "machine can read",
                           where);
            }
        }
    }
}

/* CFD013, precedence inversion. A policy lists the sources permitted
   for a key in the order it wants them to win, and that order can put
   a less specific source above a more specific one - the environment
   above the command line, say. Usually deliberate, which is why this
   is information rather than a warning; it is worth seeing because
   explain() otherwise shows a command line losing to a variable and
   nothing on the page says why. */
static void rule_inversion(const report* r) {
    size_t i, j;

    for (i = 0; i < r->nkeys; ++i) {
        const rkey* k = &r->keys[i];
        char        name[CF_KEY_MAX];

        if (k->policy == NULL) {
            continue;
        }
        for (j = 0; k->policy[j] != NULL && k->policy[j + 1] != NULL; ++j) {
            const rsrc* hi = doctor_find_source(r, k->policy[j]);
            const rsrc* lo = doctor_find_source(r, k->policy[j + 1]);
            if (hi == NULL || lo == NULL || hi->rank <= lo->rank) {
                continue;
            }
            doctor_key_name(k, name, sizeof name);
            finding(r->D, "CFD013", "info", name,
                    "policy puts %s above %s, the reverse of their usual precedence",
                    hi->name, lo->name);
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
        doctor_key_name(k, name, sizeof name);
        finding(r->D, "CFD004", "error", name, "required and has no value");
    }
}

/* CFD006. A candidate was refused by its own contract. */
static void rule_refused(const report* r) {
    size_t i, j;

    for (i = 0; i < r->nkeys; ++i) {
        const rkey* k = &r->keys[i];
        char        name[CF_KEY_MAX];
        doctor_key_name(k, name, sizeof name);
        for (j = 0; j < k->nc; ++j) {
            /* A rejection under policy is CFD016's: the value may be
               perfectly valid, it came from somewhere it may not. Two
               findings for one problem is how the first one stops
               being read. */
            if (k->c[j].reason != NULL && strncmp(k->c[j].reason, "policy:", 7) == 0) {
                continue;
            }
            if (k->c[j].outcome != NULL && strcmp(k->c[j].outcome, "rejected") == 0) {
                finding_at(r->D, "CFD006", "error", name, k->c[j].at,
                           "%s refused %s: %s", k->c[j].source, doctor_shown(&k->c[j]),
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
        doctor_key_name(k, name, sizeof name);
        for (j = 0; j < k->nc; ++j) {
            if (k->c[j].reason == NULL || strncmp(k->c[j].reason, "policy:", 7) != 0) {
                continue;
            }
            finding_at(r->D, "CFD016", "error", name, k->c[j].at,
                       "%s supplied this from %s, and %s", k->c[j].source,
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
        doctor_key_name(k, name, sizeof name);
        finding(r->D, "CFD008", "info", name,
                "no source configured this; the built-in default applies");
    }
}

/* CFD015. An element lies beyond the length its array's winning count
   gives, so the library discarded it: it is not read, listed or written,
   whichever source supplied it - most often a longer list in a file the
   command line's shorter count overrode. Inherited from the flat model
   and worth saying out loud. The report's reason names the count that
   cut it and where that count came from; a report from another producer
   may have none, and then gets the general sentence. */
static void rule_truncated(const report* r) {
    size_t i, j;

    for (i = 0; i < r->nkeys; ++i) {
        const rkey* k = &r->keys[i];
        char        name[CF_KEY_MAX];
        doctor_key_name(k, name, sizeof name);
        for (j = 0; j < k->nc; ++j) {
            if (k->c[j].outcome == NULL || strcmp(k->c[j].outcome, "truncated") != 0) {
                continue;
            }
            finding_at(r->D, "CFD015", "warning", name, k->c[j].at, "discarded: %s",
                       (k->c[j].reason != NULL) ? k->c[j].reason
                                                : "beyond the winning array length");
        }
    }
}

/* ------------------------------------------------------------------ *
 * Comparing two reports
 * ------------------------------------------------------------------ */

/* The candidate that won: the one the report gives no outcome. */
static const cand* winner(const rkey* k) {
    size_t j;

    for (j = 0; j < k->nc; ++j) {
        if (k->c[j].outcome == NULL) {
            return &k->c[j];
        }
    }
    return NULL;
}

typedef enum { AGREE_UNKNOWN, AGREE_YES, AGREE_NO } agreement;

/* Whether two winners hold the same value. A redacted one has a digest
   and no value, so a pair with either side redacted is compared by
   digest, which is how a production report can be audited without the
   secret being in it at all. What the two reports cannot compare - a
   digest on one side and the value on the other - is neither agreement
   nor a difference. */
static agreement values_agree(const cand* a, const cand* b) {
    const char *x = a->value, *y = b->value;

    if (x == NULL || y == NULL) {
        x = a->digest;
        y = b->digest;
    }
    if (x == NULL || y == NULL) {
        return AGREE_UNKNOWN;
    }
    return (strcmp(x, y) == 0) ? AGREE_YES : AGREE_NO;
}

/* What kind of source supplied a key: "env", "file", "cli". The report's
   own table of sources says; a source it does not list is told apart by
   its name alone. */
static const char* origin_kind(const report* r, const rkey* k) {
    const rsrc* s = doctor_find_source(r, k->selected);

    return (s != NULL && s->kind != NULL) ? s->kind : k->selected;
}

/* CFD002. The two agree on the value and disagree on what kind of
   source it came from. A plain value diff shows nothing here, which is
   exactly why it is dangerous: the day the environment variable is
   missing, the two environments stop behaving the same. Values that
   differ are not drift: the value diff has said so, once. */
static void rule_origin_drift(const report* a, const report* b) {
    size_t i;

    for (i = 0; i < a->nkeys; ++i) {
        const rkey* ka = &a->keys[i];
        const rkey* kb = doctor_find_key(b, ka->key, ka->keylen);
        const cand *ca, *cb;
        char        name[CF_KEY_MAX];

        if (kb == NULL || ka->selected == NULL || kb->selected == NULL) {
            continue;
        }
        ca = winner(ka);
        cb = winner(kb);
        if (ca == NULL || cb == NULL || values_agree(ca, cb) != AGREE_YES) {
            continue;
        }
        if (strcmp(origin_kind(a, ka), origin_kind(b, kb)) == 0) {
            continue;
        }

        doctor_key_name(ka, name, sizeof name);
        finding(a->D, "CFD002", "warning", name,
                "same value, from '%s' here and '%s' there", ka->selected,
                kb->selected);
    }
}

/* CFD003. Overridden in one environment and not the other, so the code
   path exercised in testing is not the one that runs. */
static void rule_shadow_divergence(const report* a, const report* b) {
    size_t i, j;

    for (i = 0; i < a->nkeys; ++i) {
        const rkey* ka = &a->keys[i];
        const rkey* kb = doctor_find_key(b, ka->key, ka->keylen);
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
        doctor_key_name(ka, name, sizeof name);
        finding(a->D, "CFD003", "warning", name,
                "%d source(s) overridden here, %d there", da, db);
    }
}

/* CFD012. A key exists on one side and not the other. */
static void rule_parity(const report* a, const report* b) {
    size_t i;

    for (i = 0; i < a->nkeys; ++i) {
        char name[CF_KEY_MAX];
        if (doctor_find_key(b, a->keys[i].key, a->keys[i].keylen) != NULL) {
            continue;
        }
        doctor_key_name(&a->keys[i], name, sizeof name);
        finding(a->D, "CFD012", "warning", name, "present in %s, absent in %s", a->path,
                b->path);
    }
}

static void rule_value_diff(const report* a, const report* b) {
    size_t i;

    for (i = 0; i < a->nkeys; ++i) {
        const rkey* ka = &a->keys[i];
        const rkey* kb = doctor_find_key(b, ka->key, ka->keylen);
        const cand *ca, *cb;
        char        name[CF_KEY_MAX];

        if (kb == NULL || ka->selected == NULL || kb->selected == NULL) {
            continue;
        }
        ca = winner(ka);
        cb = winner(kb);
        if (ca == NULL || cb == NULL || values_agree(ca, cb) != AGREE_NO) {
            continue;
        }
        if (ca->is_default && cb->is_default) {
            continue; /* CFD017's: two defaults, not two configurations */
        }

        doctor_key_name(ka, name, sizeof name);
        if (ca->value == NULL || cb->value == NULL) {
            finding(a->D, "CFD014", "info", name,
                    "sensitive value differs (by digest)");
        } else {
            finding(a->D, "VALUE", "info", name, "%s here, %s there", ca->value,
                    cb->value);
        }
    }
}

/* The candidate that is the key's declared default, if it has one. */
static const cand* default_of(const rkey* k) {
    size_t j;

    for (j = 0; j < k->nc; ++j) {
        if (k->c[j].is_default && !k->c[j].synthesized) {
            return &k->c[j];
        }
    }
    return NULL;
}

/* CFD017. A key takes its declared default on one side or both, and the
   defaults are not the same: the program changed what it falls back
   to, which no configuration file shows. A new release's default
   reaches every deployment that never set the key, so diffing the
   deployed report against the candidate's names those keys before the
   upgrade does. A default on one side only is the same hazard. */
static void rule_default_drift(const report* a, const report* b) {
    size_t i;

    for (i = 0; i < a->nkeys; ++i) {
        const rkey* ka = &a->keys[i];
        const rkey* kb = doctor_find_key(b, ka->key, ka->keylen);
        const cand *da, *db;
        cf_bool     here, there;
        const char* side;
        char        name[CF_KEY_MAX];

        if (kb == NULL) {
            continue;
        }
        da    = default_of(ka);
        db    = default_of(kb);
        here  = (da != NULL && winner(ka) == da) ? CF_TRUE : CF_FALSE;
        there = (db != NULL && winner(kb) == db) ? CF_TRUE : CF_FALSE;
        if (!here && !there) {
            continue;
        }
        doctor_key_name(ka, name, sizeof name);
        side = (here && there) ? "in both" : here ? "here" : "there";
        if (da == NULL || db == NULL) {
            finding(a->D, "CFD017", "warning", name,
                    "takes its default %s; %s it has none", side,
                    here ? "there" : "here");
        } else if (values_agree(da, db) != AGREE_NO) {
            continue;
        } else if (da->value == NULL || db->value == NULL) {
            finding(a->D, "CFD017", "warning", name,
                    "takes its default %s; the defaults differ (by digest)", side);
        } else if (here && there) {
            finding(a->D, "CFD017", "warning", name,
                    "takes its default in both, '%s' here and '%s' there", da->value,
                    db->value);
        } else {
            finding(a->D, "CFD017", "warning", name,
                    "takes its default %s, '%s'; %s the default is '%s'", side,
                    here ? da->value : db->value, here ? "there" : "here",
                    here ? db->value : da->value);
        }
    }
}

/* CFD018. A source set the key and the program never read it, as far
   as the report saw: a setting that changes nothing, most often one the
   program has stopped reading. Only a report from a program that
   tracked reads (cf_builder_track_reads) says, and only what was read
   before it was written, so take it once the program has done its
   work. A key whose default won, or whose count a list made up, is not
   a setting. */
static void rule_unread(const report* r) {
    size_t i;

    for (i = 0; i < r->nkeys; ++i) {
        const rkey* k = &r->keys[i];
        const cand* w;
        char        name[CF_KEY_MAX];

        if (k->read != 0) {
            continue;
        }
        w = winner(k);
        if (w == NULL || w->is_default || w->synthesized) {
            continue;
        }
        doctor_key_name(k, name, sizeof name);
        finding_at(r->D, "CFD018", "info", name, w->at, "set by %s and never read",
                   w->source);
    }
}

/* CFD019. The fingerprint a report states is the one its own keys give.
   cf_fingerprint takes each selected value in key order and a secret by
   its digest, which a value-blind report carries, so the report holds all
   it needs to be checked: one edited by hand, cut short, or written by a
   build that got it wrong says one thing and shows another. A report
   from before the field has none, and that is not a fault. The steps are
   the library's (cf_fingerprint_step), not a copy of them.
   Left unchecked, with no finding, when the report cannot be: a key
   whose type or value it does not give. */
static void rule_fingerprint(const report* r) {
    cf_u64 h;
    size_t i;
    char   want[24];

    if (r->fingerprint == NULL || (r->nkeys > 1 && r->byname == NULL)) {
        return;
    }
    h = cf_fingerprint_seed();
    for (i = 0; i < r->nkeys; ++i) {
        const rkey*     k = (r->byname != NULL) ? r->byname[i].k : &r->keys[i];
        const cand*     w = winner(k);
        const cf_value* v;
        char            at[CF_KEY_MAX];
        char            buf[CF_VALUE_TEXT_MAX > 64 ? CF_VALUE_TEXT_MAX : 64];
        const char*     type;
        const char*     text;
        size_t          n;

        if (w == NULL) {
            continue; /* nothing selected: the fingerprint skips it too */
        }
        n = cf_fmt(at, sizeof at, "keys.%S.candidates.%zu.value", k->key, k->keylen,
                   (size_t)(w - k->c));
        v = (n < sizeof at) ? cf_ks_get(&r->doc, at, n) : NULL;
        if (k->sensitive) {
            if (w->digest != NULL) {
                /* Blind: the digest in place of the value, and the
                   declared type, which a secret's value is held as. */
                type = k->type;
                text = w->digest;
                n    = strlen(text);
            } else if (v != NULL) {
                type = cf_type_name(v->type);
                n    = cf_ks_digest_text(cf_ks_value_digest(v), buf, sizeof buf);
                text = buf;
            } else {
                return;
            }
        } else if (v != NULL) {
            type = cf_type_name(v->type);
            if (v->type == CF_TYPE_TEXT) {
                text = v->u.t.p;
                n    = v->u.t.n;
            } else {
                n    = cf_value_to_text(v, buf, sizeof buf);
                text = buf;
            }
        } else {
            return;
        }
        if (type == NULL || text == NULL) {
            return;
        }
        h = cf_fingerprint_step(h, k->key, k->keylen, type, text, n);
    }
    (void)cf_ks_digest_text(h, want, sizeof want);
    if (strcmp(want, r->fingerprint) != 0) {
        finding(r->D, "CFD019", "error", "fingerprint",
                "the report says %s and its keys give %s: edited, cut short, or "
                "written by a build that got it wrong",
                r->fingerprint, want);
    }
}

/* The rules run from tables, through a pointer, rather than as a list of
   calls. Called directly, each is called once, so a compiler may inline
   every one into the same frame without sharing their buffers: Alpine's
   gcc at -O3 made main 4416 bytes that way, over the stack budget. */
typedef void (*lint_rule)(const report*);
typedef void (*diff_rule)(const report*, const report*);

static const lint_rule g_lint_rules[] = {
    rule_unreachable, rule_dead_source, rule_required,
    rule_refused,     rule_policy,      rule_sensitive_source,
    rule_inversion,   rule_undefined,   rule_deprecated,
    rule_empty,       rule_truncated,   rule_default_only,
    rule_unread,      rule_fingerprint};

static const diff_rule g_diff_rules[] = {rule_origin_drift, rule_shadow_divergence,
                                         rule_default_drift, rule_value_diff};

void doctor_lint_reports(const report* r) {
    size_t i;
    /* A profile decides some severities; a diff spans two, so only lint
       has one to apply. */
    r->D->profile = r->profile;
    if (r->D->only_base != NULL) {
        (void)only_prepare(r->D, r);
    }
    for (i = 0; i < sizeof g_lint_rules / sizeof g_lint_rules[0]; ++i) {
        g_lint_rules[i](r);
    }
    only_done(r->D);
    r->D->profile = NULL;
}

void doctor_diff_reports(const report* a, const report* b) {
    size_t i;
    /* Parity asks what one side has and the other lacks, so it runs both
       ways round. */
    rule_parity(a, b);
    rule_parity(b, a);
    for (i = 0; i < sizeof g_diff_rules / sizeof g_diff_rules[0]; ++i) {
        g_diff_rules[i](a, b);
    }
}

/* ------------------------------------------------------------------ *
 * A sequence of reports
 *
 * The same service's report from one deployment after another, oldest
 * first: what changed from each to the next, and since which report a
 * key has been what it is now. The order is the caller's - a report
 * says when it was written, and a re-run of an old build would put it
 * in the wrong place. No value is printed: a history is pasted into
 * tickets the reports themselves never go near, so it names reports
 * and sources, and compares digests without showing them.
 * ------------------------------------------------------------------ */

typedef enum { STEP_SAME, STEP_CHANGED, STEP_UNKNOWN } step_result;

static const cand* chosen(const rkey* k) {
    return (k != NULL && k->selected != NULL) ? winner(k) : NULL;
}

/* What happened to one key from one report to the next: `was` its
   entry in the earlier, `now` in the later, NULL where a report lacks
   it. A step the two cannot settle - one shows the value and the other
   only its digest - is STEP_UNKNOWN, neither same nor changed, as
   values_agree has it. */
static step_result key_step(const rkey* was, const rkey* now, char* what, size_t cap) {
    const cand* wa = chosen(was);
    const cand* wb = chosen(now);
    agreement   agree;
    cf_bool     moved;

    if (was == NULL || now == NULL) {
        if (was == NULL && now == NULL) {
            return STEP_SAME;
        }
        if (now == NULL) {
            cf_fmt(what, cap, "removed");
        } else if (wb != NULL) {
            cf_fmt(what, cap, "added, from '%s'", now->selected);
        } else {
            cf_fmt(what, cap, "added, with no value");
        }
        return STEP_CHANGED;
    }
    if (wa == NULL || wb == NULL) {
        if (wa == NULL && wb == NULL) {
            return STEP_SAME;
        }
        if (wb != NULL) {
            cf_fmt(what, cap, "now set, from '%s'", now->selected);
        } else {
            cf_fmt(what, cap, "no longer set; it was from '%s'", was->selected);
        }
        return STEP_CHANGED;
    }
    agree = values_agree(wa, wb);
    moved = (strcmp(was->selected, now->selected) != 0) ? CF_TRUE : CF_FALSE;
    if (agree == AGREE_YES) {
        if (!moved) {
            return STEP_SAME;
        }
        cf_fmt(what, cap, "same value, now from '%s' rather than '%s'", now->selected,
               was->selected);
    } else if (agree == AGREE_NO) {
        const char* how =
            (wa->value == NULL || wb->value == NULL) ? " (by digest)" : "";
        if (moved) {
            cf_fmt(what, cap, "value changed%s, now from '%s' rather than '%s'", how,
                   now->selected, was->selected);
        } else {
            cf_fmt(what, cap, "value changed%s, still from '%s'", how, now->selected);
        }
    } else if (moved) {
        cf_fmt(what, cap,
               "now from '%s' rather than '%s'; whether the value changed cannot "
               "be told",
               now->selected, was->selected);
    } else {
        cf_fmt(what, cap,
               "whether the value changed cannot be told: one report has it only "
               "as a digest, and the other has no digest to match");
        return STEP_UNKNOWN;
    }
    return STEP_CHANGED;
}

/* One step's finding, recorded or only counted. */
static size_t history_step(const report* later, const rkey* was, const rkey* now,
                           cf_bool record) {
    char what[256];

    if (key_step(was, now, what, sizeof what) == STEP_SAME) {
        return 0;
    }
    if (record) {
        char        name[CF_KEY_MAX];
        const cand* w = chosen(now);

        doctor_key_name((now != NULL) ? now : was, name, sizeof name);
        finding_at(later->D, "HISTORY", "info", name, (w != NULL) ? w->at : NULL,
                   "%s: %s", later->path, what);
    }
    return 1;
}

/* Every change from each report to the next, in order: the keys of the
   later report, then those it lost. Run once to count and once to
   record, so room for all of them is reserved before the first. */
static size_t history_pass(const report* rs, size_t n, const char* key,
                           cf_bool record) {
    size_t i, j, count = 0;

    for (i = 1; i < n; ++i) {
        const report* was = &rs[i - 1];
        const report* now = &rs[i];

        if (key != NULL) {
            size_t kl = strlen(key);
            count += history_step(now, doctor_find_key(was, key, kl),
                                  doctor_find_key(now, key, kl), record);
            continue;
        }
        for (j = 0; j < now->nkeys; ++j) {
            const rkey* k = &now->keys[j];
            count +=
                history_step(now, doctor_find_key(was, k->key, k->keylen), k, record);
        }
        for (j = 0; j < was->nkeys; ++j) {
            const rkey* k = &was->keys[j];
            if (doctor_find_key(now, k->key, k->keylen) == NULL) {
                count += history_step(now, k, NULL, record);
            }
        }
    }
    return count;
}

/* Since which report `key` has been as the last one has it: walked back
   from the last report while nothing changes. One finding, placed where
   that report says the value was set. */
static void blame_key(const report* rs, size_t n, const char* key) {
    size_t      kl   = strlen(key);
    size_t      k    = n - 1;
    step_result step = STEP_SAME;
    const rkey* now  = doctor_find_key(&rs[n - 1], key, kl);
    const cand* w;
    char        what[256];
    char        state[160];

    while (k > 0) {
        step = key_step(doctor_find_key(&rs[k - 1], key, kl),
                        doctor_find_key(&rs[k], key, kl), what, sizeof what);
        if (step != STEP_SAME) {
            break;
        }
        --k;
    }
    w = chosen(doctor_find_key(&rs[k], key, kl));
    if (now == NULL) {
        cf_fmt(state, sizeof state, "absent");
    } else if (chosen(now) == NULL) {
        cf_fmt(state, sizeof state, "set by no source");
    } else {
        cf_fmt(state, sizeof state, "from '%s'", now->selected);
    }
    if (step == STEP_SAME) {
        finding_at(rs[0].D, "BLAME", "info", key, (w != NULL) ? w->at : NULL,
                   "%s since %s, the first of %u report(s)", state, rs[0].path,
                   (unsigned)n);
    } else if (step == STEP_CHANGED) {
        finding_at(rs[0].D, "BLAME", "info", key, (w != NULL) ? w->at : NULL,
                   "%s since %s: %s", state, rs[k].path, what);
    } else {
        finding_at(rs[0].D, "BLAME", "info", key, (w != NULL) ? w->at : NULL,
                   "%s since %s at least; before it, %s", state, rs[k].path, what);
    }
}

/* ------------------------------------------------------------------ *
 * The library (cf_doctor.h)
 * ------------------------------------------------------------------ */

cf_doctor* cf_doctor_new(void) {
    const cf_allocator* a = cf_default_allocator();
    cf_doctor*          d = (cf_doctor*)a->alloc(a->ctx, sizeof(cf_doctor));

    if (d != NULL) {
        memset(d, 0, sizeof *d);
    }
    return d;
}

void cf_doctor_free(cf_doctor* d) {
    const cf_allocator* a = cf_default_allocator();

    if (d == NULL) {
        return;
    }
    found_release(d);
    if (d->only_base != NULL) {
        doctor_release(d->only_base);
        a->release(a->ctx, d->only_base);
    }
    a->release(a->ctx, d);
}

cf_bool cf_doctor_only_changed_file(cf_doctor* d, const char* path, cf_error* err) {
    const cf_allocator* a = cf_default_allocator();
    report*             r;

    if (d == NULL || path == NULL) {
        return cf_error_set(err, CF_E_SOURCE, NULL, path, "no base report");
    }
    r = (report*)a->alloc(a->ctx, sizeof *r);
    if (r == NULL) {
        return cf_error_set(err, CF_E_MEMORY, NULL, path,
                            "no room for the base report");
    }
    memset(r, 0, sizeof *r);
    if (!doctor_load(r, d, path, err)) {
        doctor_release(r);
        a->release(a->ctx, r);
        return CF_FALSE;
    }
    if (d->only_base != NULL) {
        doctor_release(d->only_base);
        a->release(a->ctx, d->only_base);
    }
    d->only_base = r;
    return CF_TRUE;
}

size_t cf_doctor_unchanged_count(const cf_doctor* d) {
    return (d != NULL && d->unchanged > 0) ? (size_t)d->unchanged : 0;
}

cf_bool cf_doctor_accept(cf_doctor* d, const char* rule, const char* key) {
    if (d == NULL || rule == NULL || key == NULL || d->base.count >= MAX_BASELINE) {
        return CF_FALSE;
    }
    d->base.loaded = CF_TRUE;
    cf_fmt(d->base.id[d->base.count], sizeof d->base.id[0], "%s", rule);
    cf_fmt(d->base.key[d->base.count], sizeof d->base.key[0], "%s", key);
    d->base.matched[d->base.count] = CF_FALSE;
    ++d->base.count;
    return CF_TRUE;
}

cf_bool cf_doctor_baseline(cf_doctor* d, const char* path, cf_error* err) {
    if (d == NULL || path == NULL) {
        return cf_error_set(err, CF_E_SOURCE, NULL, NULL, "no baseline to read");
    }
    baseline_load(d, path);
    return CF_TRUE;
}

cf_bool cf_doctor_severity(cf_doctor* d, const char* rule, const char* level,
                           const char* profile, cf_error* err) {
    severity_line* s;

    if (d == NULL || rule == NULL || level == NULL) {
        return cf_error_set(err, CF_E_SOURCE, NULL, NULL, "no severity to set");
    }
    if (!doctor_level_known(level)) {
        return cf_error_set(err, CF_E_SOURCE, rule, NULL,
                            "'%s' is not a severity; use error, warning, info or off",
                            level);
    }
    if (d->nsev >= MAX_SEVERITY) {
        return cf_error_set(err, CF_E_MEMORY, rule, NULL, "more than %d severities",
                            MAX_SEVERITY);
    }
    s = &d->sev[d->nsev++];
    cf_fmt(s->id, sizeof s->id, "%s", rule);
    cf_fmt(s->level, sizeof s->level, "%s", level);
    cf_fmt(s->profile, sizeof s->profile, "%s", (profile != NULL) ? profile : "");
    return CF_TRUE;
}

cf_bool cf_doctor_severity_file(cf_doctor* d, const char* path, cf_error* err) {
    if (d == NULL || path == NULL) {
        return cf_error_set(err, CF_E_SOURCE, NULL, NULL, "no severity file to read");
    }
    return severity_load(d, path, err);
}

/* A configuration's report, written in memory with every value. The
   buffer is wiped before it is released, since it held them. */
static char* doctor_report_of(const cf_config* c, const char* profile, size_t* n,
                              cf_error* err) {
    const cf_allocator* a    = cf_default_allocator();
    cf_report_info      info = CF_REPORT_INFO_INIT;
    char*               buf;

    info.mode        = CF_REPORT_FULL;
    info.profile     = profile;
    info.application = "cf_doctor";
    *n               = cf_report(c, &info, NULL, 0);
    if (*n == 0) {
        cf_error_set(err, CF_E_CAPABILITY, NULL, NULL,
                     "a configuration's report needs CF_WITH_REPORT");
        return NULL;
    }
    buf = (char*)a->alloc(a->ctx, *n + 1);
    if (buf == NULL) {
        cf_error_set(err, CF_E_MEMORY, NULL, NULL, "no room for the report");
        return NULL;
    }
    cf_report(c, &info, buf, *n + 1);
    return buf;
}

static void doctor_report_free(char* buf, size_t n) {
    const cf_allocator* a = cf_default_allocator();
    volatile char*      p = buf;
    size_t              i;

    if (buf == NULL) {
        return;
    }
    for (i = 0; i < n; ++i) {
        p[i] = 0;
    }
    a->release(a->ctx, buf);
}

cf_bool cf_doctor_lint_report(cf_doctor* d, const char* json, size_t n,
                              const char* name, cf_error* err) {
    report  r;
    cf_bool ok;

    if (d == NULL || json == NULL) {
        return cf_error_set(err, CF_E_SOURCE, NULL, name, "no report to lint");
    }
    ok = doctor_load_text(&r, d, json, n, name, err);
    if (ok) {
        doctor_lint_reports(&r);
    }
    doctor_release(&r);
    return ok;
}

cf_bool cf_doctor_lint_file(cf_doctor* d, const char* path, cf_error* err) {
    report  r;
    cf_bool ok;

    if (d == NULL || path == NULL) {
        return cf_error_set(err, CF_E_SOURCE, NULL, path, "no report to lint");
    }
    ok = doctor_load(&r, d, path, err);
    if (ok) {
        doctor_lint_reports(&r);
    }
    doctor_release(&r);
    return ok;
}

cf_bool cf_doctor_lint(cf_doctor* d, const cf_config* c, const char* profile,
                       cf_error* err) {
    size_t  n   = 0;
    char*   buf = (c != NULL) ? doctor_report_of(c, profile, &n, err) : NULL;
    cf_bool ok;

    if (buf == NULL) {
        return (c == NULL)
                   ? cf_error_set(err, CF_E_SOURCE, NULL, NULL, "no configuration")
                   : CF_FALSE;
    }
    ok = cf_doctor_lint_report(d, buf, n, "<configuration>", err);
    doctor_report_free(buf, n);
    return ok;
}

cf_bool cf_doctor_diff_report(cf_doctor* d, const char* a, size_t an, const char* aname,
                              const char* b, size_t bn, const char* bname,
                              cf_error* err) {
    report  ra, rb;
    cf_bool ok;

    if (d == NULL || a == NULL || b == NULL) {
        return cf_error_set(err, CF_E_SOURCE, NULL, NULL, "two reports are needed");
    }
    memset(&rb, 0, sizeof rb);
    ok = doctor_load_text(&ra, d, a, an, aname, err) &&
         doctor_load_text(&rb, d, b, bn, bname, err);
    if (ok) {
        doctor_diff_reports(&ra, &rb);
    }
    doctor_release(&ra);
    doctor_release(&rb);
    return ok;
}

cf_bool cf_doctor_diff_files(cf_doctor* d, const char* a, const char* b,
                             cf_error* err) {
    report  ra, rb;
    cf_bool ok;

    if (d == NULL || a == NULL || b == NULL) {
        return cf_error_set(err, CF_E_SOURCE, NULL, NULL, "two reports are needed");
    }
    memset(&rb, 0, sizeof rb);
    ok = doctor_load(&ra, d, a, err) && doctor_load(&rb, d, b, err);
    if (ok) {
        doctor_diff_reports(&ra, &rb);
    }
    doctor_release(&ra);
    doctor_release(&rb);
    return ok;
}

cf_bool cf_doctor_diff(cf_doctor* d, const cf_config* a, const cf_config* b,
                       cf_error* err) {
    size_t  an = 0, bn = 0;
    char*   ra = (a != NULL && b != NULL) ? doctor_report_of(a, NULL, &an, err) : NULL;
    char*   rb = (ra != NULL) ? doctor_report_of(b, NULL, &bn, err) : NULL;
    cf_bool ok = CF_FALSE;

    if (a == NULL || b == NULL) {
        cf_error_set(err, CF_E_SOURCE, NULL, NULL, "two configurations are needed");
    } else if (ra != NULL && rb != NULL) {
        ok = cf_doctor_diff_report(d, ra, an, "<a>", rb, bn, "<b>", err);
    }
    doctor_report_free(ra, an);
    doctor_report_free(rb, bn);
    return ok;
}

static void reports_release(report* rs, size_t n) {
    const cf_allocator* a = cf_default_allocator();
    size_t              i;

    for (i = 0; i < n; ++i) {
        doctor_release(&rs[i]);
    }
    a->release(a->ctx, rs);
}

/* Every report of a sequence, or none: one that cannot be read leaves
   no finding from the rest, as a sequence missing a step would tell a
   different history. */
static report* reports_load(cf_doctor* d, const char* const* paths, size_t n,
                            cf_error* err) {
    const cf_allocator* a = cf_default_allocator();
    report*             rs;
    size_t              i;

    if (d == NULL || paths == NULL || n == 0) {
        cf_error_set(err, CF_E_SOURCE, NULL, NULL, "no reports to read");
        return NULL;
    }
    rs = (n <= (size_t)-1 / sizeof(report))
             ? (report*)a->alloc(a->ctx, n * sizeof(report))
             : NULL;
    if (rs == NULL) {
        cf_error_set(err, CF_E_MEMORY, NULL, NULL, "no room for %u reports",
                     (unsigned)n);
        return NULL;
    }
    memset(rs, 0, n * sizeof(report));
    for (i = 0; i < n; ++i) {
        if (paths[i] == NULL) {
            cf_error_set(err, CF_E_SOURCE, NULL, NULL, "report %u has no path",
                         (unsigned)(i + 1));
            reports_release(rs, i);
            return NULL;
        }
        if (!doctor_load(&rs[i], d, paths[i], err)) {
            reports_release(rs, i + 1);
            return NULL;
        }
    }
    return rs;
}

static cf_bool key_in_any(const report* rs, size_t n, const char* key) {
    size_t i;

    for (i = 0; i < n; ++i) {
        if (doctor_find_key(&rs[i], key, strlen(key)) != NULL) {
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}

cf_bool cf_doctor_history_files(cf_doctor* d, const char* const* paths, size_t n,
                                const char* key, cf_error* err) {
    report* rs = reports_load(d, paths, n, err);
    cf_bool ok = CF_FALSE;

    if (rs == NULL) {
        return CF_FALSE;
    }
    if (key != NULL && !key_in_any(rs, n, key)) {
        cf_error_set(err, CF_E_KEY, key, NULL, "'%s' is in none of the reports", key);
    } else if (!found_reserve(d, history_pass(rs, n, key, CF_FALSE))) {
        cf_error_set(err, CF_E_MEMORY, key, NULL, "no room for the history");
    } else {
        (void)history_pass(rs, n, key, CF_TRUE);
        ok = CF_TRUE;
    }
    reports_release(rs, n);
    return ok;
}

cf_bool cf_doctor_blame_files(cf_doctor* d, const char* const* paths, size_t n,
                              const char* key, cf_error* err) {
    report* rs;
    cf_bool ok = CF_FALSE;

    if (key == NULL) {
        return cf_error_set(err, CF_E_KEY, NULL, NULL, "no key to blame");
    }
    rs = reports_load(d, paths, n, err);
    if (rs == NULL) {
        return CF_FALSE;
    }
    if (!key_in_any(rs, n, key)) {
        cf_error_set(err, CF_E_KEY, key, NULL, "'%s' is in none of the reports", key);
    } else if (!found_reserve(d, 1)) {
        cf_error_set(err, CF_E_MEMORY, key, NULL, "no room for the answer");
    } else {
        blame_key(rs, n, key);
        ok = CF_TRUE;
    }
    reports_release(rs, n);
    return ok;
}

size_t cf_doctor_count(const cf_doctor* d) {
    return (d != NULL) ? d->nfound : 0;
}

const cf_finding* cf_doctor_finding(cf_doctor* d, size_t i) {
    dfinding* f;

    if (d == NULL || i >= d->nfound) {
        return NULL;
    }
    f               = &d->found[i];
    f->view.rule    = f->id;
    f->view.level   = f->level;
    f->view.key     = f->key;
    f->view.at      = (f->at[0] != '\0') ? f->at : NULL;
    f->view.message = f->msg;
    return &f->view;
}

size_t cf_doctor_count_at(const cf_doctor* d, const char* level) {
    if (d == NULL || level == NULL) {
        return 0;
    }
    if (strcmp(level, "error") == 0) {
        return (size_t)d->errors;
    }
    if (strcmp(level, "warning") == 0) {
        return (size_t)d->warnings;
    }
    if (strcmp(level, "info") == 0) {
        return (size_t)d->infos;
    }
    if (strcmp(level, "accepted") == 0) {
        return (size_t)d->baselined;
    }
    return 0;
}

size_t cf_doctor_stale_count(const cf_doctor* d) {
    size_t i, n = 0;

    if (d == NULL) {
        return 0;
    }
    for (i = 0; i < d->base.count; ++i) {
        if (!d->base.matched[i]) {
            ++n;
        }
    }
    return n;
}

cf_bool cf_doctor_stale(const cf_doctor* d, size_t i, const char** rule,
                        const char** key) {
    size_t j, n = 0;

    if (d == NULL) {
        return CF_FALSE;
    }
    for (j = 0; j < d->base.count; ++j) {
        if (!d->base.matched[j] && n++ == i) {
            if (rule != NULL) {
                *rule = d->base.id[j];
            }
            if (key != NULL) {
                *key = d->base.key[j];
            }
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}

size_t cf_doctor_rule_count(void) {
    return sizeof g_rules / sizeof g_rules[0];
}

const char* cf_doctor_rule_id(size_t i) {
    return (i < cf_doctor_rule_count()) ? g_rules[i].id : NULL;
}

const char* cf_doctor_rule_text(const char* id) {
    size_t i;

    for (i = 0; id != NULL && i < cf_doctor_rule_count(); ++i) {
        if (strcmp(g_rules[i].id, id) == 0) {
            return g_rules[i].text;
        }
    }
    return NULL;
}
