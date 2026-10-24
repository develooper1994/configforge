/*
 * tier0.cpp - the smallest complete program, in C++03.
 *
 * tier0.c through the wrapper: one call, three reads, and the error in
 * the handle rather than in an out-parameter.
 *
 *   ./tier0_cpp
 *   ./tier0_cpp --server.port=7000
 *   MYAPP_SERVER__PORT=9090 ./tier0_cpp
 */

#include <configforge/configforge.hpp>

#include <cstdio>

int main(int argc, char** argv)
{
    cf::Config cfg = cf::app("myapp", argc, argv);

    if (!cfg) {
        std::fputs(cfg.error().message(), stderr);
        return 1;
    }
    std::printf("%s:%d tls=%s\n",
                cfg.text("server.host", "0.0.0.0").c_str(),
                cfg.get<int>("server.port", 8080),
                cfg.get<bool>("tls.enabled", false) ? "on" : "off");
    return 0; /* the handle frees the configuration */
}
