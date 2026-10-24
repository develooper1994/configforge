/*
 * doctor.c - ConfigForge Doctor, the command line.
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
 *     doctor lint    <report> [options]
 *     doctor diff    <a> <b>  [options]
 *
 *     --baseline <file>   findings already accepted, one per line
 *     --severity <file>   this project's severities, one per line
 *     --fail-on <level>   error (default), warning, info or never
 *     --format <name>     text (default) or sarif
 *
 * The rules are the library's (engine.c, cf_doctor.h); this file reads
 * the command line and prints.
 */

#include "adapters.h"
#include "engine.h"

#include <stdio.h>
#include <string.h>

static int baseline_stale(const cf_doctor* D) {
    size_t      i;
    const char* rule;
    const char* key;

    for (i = 0; cf_doctor_stale(D, i, &rule, &key); ++i) {
        printf("%-7s %-7s %-28s %s\n", rule, "stale", key,
               "in the baseline and no longer found: either it was fixed "
               "or the rule stopped working");
    }
    return (int)i;
}

static void sarif_quote(const char* t) {
    putchar('"');
    for (; *t != '\0'; ++t) {
        unsigned char ch = (unsigned char)*t;
        if (ch == '"' || ch == '\\') {
            putchar('\\');
            putchar((int)ch);
        } else if (ch < 0x20) {
            printf("\\u%04x", (unsigned)ch);
        } else {
            putchar((int)ch);
        }
    }
    putchar('"');
}

static const char* sarif_level(const char* l) {
    return (strcmp(l, "error") == 0)     ? "error"
           : (strcmp(l, "warning") == 0) ? "warning"
                                         : "note";
}

static void print_sarif(const cf_doctor* D, const char* report_path) {
    size_t i;

    printf("{\n  \"$schema\": \"https://json.schemastore.org/sarif-2.1.0.json\",\n");
    printf("  \"version\": \"2.1.0\",\n  \"runs\": [{\n");
    printf("    \"tool\": { \"driver\": { \"name\": \"ConfigForge Doctor\", ");
    printf("\"version\": ");
    sarif_quote(cf_version());
    printf(", \"rules\": [");
    for (i = 0; i < cf_doctor_rule_count(); ++i) {
        printf("%s\n      { \"id\": \"%s\", \"shortDescription\": { \"text\": ",
               (i > 0) ? "," : "", cf_doctor_rule_id(i));
        sarif_quote(cf_doctor_rule_text(cf_doctor_rule_id(i)));
        printf(" } }");
    }
    printf("] } },\n    \"results\": [");
    for (i = 0; i < D->nfound; ++i) {
        const dfinding* f = &D->found[i];
        size_t          plen;
        long            line;
        char            uri[256];

        printf("%s\n      { \"ruleId\": \"%s\", \"level\": \"%s\", \"message\": { "
               "\"text\": ",
               (i > 0) ? "," : "", f->id, sarif_level(f->level));
        {
            char text[CF_KEY_MAX + 520];
            cf_fmt(text, sizeof text, "%s: %s", f->key, f->msg);
            sarif_quote(text);
        }
        printf(" }, \"locations\": [{ \"physicalLocation\": { \"artifactLocation\": { "
               "\"uri\": ");
        /* A file and a line when the value came from one; otherwise the
           report itself, which is still the artifact the finding is
           about. argv[3] is a place, but not one a forge can show. */
        if (doctor_place_of(f->at, &plen, &line) &&
            !(plen == 4 && memcmp(f->at, "argv", 4) == 0) && plen + 1 < sizeof uri) {
            size_t k;
            for (k = 0; k < plen; ++k) {
                /* One cast over the choice: '/' is an int in C. */
                uri[k] = (char)((f->at[k] == '\\') ? '/' : f->at[k]);
            }
            uri[plen] = '\0';
            sarif_quote(uri);
            printf(" }, \"region\": { \"startLine\": %ld } } }] }", line);
        } else {
            sarif_quote(report_path);
            printf(" } } }] }");
        }
    }
    printf("\n    ]\n  }]\n}\n");
}

