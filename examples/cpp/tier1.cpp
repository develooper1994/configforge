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
                        .defineText("cf.explain", 0, CF_TRANSIENT)
                        .defineBool("cf.save", false, CF_TRANSIENT)
                        .open("plugins")
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

    /* Walking a prefix. C++03 has no range-for, so this is begin/end -
       and because it is a real iterator type, a C++11 consumer gets
       range-for over the same interface with no second API. */
    {
        cf::Config::Range r = cfg.children("plugins");
        for (cf::Config::Range::iterator i = r.begin(); i != r.end(); ++i) {
            std::printf("plugin: %s\n", (*i).name().c_str());
        }
    }

    /* What someone configured, in a form that can be saved and read
       back: an overrides file, not a snapshot of every default. */
    if (cfg.has("cf.save")) {
        cf::Error e;
        std::string out = cfg.emit("json", CF_EMIT_CONFIGURED, &e);
        if (out.empty()) {
            std::fprintf(stderr, "%s\n", e.message());
            return 1;
        }
        std::fputs(out.c_str(), stdout);
        return 0;
    }

    std::printf("%s:%d tls=%s\n",
                cfg.text("server.host").c_str(),
                cfg.get<int>("server.port", 8080),
                cfg.get<bool>("tls.enabled", false) ? "on" : "off");
    std::fputs(cfg.inspect().c_str(), stdout);
    return 0;
}
