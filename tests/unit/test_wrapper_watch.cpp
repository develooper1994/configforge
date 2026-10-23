/*
 * test_wrapper_watch.cpp - change notification through the C++03
 * wrapper.
 *
 * Built as C++98 like the other wrapper consumers. A wrapper that
 * covers half its library is worse than none, and change notification
 * was the half that was missing: Config::changed() and Watcher.
 */

#include <configforge/configforge.hpp>

#include <cstdio>
#include <cstring>

static int  g_fail = 0, g_run = 0;
static void check(bool c, const char* w) {
    ++g_run;
    if (!c) {
        ++g_fail;
        std::printf("FAIL: %s\n", w);
    }
}

static bool spill(const char* path, const char* text) {
    std::FILE* f = std::fopen(path, "wb");
    if (f == 0) {
        return false;
    }
    std::fputs(text, f);
    std::fclose(f);
    return true;
}

/* With a drop-in directory where the build has one: a watcher over a
   file and a directory is the case worth testing, and one over a file
   alone is what is left without CF_WITH_DIR. */
static cf::Config build() {
    cf::Builder b;
    b.file("file", CF_RANK_USER_FILE, "cf_cxx.ini");
#if CF_WITH_DIR
    b.dir("conf.d", CF_RANK_SYS_FILE, "cf_cxx.d", 0, CF_FILE_OPTIONAL);
#endif
    return b.defineInt("server.port", 8080, 1, 65535).create();
}

int main() {
    check(spill("cf_cxx.ini", "[server]\nport = 3000\n"), "wrote a file");
    cf::Config cfg = build();
    check(cfg.valid(), "builds");
    if (!cfg) {
        std::printf("  %s\n", cfg.error().message());
        return 1;
    }
    check(!cfg.changed(), "nothing has changed yet");

    {
        cf::Watcher w(cfg);
        if (!cf_platform_has_wake()) {
            check(!w.valid() && w.error().code() == CF_E_CAPABILITY,
                  "a platform without a handle says so");
        } else {
            check(w.valid(), "a watcher opens");
            check(w.fd() >= 0 || w.handle() != 0, "and has something to wait on");
            for (int i = 0; i < 20 && w.wait(150) != 0; ++i) {
            }
            check(spill("cf_cxx.ini", "[server]\nport = 3000\n# edited\n"),
                  "the file is rewritten");
            check(w.wait(5000) == 1, "which wakes it");
        }
    }
    check(cfg.changed(), "and Config::changed() sees it");

    /* Rebuild and say what the reload did: the line a program that
       watches its files would log. */
    check(spill("cf_cxx.ini", "[server]\nport = 4000\n"), "the file changes a value");
    {
        cf::Config next = build();
        check(next.valid(), "it rebuilds");
        std::string what = next.changesSince(cfg);
        check(what.find("server.port: 3000 -> 4000") != std::string::npos,
              "and changesSince() says what the reload did");
        check(cfg.changesSince(cfg).empty(), "nothing changes against itself");
    }

    std::remove("cf_cxx.ini");
    std::printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
