/*
 * tier1.cpp - the same program in C++03.
 *
 * Compiled with -std=c++98 -pedantic, so it is the proof that the
 * wrapper really is C++03 rather than nearly C++03.
 */

#include <configforge/configforge.hpp>

#include <cstdio>
#include <string>

int main(int argc, char** argv)
{
    cf::Config cfg = cf::Builder("myapp", argc, argv)
                        .defineInt ("server.port", 8080, 1, 65535)
                        .defineText("server.host", "0.0.0.0")
                        .defineBool("tls.enabled", false)
                        .defineText("cf.explain")
                        .unknown(CF_UNKNOWN_REJECT)
                        .create();

    if (!cfg) {
        std::fprintf(stderr, "configuration error (%s): %s\n",
                     cfg.error().name(), cfg.error().message());
        if (cfg.error().source()[0] != '\0') {
            std::fprintf(stderr, "  set at: %s\n", cfg.error().source());
        }
        return 1;
    }

    if (cfg.has("cf.explain")) {
        std::fputs(cfg.explain(cfg.text("cf.explain").c_str()).c_str(), stdout);
        return 0;
    }

    std::printf("%s:%d tls=%s\n",
                cfg.text("server.host").c_str(),
                cfg.get<int>("server.port", 8080),
                cfg.get<bool>("tls.enabled", false) ? "on" : "off");
    std::fputs(cfg.inspect().c_str(), stdout);
    return 0;
}
