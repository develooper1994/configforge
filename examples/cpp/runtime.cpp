/*
 * runtime.cpp - changing a setting while the program runs, in C++03.
 *
 * runtime.c through the wrapper. An operator turns the log level up
 * from an admin page:
 *
 *   1. a request in flight keeps the snapshot it started with - a Config
 *      is a counted handle to an immutable snapshot, so copying one is
 *      how a request holds it;
 *   2. the change meets the same contract a file would;
 *   3. explain() says who changed it, and what it was before.
 *
 *     ./runtime_cpp
 */

#include <configforge/configforge.hpp>

#include <cstdio>

int main()
{
    std::FILE* f = std::fopen("service.ini", "wb");
    if (f == 0) {
        return 1;
    }
    std::fputs("[log]\nlevel = info\n[server]\nworkers = 4\n", f);
    std::fclose(f);

    cf::Config live = cf::Builder()
                          .file("service.ini", CF_RANK_SYS_FILE, "service.ini")
                          .defineText("log.level", "warn")
                          .defineInt("server.workers", 1, 1, 64)
                          .create();
    if (!live) {
        std::fprintf(stderr, "runtime: %s\n", live.error().message());
        return 1;
    }
    std::printf("started: log level %s, %d workers\n", live.text("log.level").c_str(),
                live.get<int>("server.workers", 0));

    /* A request takes the configuration as it is now. */
    cf::Config inFlight = live;

    /* The operator's change, as a source of its own named "admin". */
    cf_pair    debug[1] = {{"log.level", "debug"}};
    cf::Config next     = live.derive("admin", debug, 1);
    if (!next) {
        std::fprintf(stderr, "runtime: %s\n", next.error().message());
        return 1;
    }
    std::printf("\nthe request in flight still sees: %s\n", inFlight.text("log.level").c_str());
    std::printf("a new request sees:               %s\n", next.text("log.level").c_str());

    std::printf("\nchanged:\n%s", next.changesSince(live).c_str());
    std::printf("\n%s", next.explain("log.level").c_str());

    /* A mistake typed into the admin page is refused, and the running
       configuration stays as it was. */
    cf_pair    zero[1] = {{"server.workers", "0"}};
    cf::Config refused = next.derive("admin", zero, 1);
    std::printf("\n0 workers: %s\n", !refused ? refused.error().message() : "accepted");
    std::printf("still running with %d workers\n", next.get<int>("server.workers", 0));

    std::remove("service.ini");
    return 0; /* each snapshot goes when its last handle does */
}
