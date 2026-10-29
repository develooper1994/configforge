/*
 * references.cpp - one value made from others, ${key} (D-10), in C++03.
 *
 * references.c through the wrapper: a URL written once from the host and
 * port, a flag that moves it, explain() saying what it was made from,
 * and a cycle refused by name.
 *
 *     ./references_cpp
 */

#include <configforge/configforge.hpp>

#include <cstdio>

static const char* const kFile = "app.ini";

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

static cf::Config load(int argc, char** argv)
{
    return cf::Builder()
        .file("app.ini", CF_RANK_USER_FILE, kFile)
        .cli("cli", CF_RANK_CLI, argc, argv)
        .defineText("server.host", "localhost")
        .defineInt("server.port", 8080, 1, 65535)
        .defineText("server.url")
        .create();
}

int main()
{
    static char  a0[]   = "references";
    static char  a1[]   = "--server.host=web2";
    static char* argv[] = {a0, a1, 0};

    if (!writeFile("[server]\nhost = web1\nport = 9443\n"
                   "url = https://${server.host}:${server.port}/\n")) {
        return 1;
    }
    cf::Config cfg = load(1, argv);
    if (!cfg) {
        std::fprintf(stderr, "references: %s\n", cfg.error().message());
        return 1;
    }
    std::printf("url: %s\n", cfg.text("server.url").c_str());
    std::fputs(cfg.explain("server.url").c_str(), stdout);

    cf::Config flagged = load(2, argv);
    std::printf("\nwith --server.host=web2, url: %s\n", flagged.text("server.url").c_str());

    writeFile("[server]\nhost = ${server.url}\nurl = https://${server.host}/\n");
    cf::Config cycle = load(1, argv);
    std::printf("\na cycle: %s\n", !cycle ? cycle.error().message() : "accepted");

    std::remove(kFile);
    return 0;
}