static void print_text(const cf_doctor* D) {
    size_t i;
    for (i = 0; i < D->nfound; ++i) {
        printf("%-7s %-7s %-28s %s\n", D->found[i].id, D->found[i].level,
               D->found[i].key, D->found[i].msg);
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
        doctor_key_name(k, name, sizeof name);

        printf("%s", name);
        if (k->type != NULL) {
            printf("  (%s)", k->type);
        }
        if (k->sensitive) {
            printf("  [sensitive]");
        }
        printf("\n");
        for (j = 0; j < k->nc; ++j) {
            printf("  %-12s %-22s %s%s%s\n", k->c[j].source, doctor_shown(&k->c[j]),
                   (k->c[j].outcome != NULL) ? k->c[j].outcome : "selected",
                   (k->c[j].at != NULL) ? "   " : "",
                   (k->c[j].at != NULL) ? k->c[j].at : "");
        }
    }
}

static int cmd_explain(const report* r, const char* key) {
    const rkey* k = doctor_find_key(r, key, strlen(key));
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
        printf("  %-12s %-22s %s%s%s%s%s\n", k->c[j].source, doctor_shown(&k->c[j]),
               (k->c[j].outcome != NULL) ? k->c[j].outcome : "selected",
               (k->c[j].at != NULL) ? "   " : "",
               (k->c[j].at != NULL) ? k->c[j].at : "",
               (k->c[j].reason != NULL) ? "   " : "",
               (k->c[j].reason != NULL) ? k->c[j].reason : "");
    }
    return 0;
}

static int usage(void) {
    /* Split across several calls: C90 requires support for string
       literals only up to 509 characters, and one long one is the sort
       of limit that is invisible until a toolchain enforces it. */
    fputs("ConfigForge Doctor\n"
          "\n"
          "  doctor dump    <report>\n"
          "  doctor explain <report> <key>\n"
          "  doctor lint    <report> [options]\n"
          "  doctor diff    <report-a> <report-b> [options]\n"
          "  doctor --version\n"
          "\n",
          stderr);
    fputs("  doctor import-git [--full]     < git config output\n"
          "  doctor import-spring           < /actuator/env JSON\n"
          "  doctor import-dotnet [--full]  < GetDebugView() text\n"
          "\n"
          "  --baseline <file>  findings already accepted\n"
          "  --severity <file>  this project's severities\n"
          "  --fail-on <level>  error (default), warning, info, never\n"
          "  --format <name>    text (default) or sarif\n"
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
          "\n",
          stderr);
    fputs("A severity file says where a project disagrees with a rule's\n"
          "default, one per line: \"CFD010 warning\", \"CFD008 error\n"
          "production\" for one profile, \"CFD011 off\".\n"
          "\n"
          "Exit codes: 0 nothing at the --fail-on level, 1 something at\n"
          "it, 2 unusable input.\n",
          stderr);
    return 2;
}

/* A report, or the reason it is not one. */
static cf_bool load(report* r, cf_doctor* D, const char* path) {
    cf_error err = CF_ERROR_INIT;
    if (!doctor_load(r, D, path, &err)) {
        fprintf(stderr, "doctor: %s\n", err.message);
        return CF_FALSE;
    }
    return CF_TRUE;
}

static int run(cf_doctor* D, int argc, char** argv);

int main(int argc, char** argv) {
    cf_doctor* D;
    int        rc;

    /* An adapter writes a report rather than reading one, so it takes no
       report argument and is dispatched before anything expects one. */
    if (argc >= 2 && strcmp(argv[1], "import-git") == 0) {
        return doctor_import_git(argc - 2, argv + 2);
    }
    if (argc >= 2 && strcmp(argv[1], "import-spring") == 0) {
        return doctor_import_spring(argc - 2, argv + 2);
    }
    if (argc >= 2 && strcmp(argv[1], "import-dotnet") == 0) {
        return doctor_import_dotnet(argc - 2, argv + 2);
    }
    /* The version, and the commit it was built from when the build knew
       it: two Doctors of one version can still differ. */
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        const char* id = cf_build_id();
        printf("ConfigForge Doctor %s%s%s%s\n", cf_version(),
               (id[0] != '\0') ? " (" : "", id, (id[0] != '\0') ? ")" : "");
        return 0;
    }
    if (argc < 3) {
        return usage();
    }
    D = cf_doctor_new();
    if (D == NULL) {
        fprintf(stderr, "doctor: cannot start\n");
        return 2;
    }
    rc = run(D, argc, argv);
    cf_doctor_free(D);
    return rc;
}

