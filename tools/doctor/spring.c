/*
 * spring.c - Doctor's adapter for Spring Boot's /actuator/env. C89.
 *
 *     curl -s localhost:8080/actuator/env | doctor import-spring > app.json
 *
 * Spring publishes its own origins, as git does: /actuator/env lists the
 * property sources in the order Spring consults them, highest first,
 * and each property with its value and, for most sources, where it was
 * read - "class path resource [application.properties] - 3:13". This
 * writes that down in the shape Doctor reads, through the library's own
 * report writer. It resolves nothing (Doctor.md, section 12):
 *
 *   precedence   the order Spring printed, which is Spring's order. The
 *                first source holding a property is the one Spring
 *                uses, and the adapter only writes that down.
 *   keys         Spring's names as printed. One a report key cannot
 *                carry - SERVER_PORT from the environment, before
 *                Spring's relaxed binding has made it server.port - is
 *                left out and counted, never re-spelled: the binding is
 *                Spring's to do.
 *   secrets      Spring's sanitiser decides, not this: a value it masked
 *                ("******") is recorded as sensitive, and only those. A
 *                blanket redaction, as the git adapter does, would make
 *                every command-line property a secret to CFD007; Spring
 *                already exposed what /actuator/env exposed, and masked
 *                what it judged a secret - every value, by default, since
 *                Spring Boot 3.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#include "adapters.h"

#define SPRING_SOURCES CF_MAX_SOURCES

typedef struct spring_scan {
    cf_arena*     ar;
    cf_keyspace   ks;
    cf_defset     ds;
    int           depth;       /* containers open                    */
    int           in_sources;  /* depth of the propertySources array */
    int           in_props;    /* depth of a source's properties     */
    int           in_profiles; /* depth of activeProfiles            */
    int           source;      /* index of the source being read     */
    char*         names[SPRING_SOURCES];
    size_t        nsources;
    char          prop[CF_KEY_MAX]; /* the property being read          */
    size_t        plen;
    cf_bool       prop_ok;
    cf_value      value;
    cf_bool       has_value;
    char          origin[512];
    char          profile[128];
    unsigned long skipped;
    char          first_skipped[CF_KEY_MAX];
    cf_bool       failed;
} spring_scan;

static void spring_note_skip(spring_scan* s, const char* name, size_t n) {
    if (s->skipped++ == 0) {
        size_t k = (n + 1 < sizeof s->first_skipped) ? n : sizeof s->first_skipped - 1;
        memcpy(s->first_skipped, name, k);
        s->first_skipped[k] = '\0';
    }
}

/* "class path resource [application.properties] - 3:13" is
   application.properties at line 3, column 13, which is how Doctor and
   every forge read a place; anything else is kept as Spring wrote it. */
static void spring_origin(const char* in, size_t n, char* out, size_t cap) {
    const char* open = (const char*)memchr(in, '[', n);
    const char* close =
        (open != NULL) ? (const char*)memchr(open, ']', n - (size_t)(open - in)) : NULL;

    if (open != NULL && close != NULL && close + 3 < in + n && close[1] == ' ' &&
        close[2] == '-' && close[3] == ' ') {
        cf_fmt(out, cap, "%S:%S", open + 1, (size_t)(close - open - 1), close + 4,
               (size_t)(in + n - close - 4));
        return;
    }
    cf_fmt(out, cap, "%S", in, n);
}

/* One finished property: a candidate from the current source. */
static void spring_property(spring_scan* s) {
    cf_value v = s->value;

    if (!s->prop_ok || !s->has_value || s->source < 0) {
        return;
    }
    if (v.type == CF_TYPE_TEXT && v.u.t.n == 6 && memcmp(v.u.t.p, "******", 6) == 0) {
        /* Spring masked it: a secret, or Spring treats it as one. */
        cf_error    err = CF_ERROR_INIT;
        const char* key = cf_arena_dup(s->ar, s->prop, s->plen);
        if (key != NULL && cf_defset_find(&s->ds, key, s->plen) == NULL) {
            cf_def_text(&s->ds, key, NULL, CF_SENSITIVE, &err);
        }
    }
    if (!cf_ks_add(&s->ks, s->prop, s->plen, s->source * 10, &v,
                   (s->origin[0] != '\0') ? s->origin : NULL)) {
        s->failed = CF_TRUE;
    }
}

