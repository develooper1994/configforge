/*
 * reload.cpp - notice a file changing, rebuild, and say what changed,
 * in C++03.
 *
 * reload.c through the wrapper: a Watcher to wait on, changed() to ask,
 * the same recipe to rebuild, changesSince() for the log line, and an
 * assignment to swap - the old snapshot lives on in any handle still
 * holding it. The program makes the edit itself, to run unattended.
 *
 *     ./reload_cpp
 */

#include <configforge/configforge.hpp>

#include <cstdio>

static const char* const kFile = "reload.toml";

/* The recipe, once at startup and once per reload. */
static cf::Config load()
{
    return cf::Builder()
        .file("file", CF_RANK_USER_FILE, kFile)
        .defineInt("server.port", 8080, 1, 65535)
        .defineInt("pool.size", 4, 1, 256)
        .defineText("db.password", 0, CF_SENSITIVE)
        .create();
}

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

int main()
{
    if (!writeFile("[server]\nport = 8080\n[db]\npassword = \"first\"\n")) {
        return 1;
    }
    cf::Config cfg = load();
    if (!cfg) {
        std::fprintf(stderr, "reload: %s\n", cfg.error().message());
        return 1;
    }
    std::printf("started on port %d\n", cfg.get<int>("server.port", 0));

    {
        /* Not valid where the platform has nothing to wait on; changed()
           works everywhere. */
        cf::Watcher w(cfg);

        if (!writeFile("[server]\nport = 9443\n[pool]\nsize = 16\n"
                       "[db]\npassword = \"second\"\n")) {
            return 1;
        }
        int woke = w.valid() ? w.wait(5000) : 0;
        if (!cfg.changed()) {
            std::fprintf(stderr, "reload: the edit was not noticed (woke=%d)\n", woke);
            return 1;
        }
    }

    cf::Config next = load();
    if (!next) {
        /* A broken edit fails the reload, not the program. */
        std::fprintf(stderr, "reload: keeping the old configuration: %s\n",
                     next.error().message());
    } else {
        std::printf("reloaded:\n%s", next.changesSince(cfg).c_str());
        cfg = next;
    }
    std::printf("now on port %d\n", cfg.get<int>("server.port", 0));
    std::remove(kFile);
    return 0;
}
