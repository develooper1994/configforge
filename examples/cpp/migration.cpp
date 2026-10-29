/*
 * migration.cpp - renaming a key without breaking anybody's file, in
 * C++03.
 *
 * migration.c through the wrapper: db.host becomes database.host, the
 * old key stays deprecated with replaced_by naming the new one, and a
 * file written for the old version still configures the new.
 *
 *     ./migration_cpp
 */

#include <configforge/configforge.hpp>

#include <cstdio>

static cf::Config load()
{
    cf_def old;
    cf_def_init(&old, CF_TYPE_TEXT, "db.host");
    old.flags       = CF_DEPRECATED;
    old.replaced_by = "database.host";

    return cf::Builder()
        .file("app.ini", CF_RANK_USER_FILE, "app.ini")
        .defineText("database.host", "localhost")
        .defineInt("database.port", 5432, 1, 65535)
        .define(old)
        .unknown(CF_UNKNOWN_REJECT)
        .create();
}

static void run(const char* title, const char* text)
{
    std::FILE* f = std::fopen("app.ini", "wb");
    if (f != 0) {
        std::fputs(text, f);
        std::fclose(f);
    }
    cf::Config cfg = load();
    std::printf("%s\n", title);
    if (!cfg) {
        std::printf("  refused: %s\n\n", cfg.error().message());
        return;
    }
    std::printf("  connecting to %s:%d\n", cfg.text("database.host").c_str(),
                cfg.get<int>("database.port", 0));
    std::fputs(cfg.explain("database.host").c_str(), stdout);
    std::fputs("\n", stdout);
}

int main()
{
    run("1. a file written for version 1", "[db]\nhost = db1.internal\n");
    run("2. a file half updated: the new spelling wins",
        "[database]\nhost = db2.internal\n[db]\nhost = db1.internal\n");
    std::remove("app.ini");
    return 0;
}
