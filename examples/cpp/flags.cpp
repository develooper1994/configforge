/*
 * flags.cpp - feature flags, rolled out by host and killed in an
 * incident, in C++03.
 *
 * flags.c through the wrapper, reading the same files: every feature
 * shipped off, a canary drop-in turning two on, an environment variable
 * killing one - and the flags discovered by iterating an open prefix,
 * not listed in the program.
 *
 *     cd examples/c/flags && ../../../build/examples/cpp/flags_cpp
 */

#include <configforge/configforge.hpp>

#include <cstdio>
#include <string>

int main()
{
    static char  kill[] = "SHOP_FEATURES__SEARCH_V2=false";
    static char* env[]  = {kill, 0};

    cf::Config cfg = cf::Builder()
                         .file("features.ini", CF_RANK_SYS_FILE, "features.ini")
                         .dir("conf.d", CF_RANK_USER_FILE, "conf.d", 0, CF_FILE_OPTIONAL)
                         .env("env", CF_RANK_ENV, "SHOP_", env)
                         .open("features") /* discovered, not declared */
                         .unknown(CF_UNKNOWN_REJECT)
                         .create();
    if (!cfg) {
        std::fprintf(stderr, "flags: %s\n", cfg.error().message());
        return 1;
    }
    cf::Config::Range r = cfg.children("features");
    for (cf::Config::Range::iterator i = r.begin(); i != r.end(); ++i) {
        std::string key = "features." + (*i).name();
        std::printf("%-14s %s\n", (*i).name().c_str(),
                    cfg.get<bool>(key.c_str(), false) ? "on" : "off");
    }
    std::printf("\n");
    std::fputs(cfg.explain("features.search_v2").c_str(), stdout);
    return 0;
}
