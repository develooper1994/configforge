/*
 * mistakes.c - what the person running a program sees when its
 * configuration is wrong.
 *
 * Sooner or later every program is started with a bad configuration: a
 * key spelt wrong, a port of 99999, a log level nobody defined, a
 * required setting forgotten. What decides whether that costs a minute
 * or an afternoon is the message. Does it name the key? The file and
 * the line? What would have been right?
 *
 * This program declares one contract and builds it from five files,
 * each with one mistake, printing exactly what cf_builder_create says.
 * Then it shows the one kind of mistake a contract can be told to
 * forgive, and how explain() still shows what was forgiven.
 *
 *     ./mistakes
 *
 * The suite checks every message below, so they are the library's
 * words, not this comment's.
 */

#include <configforge/configforge.h>

#include <stdio.h>

#define FILE_NAME "mistakes.ini"

/* The contract: what this program accepts. Everything the messages
   below can say comes from here - the library can only say "did you
   mean server.port" because the program told it server.port exists. */
static void contract(cf_builder* b) {
    static const char* const levels[] = {"debug", "info", "warn", "error", NULL};
    cf_def                   d;

    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);

    /* An enum: a text value with a closed list of spellings. */
    cf_def_init(&d, CF_TYPE_TEXT, "log.level");
    d.allowed     = levels;
    d.has_default = CF_TRUE;
    cf_value_text(&d.default_value, "info", 4);
    cf_builder_define(b, &d);

    /* Required: no default, because no default is right. A database
       address that silently became "localhost" in production is worse
       than a program that refuses to start. */
    cf_builder_text(b, "db.url", NULL, CF_REQUIRED);

    /* Unknown keys are refused rather than ignored: a typo is a setting
       that silently does nothing, which is the worst kind. */
    cf_builder_unknown(b, CF_UNKNOWN_REJECT);
}

/* Build the contract from a file holding `text`, and say what happened. */
static void attempt(const char* title, const char* text) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  cfg;
    FILE*       f = fopen(FILE_NAME, "wb");

    if (f != NULL) {
        fputs(text, f);
        fclose(f);
    }
    cf_builder_file(b, "file", CF_RANK_USER_FILE, FILE_NAME, NULL, 0);
    contract(b);
    cfg = cf_builder_create(b, &err);

    printf("%s\n", title);
    if (cfg == NULL) {
        /* One message, built once, naming everything it knows. */
        printf("  refused: %s\n\n", err.message);
        return;
    }
    printf("  started: port %d, log level %s\n\n",
           (int)cf_get_int(cfg, "server.port", 0), cf_get_text(cfg, "log.level", "?"));
    cf_config_free(cfg);
}

/* The forgiving kind. A cache size is a tuning knob: if an operator
   writes nonsense, running with the default is better than not running.
   CF_INVALID_FALLBACK says so for this one key - and only this one, so
   that the production database address can never quietly degrade the
   same way. The refusal is not hidden: explain() shows it. */
static void forgiving(void) {
    static char why[1024];
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  cfg;
    cf_def      d;
    FILE*       f = fopen(FILE_NAME, "wb");

    if (f != NULL) {
        fputs("[cache]\nsize = lots\n", f);
        fclose(f);
    }
    cf_builder_file(b, "file", CF_RANK_USER_FILE, FILE_NAME, NULL, 0);
    cf_def_init(&d, CF_TYPE_INT, "cache.size");
    d.has_default = CF_TRUE;
    cf_value_int(&d.default_value, 64);
    d.on_invalid = CF_INVALID_FALLBACK;
    cf_builder_define(b, &d);
    cfg = cf_builder_create(b, &err);

    printf("6. a value the contract may forgive\n");
    if (cfg == NULL) {
        printf("  refused: %s\n", err.message);
        return;
    }
    printf("  started: cache.size %d\n", (int)cf_get_int(cfg, "cache.size", 0));
    cf_explain(cfg, "cache.size", why, sizeof why);
    fputs(why, stdout);
    cf_config_free(cfg);
}

int main(void) {
    attempt("1. a configuration with nothing wrong",
            "[server]\nport = 9000\n[db]\nurl = postgres://db/app\n");
    attempt("2. a key spelt wrong",
            "[server]\npot = 9000\n[db]\nurl = postgres://db/app\n");
    attempt("3. a number out of range",
            "[server]\nport = 99999\n[db]\nurl = postgres://db/app\n");
    attempt("4. a word the contract does not know",
            "[log]\nlevel = verbose\n[db]\nurl = postgres://db/app\n");
    attempt("5. a required setting left out", "[server]\nport = 9000\n");
    forgiving();
    remove(FILE_NAME);
    return 0;
}
