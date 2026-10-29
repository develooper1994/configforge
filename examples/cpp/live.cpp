/*
 * live.cpp - live.c through the wrapper: a gateway that reloads itself
 * under the mode its operators chose, in C++03 (cf::Live).
 *
 *     ./live_cpp
 */

#include <configforge/configforge.hpp>

#include <cstdio>

static const char* const kFile = "gateway.ini";

static cf_builder* build(void*)
{
    return cf::Builder()
        .file("gateway.ini", CF_RANK_USER_FILE, kFile)
        .defineInt("server.port", 8080, 1, 65535, CF_RESTART)
        .defineText("log.level", "info")
        .defineInt("limit.per_minute", 600, 1, 100000)
        .release();
}

static bool write_file(const char* text)
{
    std::FILE* f = std::fopen(kFile, "wb");
    if (f == 0) {
        return false;
    }
    std::fputs(text, f);
    std::fclose(f);
    return true;
}

static void serve(const cf::Live& live)
{
    cf::Config cfg = live.get(); /* one request, one snapshot */
    std::printf("serving on :%d, log %s, %d requests a minute\n",
                cfg["server.port"].as<int>(), cfg.text("log.level").c_str(),
                cfg["limit"]["per_minute"].as<int>());
}

int main()
{
    if (!write_file("[server]\nport = 8080\n")) {
        return 1;
    }

    /* 1. AUTO */
    {
        cf::Live live(CF_LIVE_AUTO, build);
        if (!live.valid()) {
            std::fprintf(stderr, "live: %s\n", live.error().message());
            return 1;
        }
        serve(live);
        write_file("[server]\nport = 9443\n[log]\nlevel = debug\n");
        std::printf("1. reload: %s\n",
                    live.wait(2000) == CF_LIVE_SWAPPED ? "swapped" : "not swapped");
        serve(live);
        std::printf("waiting for a restart: %s", live.restarts().c_str());
        std::printf("%s", live.get().explain("server.port").c_str());
    }

    /* 2. APPROVE */
    write_file("[server]\nport = 8080\n");
    cf::Live live(CF_LIVE_APPROVE, build);
    if (!live.valid()) {
        std::fprintf(stderr, "live: %s\n", live.error().message());
        return 1;
    }
    write_file("[server]\nport = 8080\n[limit]\nper_minute = 60\n");
    if (live.wait(2000) == CF_LIVE_PENDING) {
        std::printf("2. waiting for approval:\n%s",
                    live.pending().changesSince(live.get()).c_str());
        serve(live);
        std::printf("operator: approved\n");
        live.approve();
    }
    serve(live);

    /* 3. A broken edit */
    write_file("[server]\nport = 8080\n[limit]\nper_minute = lots\n");
    cf::Error err;
    cf_live_result r = live.wait(2000, &err);
    std::printf("3. reload %s: %s\n", r == CF_LIVE_FAILED ? "refused" : "accepted?",
                err.message());
    serve(live);

    std::remove(kFile);
    return 0;
}