static cf_bool spring_table(void* ctx, const char* name, size_t n) {
    spring_scan* s = (spring_scan*)ctx;

    ++s->depth;
    if (s->in_sources > 0 && s->depth == s->in_sources + 1) {
        /* A property source. */
        if (s->nsources >= SPRING_SOURCES) {
            s->failed = CF_TRUE;
            return CF_FALSE;
        }
        s->source           = (int)s->nsources++;
        s->names[s->source] = NULL;
    } else if (s->source >= 0 && s->depth == s->in_sources + 2 && name != NULL &&
               n == 10 && memcmp(name, "properties", 10) == 0) {
        s->in_props = s->depth;
    } else if (s->in_props > 0 && s->depth == s->in_props + 1 && name != NULL) {
        /* A property, by the name Spring printed. */
        s->prop_ok = (n < sizeof s->prop && cf_key_valid(name, n)) ? CF_TRUE : CF_FALSE;
        s->has_value = CF_FALSE;
        s->origin[0] = '\0';
        if (s->prop_ok) {
            memcpy(s->prop, name, n);
            s->prop[n] = '\0';
            s->plen    = n;
        } else {
            spring_note_skip(s, name, n);
        }
    }
    return CF_TRUE;
}

static cf_bool spring_array(void* ctx, const char* name, size_t n) {
    spring_scan* s = (spring_scan*)ctx;

    ++s->depth;
    if (s->depth == 2 && name != NULL) {
        if (n == 15 && memcmp(name, "propertySources", 15) == 0) {
            s->in_sources = s->depth;
        } else if (n == 14 && memcmp(name, "activeProfiles", 14) == 0) {
            s->in_profiles = s->depth;
        }
    }
    return CF_TRUE;
}

static cf_bool spring_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                             cf_bool textual, const cf_loc* at) {
    spring_scan* s = (spring_scan*)ctx;

    (void)textual;
    (void)at;
    if (s->in_profiles > 0 && s->depth == s->in_profiles && v->type == CF_TYPE_TEXT &&
        s->profile[0] == '\0') {
        /* The first active profile: the one a severity is likeliest to
           name. */
        cf_fmt(s->profile, sizeof s->profile, "%S", v->u.t.p, v->u.t.n);
    } else if (s->source >= 0 && s->depth == s->in_sources + 1 && name != NULL &&
               n == 4 && memcmp(name, "name", 4) == 0 && v->type == CF_TYPE_TEXT) {
        s->names[s->source] = cf_arena_dup(s->ar, v->u.t.p, v->u.t.n);
    } else if (s->in_props > 0 && s->depth == s->in_props + 1 && name != NULL) {
        if (n == 5 && memcmp(name, "value", 5) == 0) {
            s->value     = *v;
            s->has_value = CF_TRUE;
            if (v->type == CF_TYPE_TEXT) {
                /* Copied: the codec's text does not outlive the event. */
                char* p = cf_arena_dup(s->ar, v->u.t.p, v->u.t.n);
                if (p == NULL) {
                    s->failed = CF_TRUE;
                    return CF_FALSE;
                }
                s->value.u.t.p = p;
            }
        } else if (n == 6 && memcmp(name, "origin", 6) == 0 &&
                   v->type == CF_TYPE_TEXT) {
            spring_origin(v->u.t.p, v->u.t.n, s->origin, sizeof s->origin);
        }
    }
    return CF_TRUE;
}

static cf_bool spring_end(void* ctx) {
    spring_scan* s = (spring_scan*)ctx;

    if (s->in_props > 0 && s->depth == s->in_props + 1) {
        spring_property(s);
    } else if (s->depth == s->in_props) {
        s->in_props = 0;
    } else if (s->in_sources > 0 && s->depth == s->in_sources + 1) {
        s->source = -1;
    } else if (s->depth == s->in_sources) {
        s->in_sources = 0;
    } else if (s->depth == s->in_profiles) {
        s->in_profiles = 0;
    }
    --s->depth;
    return s->failed ? CF_FALSE : CF_TRUE;
}

/* What kind of source Spring's name says it is, in the words Doctor's
   rules use: a secret on the command line is CFD007's business, and a
   file is a file. Anything else is Spring's own kind. */
static const char* spring_kind(const char* name) {
    if (name == NULL) {
        return "spring";
    }
    if (strcmp(name, "commandLineArgs") == 0) {
        return "cli";
    }
    if (strcmp(name, "systemEnvironment") == 0) {
        return "env";
    }
    if (strncmp(name, "Config resource", 15) == 0 ||
        strncmp(name, "applicationConfig", 17) == 0) {
        return "file";
    }
    return "spring";
}

