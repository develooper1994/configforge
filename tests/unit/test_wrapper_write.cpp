/*
 * test_wrapper_write.cpp - changing and keeping through the C++03
 * wrapper: memory(), define(), derive() and write() reach the C calls
 * and carry their errors back in the handle.
 */

#include <configforge/configforge.hpp>

#include <cstdio>
#include <cstring>

#include "cf_test.h"

int main() {
    static const char* const only_env[] = {"env", 0};
    static const cf_pair     pairs[]    = {{"server.port", "9000"}};
    static char*             no_env[]   = {0};
    cf_def                   locked;

    cf_def_init(&locked, CF_TYPE_TEXT, "db.password");
    locked.policy = only_env;

    cf::Config c =
        cf::Builder()
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
    check(d.valid() && d.get<int>("server.port", 0) == 9443,
          "derive() applies the change");
    check(c.get<int>("server.port", 0) == 9000, "and leaves the original as it was");

    cf_pair    bad[1] = {{"server.port", "0"}};
    cf::Config r      = d.derive("admin", bad, 1);
    check(!r, "a change the contract refuses is an invalid handle");
    has(r.error().message(), "0 is outside 1..65535", "whose error says why");

    /* write(): a memory source cannot be written, and the handle's
       caller hears why. */
    cf_write_item one;
    one.key = "server.port";
    one.op  = CF_WRITE_SET;
    cf_value_int(&one.value, 9443);
    cf::Error e;
    check(!d.write("mem", &one, 1, "test", &e),
          "write() to a memory source is refused");
    check(e.code() == CF_E_CAPABILITY, "for its capability");
    has(e.message(), "cannot be written", "and says so");
    check(!d.write("nowhere", &one, 1), "and to no source, without an Error to fill");

    /* report(): an info that starts from CF_REPORT_INFO_INIT names what
       it is given; one that never had its size set is refused, and the
       string is empty (D-51). */
    if (cf_has_feature("report")) {
        cf_report_info info = CF_REPORT_INFO_INIT;
        info.application    = "wrapper";
        has(c.report(info).c_str(), "\"name\": \"wrapper\"",
            "report() names the application");
        info.size = 0;
        check(c.report(info).empty(), "and is empty for an info that was never sized");
    }

    /* Read coverage: off unless asked for; a getter and has() read,
       wasRead() does not. */
    check(!c.tracksReads(), "reads are not tracked unless asked for");
    cf::Config t = cf::Builder()
                       .memory("mem", CF_RANK_USER_FILE, pairs, 1)
                       .defineInt("server.port", 8080, 1, 65535)
                       .defineBool("tls.enabled", false)
                       .trackReads()
                       .create();
    check(t.valid() && t.tracksReads(), "trackReads() reaches the builder");
    check(!t.wasRead("server.port"), "nothing is read yet");
    check(t.get<int>("server.port", 0) == 9000 && t.has("tls.enabled"), "two reads");
    check(t.wasRead("server.port") && t.wasRead("tls.enabled"), "both recorded");
    check(t.readCount() == 2, "readCount() counts keys");

    return cf_test_done();
}
