/*
 * layers.cpp - where a value comes from, with every kind of source at
 * once, in C++03.
 *
 * layers.c through the wrapper, reading the same files: shipped
 * defaults, conf.d, a .env, the environment and the command line, each
 * above the one before, and then why each value won. The files are in
 * examples/c/layers/, shared by both programs; the suite runs this
 * there.
 *
 *     cd examples/c/layers && ../../../build/examples/cpp/layers_cpp
 */

#include <configforge/configforge.hpp>

#include <cstdio>

int main()
{
    /* Fixed rather than the process's own, so the output is the same on
       every machine. */
    static char  e0[]   = "LAYERS_LOG__LEVEL=warn";
    static char* env[]  = {e0, 0};
    static char  a0[]   = "layers";
    static char  a1[]   = "--server.port=9443";
    static char* argv[] = {a0, a1, 0};

    static const char* const keys[] = {"server.host", "server.port", "server.workers",
                                       "db.url",      "log.level",   "cache.size",
                                       0};

    /* The sources, highest rank number lowest precedence, and the
       contract, in one block. */
    cf::Config cfg = cf::Builder()
                         .file("app.toml", 35, "app.toml")
                         .dir("conf.d", 30, "conf.d", 0, CF_OPTIONAL)
                         .dotenv(".env", 15, ".env", "LAYERS_", CF_OPTIONAL)
                         .env("env", CF_RANK_ENV, "LAYERS_", env)
                         .cli("cli", CF_RANK_CLI, 2, argv)
                         .defineText("server.host", "127.0.0.1")
                         .defineInt("server.port", 8080, 1, 65535)
                         .defineInt("server.workers", 2, 1, 64)
                         .defineText("db.url", "sqlite:dev.db", CF_SENSITIVE)
                         .defineText("log.level", "info")
                         .defineInt("cache.size", 64, 1, 4096)
                         .unknown(CF_UNKNOWN_REJECT)
                         .create();

    if (!cfg) {
        std::fprintf(stderr, "layers: %s\n", cfg.error().message());
        return 1;
    }
    /* db.url is sensitive: explain() says where it was set, never what. */
    for (size_t i = 0; keys[i] != 0; ++i) {
        std::fputs(cfg.explain(keys[i]).c_str(), stdout);
        std::fputs("\n", stdout);
    }
    return 0;
}
