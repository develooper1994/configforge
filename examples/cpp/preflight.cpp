/*
 * preflight.cpp - a service that lints its own configuration before it
 * serves, in C++03.
 *
 * preflight.c through the wrapper. Doctor's library is C (cf_doctor.h),
 * and the wrapper's Config hands it the configuration with raw(): there
 * is no second C++ copy of the rules to drift from the first.
 *
 *     ./preflight_cpp
 */

#include <configforge/cf_doctor.h>
#include <configforge/configforge.hpp>

#include <cstdio>

static cf::Config load(int argc, char** argv, char** env)
{
    cf_def password;
    cf_def_init(&password, CF_TYPE_TEXT, "db.password");
    password.flags = CF_SENSITIVE | CF_REQUIRED;

    return cf::Builder()
        .file("service.ini", CF_RANK_SYS_FILE, "service.ini")
        .env("env", CF_RANK_ENV, "SHOP_", env)
        .cli("cli", CF_RANK_CLI, argc, argv)
        .defineInt("server.port", 8080, 1, 65535)
        .defineText("db.host", "localhost")
        .define(password)
        .create();
}

/* Doctor's rules as production judges them; true when it may serve. */
static bool preflight(const cf::Config& cfg)
{
    cf_error   err = CF_ERROR_INIT;
    cf_doctor* doc = cf_doctor_new();

    if (doc == 0 || !cf_doctor_lint(doc, cfg.raw(), "production", &err)) {
        std::printf("preflight: could not run: %s\n", err.message);
        cf_doctor_free(doc);
        return false;
    }
    for (size_t i = 0; i < cf_doctor_count(doc); ++i) {
        const cf_finding* f = cf_doctor_finding(doc, i);
        std::printf("preflight: %s %s %s at %s: %s\n", f->rule, f->level, f->key,
                    f->at != 0 ? f->at : "?", f->message);
    }
    bool clean = cf_doctor_count_at(doc, "error") == 0;
    cf_doctor_free(doc);
    return clean;
}

static void start(const char* title, int argc, char** argv, char** env)
{
    cf::Config cfg = load(argc, argv, env);

    std::printf("%s\n", title);
    if (!cfg) {
        std::printf("  refused by the contract: %s\n\n", cfg.error().message());
    } else if (!preflight(cfg)) {
        std::printf("  refusing to serve\n\n");
    } else {
        std::printf("  preflight clean: serving on :%d, database %s\n\n",
                    cfg.get<int>("server.port", 0), cfg.text("db.host").c_str());
    }
}

int main()
{
    static char  a0[]       = "shop";
    static char  a1[]       = "--db.password=hunter2";
    static char  a2[]       = "--server.port=9443";
    static char* typed[]    = {a0, a1, a2, 0};
    static char* flagless[] = {a0, a2, 0};
    static char  e0[]       = "SHOP_DB__PASSWORD=hunter2";
    static char* secret[]   = {e0, 0};
    static char* none[]     = {0};

    std::FILE* f = std::fopen("service.ini", "wb");
    if (f == 0) {
        return 1;
    }
    std::fputs("[db]\nhost = db.internal\n", f);
    std::fclose(f);

    start("1. the password typed into the manifest as a flag", 3, typed, none);
    start("2. the password from the environment, as the secret store sets it", 2, flagless,
          secret);
    std::remove("service.ini");
    return 0;
}
