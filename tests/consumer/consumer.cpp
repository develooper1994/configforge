/*
 * consumer.cpp - the same, through the C++03 wrapper.
 */

#include <configforge/configforge.hpp>

#include <cstdio>

int main(int argc, char** argv) {
    cf::Config cfg = cf::Builder("consumer", argc, argv)
                         .defineInt("server.port", 8080, 1, 65535)
                         .open("plugins")
                         .create();
    if (!cfg) {
        std::fprintf(stderr, "consumer: %s\n", cfg.error().message());
        return 1;
    }
    cf::Config::Range r = cfg.children("plugins");
    for (cf::Config::Range::iterator i = r.begin(); i != r.end(); ++i) {
        (void)(*i).name();
    }
    std::printf("consumer_cpp ok: port %d\n", cfg.get<int>("server.port", 0));
    return 0;
}
