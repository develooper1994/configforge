/*
 * mistakes.cpp - what the person running a program sees when its
 * configuration is wrong, in C++03.
 *
 * mistakes.c through the wrapper: one contract, five files with one
 * mistake each, and the message create() leaves in the handle. Then the
 * one mistake a contract can be told to forgive, and explain() showing
 * what was forgiven.
 *
 *     ./mistakes_cpp
 *
 * The suite checks every message, so they are the library's words.
 */

#include <configforge/configforge.hpp>

#include <cstdio>

static const char* const kFile = "mistakes.ini";

static bool writeFile(const char* text)
{
    std::FILE* f = std::fopen(kFile, "wb");
    if (f == 0) {
        return false;
    }
    std::fputs(text, f);
    std::fclose(f);
    return true;
}

/* The contract. The library can say "did you mean server.port" only
   because the program said server.port exists. */
static cf::Config build()
{
    static const char* const levels[] = {"debug", "info", "warn", "error", 0};
    cf_def                   level;

    /* An enum is a full definition: the shorthands do not say one. */
    cf_def_init(&level, CF_TYPE_TEXT, "log.level");
    level.allowed     = levels;
    level.has_default = CF_TRUE;
    cf_value_text(&level.default_value, "info", 4);

    return cf::Builder()
        .file("file", CF_RANK_USER_FILE, kFile)
        .defineInt("server.port", 8080, 1, 65535)
        .define(level)
        .defineText("db.url", 0, CF_REQUIRED) /* no default is right */
        .unknown(CF_UNKNOWN_REJECT)           /* a typo is not ignored */
        .create();
}

static void attempt(const char* title, const char* text)
{
    writeFile(text);
    cf::Config cfg = build();

    std::printf("%s\n", title);
    if (!cfg) {
        std::printf("  refused: %s\n\n", cfg.error().message());
        return;
    }
    std::printf("  started: port %d, log level %s\n\n", cfg.get<int>("server.port", 0),
                cfg.text("log.level", "?").c_str());
}

/* A tuning knob may fall back to its default when it is nonsense - this
   key only - and explain() still shows the refusal. */
static void forgiving()
{
    cf_def size;

    writeFile("[cache]\nsize = lots\n");
    cf_def_init(&size, CF_TYPE_INT, "cache.size");
    size.has_default = CF_TRUE;
    cf_value_int(&size.default_value, 64);
    size.on_invalid = CF_INVALID_FALLBACK;

    cf::Config cfg =
        cf::Builder().file("file", CF_RANK_USER_FILE, kFile).define(size).create();

    std::printf("6. a value the contract may forgive\n");
    if (!cfg) {
        std::printf("  refused: %s\n", cfg.error().message());
        return;
    }
    std::printf("  started: cache.size %d\n", cfg.get<int>("cache.size", 0));
    std::fputs(cfg.explain("cache.size").c_str(), stdout);
}

int main()
{
    attempt("1. a configuration with nothing wrong",
            "[server]\nport = 9000\n[db]\nurl = postgres://db/app\n");
    attempt("2. a key spelt wrong", "[server]\npot = 9000\n[db]\nurl = postgres://db/app\n");
    attempt("3. a number out of range",
            "[server]\nport = 99999\n[db]\nurl = postgres://db/app\n");
    attempt("4. a word the contract does not know",
            "[log]\nlevel = verbose\n[db]\nurl = postgres://db/app\n");
    attempt("5. a required setting left out", "[server]\nport = 9000\n");
    forgiving();
    std::remove(kFile);
    return 0;
}