static int run(cf_doctor* D, int argc, char** argv) {
    report a, b;

    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);

    if (strcmp(argv[1], "dump") == 0) {
        if (!load(&a, D, argv[2])) {
            doctor_release(&a);
            return 2;
        }
        cmd_dump(&a);
        doctor_release(&a);
        return 0;
    }
    if (strcmp(argv[1], "explain") == 0) {
        int rc;
        if (argc < 4) {
            return usage();
        }
        if (!load(&a, D, argv[2])) {
            doctor_release(&a);
            return 2;
        }
        rc = cmd_explain(&a, argv[3]);
        doctor_release(&a);
        return rc;
    }
    if (strcmp(argv[1], "lint") == 0 || strcmp(argv[1], "diff") == 0) {
        cf_bool     is_diff = (strcmp(argv[1], "diff") == 0) ? CF_TRUE : CF_FALSE;
        int         first   = is_diff ? 4 : 3;
        const char* fail_on = "error";
        const char* format  = "text";
        int         i;
        int         over = 0;
        int         threshold;

        if (is_diff && argc < 4) {
            return usage();
        }
        /* Options read before anything runs, so a finding is matched
           as it is produced rather than filtered afterwards. */
        for (i = first; i < argc; ++i) {
            if (i + 1 >= argc) {
                return usage();
            }
            if (strcmp(argv[i], "--baseline") == 0) {
                cf_doctor_baseline(D, argv[++i], NULL);
            } else if (strcmp(argv[i], "--severity") == 0) {
                cf_error err = CF_ERROR_INIT;
                if (!cf_doctor_severity_file(D, argv[++i], &err)) {
                    fprintf(stderr, "doctor: %s\n", err.message);
                    return 2;
                }
            } else if (strcmp(argv[i], "--fail-on") == 0) {
                fail_on = argv[++i];
                if (strcmp(fail_on, "never") != 0 && !doctor_level_known(fail_on)) {
                    return usage();
                }
            } else if (strcmp(argv[i], "--format") == 0) {
                format = argv[++i];
                if (strcmp(format, "text") != 0 && strcmp(format, "sarif") != 0) {
                    return usage();
                }
            } else {
                return usage();
            }
        }
        threshold = doctor_level_rank(fail_on);

        if (!load(&a, D, argv[2]) || (is_diff && !load(&b, D, argv[3]))) {
            doctor_release(&a);
            doctor_release(&b);
            return 2;
        }
        if (is_diff) {
            doctor_diff_reports(&a, &b);
        } else {
            doctor_lint_reports(&a);
        }

        if (strcmp(format, "sarif") == 0) {
            print_sarif(D, argv[2]);
        } else {
            int stale = 0;
            print_text(D);
            if (D->base.loaded) {
                stale = baseline_stale(D);
            }
            printf("\n%d new finding(s): %d error, %d warning, %d info", D->findings,
                   D->errors, D->warnings, D->infos);
            if (D->baselined > 0) {
                printf("; %d accepted by the baseline", D->baselined);
            }
            if (stale > 0) {
                printf("; %d baseline entry(s) no longer match", stale);
            }
            printf("\n");
        }
        doctor_release(&a);
        doctor_release(&b);

        /* A stale entry does not fail the build. It says the file has
           drifted, and failing on it would make people stop keeping the
           file rather than start pruning it. */
        for (i = 0; (size_t)i < D->nfound; ++i) {
            if (threshold > 0 && doctor_level_rank(D->found[i].level) >= threshold) {
                ++over;
            }
        }
        return (over > 0) ? 1 : 0;
    }
    return usage();
}