static cf_bool spring_read_all(cf_arena* ar, cf_blob* blob) {
    size_t cap = 1 << 16, n = 0;
    char*  buf = (char*)cf_arena_alloc(ar, cap);

    while (buf != NULL) {
        size_t got = fread(buf + n, 1, cap - n, stdin);
        n += got;
        if (n < cap) {
            break;
        }
        {
            char* bigger = (char*)cf_arena_alloc(ar, cap * 2);
            if (bigger != NULL) {
                memcpy(bigger, buf, n);
            }
            buf = bigger;
            cap *= 2;
        }
    }
    if (buf == NULL) {
        return CF_FALSE;
    }
    blob->p    = buf;
    blob->n    = n;
    blob->name = "stdin";
    return CF_TRUE;
}

static int spring_usage(void) {
    fputs("doctor import-spring [--profile <name>]\n"
          "\n"
          "Reads Spring Boot's /actuator/env JSON on stdin and writes a\n"
          "provenance report on stdout. What Spring masked is recorded as\n"
          "sensitive; the profile is Spring's first active one unless one\n"
          "is given.\n",
          stderr);
    return 2;
}

int doctor_import_spring(int argc, char** argv) {
    static spring_scan      s;
    static cf_source        src[SPRING_SOURCES];
    static cf_source_vtable vts[SPRING_SOURCES];
    const cf_source*        view[SPRING_SOURCES];
    const cf_codec*         json = cf_codec_by_name("json");
    cf_source_view          sv;
    cf_report_info          info;
    cf_doc_sink             sink;
    cf_blob                 blob;
    cf_error                err     = CF_ERROR_INIT;
    const char*             profile = NULL;
    size_t                  i, n;
    char*                   out;
    int                     k;

    memset(&s, 0, sizeof s);
    for (k = 0; k < argc; ++k) {
        if (strcmp(argv[k], "--profile") == 0 && k + 1 < argc) {
            profile = argv[++k];
        } else {
            return spring_usage();
        }
    }
    s.ar     = cf_arena_new(NULL, 1 << 16);
    s.source = -1;
    if (s.ar == NULL || json == NULL || !cf_ks_init(&s.ks, s.ar, 256) ||
        !cf_defset_init(&s.ds, s.ar, 64)) {
        fprintf(stderr, "doctor: cannot start\n");
        cf_arena_free(s.ar);
        return 3;
    }
    if (!spring_read_all(s.ar, &blob)) {
        fprintf(stderr, "doctor: out of memory reading stdin\n");
        cf_arena_free(s.ar);
        return 3;
    }
    sink.size        = sizeof(cf_doc_sink);
    sink.begin_table = spring_table;
    sink.begin_array = spring_array;
    sink.scalar      = spring_scalar;
    sink.end         = spring_end;
    sink.ctx         = &s;
    if (!json->decode(&blob, &sink, &err) || s.failed) {
        fprintf(stderr, "doctor: %s\n",
                (err.message[0] != '\0') ? err.message
                                         : "stdin is not /actuator/env output");
        cf_arena_free(s.ar);
        return 2;
    }
    if (s.nsources == 0) {
        fprintf(stderr, "doctor: stdin has no propertySources; is it /actuator/env?\n");
        cf_arena_free(s.ar);
        return 2;
    }
    if (!cf_ks_resolve(&s.ks)) {
        fprintf(stderr, "doctor: out of memory resolving\n");
        cf_arena_free(s.ar);
        return 3;
    }
    cf_defset_seal(&s.ds);

    /* The sources as Spring listed them, a kind for each. */
    for (i = 0; i < s.nsources; ++i) {
        memset(&vts[i], 0, sizeof vts[i]);
        vts[i].kind = spring_kind(s.names[i]);
        memset(&src[i], 0, sizeof src[i]);
        src[i].vt   = &vts[i];
        src[i].name = (s.names[i] != NULL) ? s.names[i] : "unnamed";
        src[i].rank = (int)i * 10;
        view[i]     = &src[i];
    }
    sv.items = view;
    sv.count = s.nsources;

    memset(&info, 0, sizeof info);
    info.mode        = CF_REPORT_BLIND; /* what Spring masked stays masked */
    info.profile     = (profile != NULL)        ? profile
                       : (s.profile[0] != '\0') ? s.profile
                                                : NULL;
    info.application = "spring";

    n   = cf_report_write(&s.ks, &s.ds, &sv, &info, NULL, 0);
    out = (char*)cf_arena_alloc(s.ar, n + 1);
    if (out == NULL) {
        fprintf(stderr, "doctor: out of memory writing\n");
        cf_arena_free(s.ar);
        return 3;
    }
    cf_report_write(&s.ks, &s.ds, &sv, &info, out, n + 1);
    fwrite(out, 1, n, stdout);
    if (s.skipped > 0) {
        fprintf(stderr,
                "doctor: %lu key(s) left out because a report key cannot carry "
                "them, e.g. %s\n",
                s.skipped, s.first_skipped);
    }
    cf_arena_free(s.ar);
    return 0;
}
