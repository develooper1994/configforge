/*
 * test_wrapper_write.cpp - changing and keeping through the C++03
 * wrapper: memory(), define(), derive() and write() reach the C calls
 * and carry their errors back in the handle.
 */

#include <configforge/configforge.hpp>

#include <cstdio>
#include <cstring>

static int  g_fail = 0, g_run = 0;
static void check(bool c, const char* w)
{
    ++g_run;
    if (!c) {
        ++g_fail;
        std::printf("FAIL: %s\n", w);
    }
}
static void has(const char* hay, const char* needle, const char* w)
{
    check(hay != 0 && std::strstr(hay, needle) != 0, w);
    if (hay != 0 && std::strstr(hay, needle) == 0) {
        std::printf("  missing \"%s\" in \"%s\"\n", needle, hay);
    }
}

int main()
{
    static const char* const only_env[] = {"env", 0};
    static const cf_pair     pairs[]    = {{"server.port", "9000"}};
    static char*             no_env[]   = {0};
    cf_def                   locked;

    cf_def_init(&locked, CF_TYPE_TEXT, "db.password");
    locked.policy = only_env;

    cf::Config c = cf::Builder()
                       .memory("mem", CF_RANK_USER_FILE, pairs, 1)
                       .env("env", CF_RANK_ENV, "APP_", no_env) /* the policy's source */
                       .defineInt("server.port", 8080, 1, 65535)
                       .define(locked)
                       .create();
    check(c.valid(), "memory() and define() build");
    if (!c) {
        std::printf("  %s\n", c.error().message());
        return 1;
    }
    check(c.get<int>("server.port", 0) == 9000, "memory() is a source");

    /* derive(): a new snapshot, the old one unchanged. */
    cf_pair    change[1] = {{"server.port", "9443"}};
    cf::Config d         = c.derive("admin", change, 1);
    check(d.valid() && d.get<int>("server.port", 0) == 9443, "derive() applies the change");
    check(c.get<int>("server.port", 0) == 9000, "and leaves the original as it was");

    cf_pair    bad[1] = {{"server.port", "0"}};
    cf::Config r      = d.derive("admin", bad, 1);
    check(!r, "a change the contract refuses is an invalid handle");
    has(r.error().message(), "0 is outside 1..65535", "whose error says why");

    /* write(): a memory source cannot be written, and the handle's
       caller hears why. */
    cf_store_item one;
    one.key = "server.port";
    cf_value_int(&one.value, 9443);
    cf::Error e;
    check(!d.write("mem", &one, 1, "test", &e), "write() to a memory source is refused");
    check(e.code() == CF_E_CAPABILITY, "for its capability");
    has(e.message(), "cannot be written", "and says so");
    check(!d.write("nowhere", &one, 1), "and to no source, without an Error to fill");

    std::printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
