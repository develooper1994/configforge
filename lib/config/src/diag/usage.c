/*
 * usage.c - the --help text, from the contract.
 *
 * A definition already holds what a --help line says: the flags (its
 * aliases and its key), the type or the permitted spellings, the
 * description, the default, the range and the environment name. A
 * program that writes its --help by hand keeps a second list of the
 * same keys, which drifts. This writes it from the first.
 *
 * The layout is one block a definition, in the order the set holds them
 * (by key); the exact text is in docs/plan/N-next.md, card N-6, and
 * held by test_help. Nothing here wraps a line or asks the terminal.
 * Measure-or-write through cf_sink, as cf_schema and cf_explain are.
 */

#include "configforge/configforge.h"
#include "configforge/cf_build_options.h"
#include "configforge/cf_define.h"
#include "configforge/cf_entry.h"
#include "configforge/cf_port.h"
#include "configforge/cf_value.h"

#include <string.h>

#define USAGE_INDENT "        "
#define USAGE_ENV_MAX 256

/* One line of details: its indent before the first item, "; " before
   each of the others. */
typedef struct usage_items {
    cf_sink* out;
    cf_bool  any;
} usage_items;

static void item(usage_items* u, const char* text) {
    cf_sink_puts(u->out, u->any ? "; " : USAGE_INDENT);
    u->any = CF_TRUE;
    cf_sink_puts(u->out, text);
}

/* A value as the contract would write it in a file: text in double
   quotes (no escaping), everything else as cf_value_to_text. */
static void put_value(cf_sink* out, const cf_value* v) {
    char tmp[CF_VALUE_TEXT_MAX + 1];

    if (v->type == CF_TYPE_TEXT) {
        cf_sink_putc(out, '"');
        cf_sink_write(out, v->u.t.p, v->u.t.n);
        cf_sink_putc(out, '"');
        return;
    }
    if (cf_value_to_text(v, tmp, sizeof tmp) > 0) {
        cf_sink_puts(out, tmp);
    }
}

static void put_range(usage_items* u, const cf_def* d) {
    cf_value lo, hi;

    if (!d->has_range) {
        return;
    }
    if (d->type == CF_TYPE_INT) {
        cf_value_int(&lo, d->imin);
        cf_value_int(&hi, d->imax);
#if CF_WITH_REAL
    } else if (d->type == CF_TYPE_REAL) {
        cf_value_real(&lo, d->rmin);
        cf_value_real(&hi, d->rmax);
#endif
    } else {
        return;
    }
    item(u, "range ");
    put_value(u->out, &lo);
    cf_sink_puts(u->out, "..");
    put_value(u->out, &hi);
}

static void put_flags(cf_sink* out, const cf_def* d) {
    size_t i;

    cf_sink_puts(out, "  ");
    for (i = 0; d->aliases != NULL && d->aliases[i] != NULL; ++i) {
        cf_sink_puts(out, (strlen(d->aliases[i]) == 1) ? "-" : "--");
        cf_sink_puts(out, d->aliases[i]);
        cf_sink_puts(out, ", ");
    }
    cf_sink_puts(out, "--");
    cf_sink_puts(out, d->key);
    if (d->type != CF_TYPE_BOOL) {
        cf_sink_puts(out, " <");
        if (d->allowed != NULL && d->allowed[0] != NULL) {
            for (i = 0; d->allowed[i] != NULL; ++i) {
                if (i > 0) {
                    cf_sink_putc(out, '|');
                }
                cf_sink_puts(out, d->allowed[i]);
            }
        } else {
            cf_sink_puts(out, cf_type_name(d->type));
        }
        cf_sink_putc(out, '>');
        if (d->list_mode != CF_LIST_NONE) {
            cf_sink_puts(out, "...");
        }
    }
    cf_sink_putc(out, '\n');
}

static void put_description(cf_sink* out, const char* text) {
    if (text == NULL || text[0] == '\0') {
        return;
    }
    cf_sink_puts(out, USAGE_INDENT);
    for (; *text != '\0'; ++text) {
        cf_sink_putc(out, *text);
        if (*text == '\n') {
            cf_sink_puts(out, USAGE_INDENT);
        }
    }
    cf_sink_putc(out, '\n');
}

static void put_details(cf_sink* out, const cf_def* d, const cf_keymap* env) {
    usage_items u;
    char        name[USAGE_ENV_MAX];

    u.out = out;
    u.any = CF_FALSE;
    if ((d->flags & CF_REQUIRED) != 0) {
        item(&u, "required");
    }
    if (d->has_default && d->list_mode == CF_LIST_NONE &&
        (d->flags & CF_SENSITIVE) == 0) {
        item(&u, "default ");
        put_value(out, &d->default_value);
    }
    put_range(&u, d);
    if (d->type == CF_TYPE_TEXT && d->max_len != 0) {
        cf_value lo, hi;

        cf_value_int(&lo, (cf_i64)d->min_len);
        cf_value_int(&hi, (cf_i64)d->max_len);
        item(&u, "length ");
        put_value(out, &lo);
        cf_sink_puts(out, "..");
        put_value(out, &hi);
    }
    if (d->list_mode != CF_LIST_NONE) {
        item(&u, "repeatable");
    }
    if ((d->flags & CF_DEPRECATED) != 0) {
        if (d->replaced_by != NULL) {
            item(&u, "deprecated, use --");
            cf_sink_puts(out, d->replaced_by);
        } else {
            item(&u, "deprecated");
        }
    }
    if (env != NULL && cf_keymap_from_key(env, d->key, strlen(d->key), CF_FALSE, name,
                                          sizeof name) > 0) {
        item(&u, "env ");
        cf_sink_puts(out, name);
    }
    if ((d->flags & CF_SENSITIVE) != 0) {
        item(&u, "sensitive");
    }
    if (u.any) {
        cf_sink_putc(out, '\n');
    }
}

size_t cf_usage(const cf_config* c, const char* program, const char* env_prefix,
                char* buf, size_t cap) {
    const cf_defset* ds;
    cf_keymap        map;
    cf_sink          out;
    size_t           i, n;

    if (c == NULL) {
        if (buf != NULL && cap > 0) {
            buf[0] = '\0';
        }
        return 0;
    }
    ds = cf_defs(c);
    n  = cf_defset_count(ds);

    cf_sink_init(&out, buf, cap);
    cf_sink_puts(&out, "Usage: ");
    cf_sink_puts(&out, (program != NULL) ? program : "program");
    cf_sink_puts(&out, " [options]\n");
    if (n > 0) {
        cf_sink_puts(&out, "\nOptions:\n");
    }
    if (env_prefix != NULL) {
        cf_keymap_init(&map, CF_KEYMAP_ENV, env_prefix);
    }
    for (i = 0; i < n; ++i) {
        const cf_def* d = cf_defset_at(ds, i);

        put_flags(&out, d);
        put_description(&out, d->description);
        put_details(&out, d, (env_prefix != NULL) ? &map : NULL);
    }
    cf_sink_terminate(&out);
    return out.len;
}
