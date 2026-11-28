/*
 * test_wrapper_callback.cpp - a callback that throws never unwinds
 * through the C library (K-9i, docs/Guarantees.md).
 *
 * cf_live calls the build and the check from inside its own frames, and an
 * exception through them is undefined behaviour. The C++11 wrapper catches
 * before C: a build that throws is a build that failed, a check that throws
 * refuses and says what it threw, and the program carries on.
 * Built as C++11, where cf::Live takes callables.
 */

#include <configforge/configforge.hpp>

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

#include "cf_test.h"

#if !CF_CPP11
#error "built as C++11, and the wrapper's C++11 part is off"
#endif

static const cf_pair g_one[] = {{"log.level", "info"}};
static const cf_pair g_two[] = {{"log.level", "debug"}};

static bool g_throw_build = false;
static bool g_throw_check = false;
static int  g_unknown     = 0;

static cf::Builder build() {
    if (g_throw_build) {
        throw std::runtime_error("the build threw");
    }
    cf::Builder b;
    b.memory("mem", CF_RANK_USER_FILE, g_unknown ? g_two : g_one, 1);
    b.defineText("log.level", "warn");
    return b;
}

static bool checker(const cf_config*, const cf_config*, cf_error*) {
    if (g_throw_check) {
        if (g_unknown == 2) {
            throw 42; /* not even an exception object */
        }
        throw std::runtime_error("boom");
    }
    return true;
}

int main() {
    cf::Live live(CF_LIVE_MANUAL, build, checker);
    check(live.valid(), "opens: nothing threw yet");

    g_unknown     = 1;
    g_throw_check = true;
    cf::Error e1;
    check(live.reload(&e1) == CF_LIVE_FAILED, "a check that throws refuses");
    check(e1.code() == CF_E_POLICY, "with the policy code");
    check(std::strstr(e1.message(), "boom") != 0, "and says what was thrown");
    check(live.get().get<std::string>("log.level", "") == "info",
          "the running snapshot stays");

    g_unknown = 2;
    cf::Error e2;
    check(live.reload(&e2) == CF_LIVE_FAILED,
          "a check that throws a non-exception refuses");
    check(e2.code() == CF_E_POLICY, "with the policy code, too");

    g_throw_check = false;
    g_throw_build = true;
    cf::Error e3;
    check(live.reload(&e3) == CF_LIVE_FAILED,
          "a build that throws is a build that failed");
    check(!e3.ok(), "with an error set");

    g_throw_build = false;
    check(live.reload() == CF_LIVE_SWAPPED, "and the next reload works");
    check(live.get().get<std::string>("log.level", "") == "debug",
          "and reads the new value");

    g_throw_build = true;
    cf::Live fresh(CF_LIVE_MANUAL, build);
    check(!fresh.valid(), "a first build that throws: no live config, no crash");

    return cf_test_done();
}
