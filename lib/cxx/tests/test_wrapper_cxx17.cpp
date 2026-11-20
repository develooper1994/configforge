/*
 * test_wrapper_cxx17.cpp - what the wrapper says under C++11 (move, an
 * explicit bool, pairs in braces) and its C++17 part: find<T> as std::optional, value()
 * as a variant, items() for a structured binding, and Fields over member pointers.
 */

#include <configforge/configforge.hpp>

#include "cf_test.h"

#if !CF_CPP17
#error "built as C++17, and the wrapper's C++17 part is off"
#endif

#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>

static cf::Config make() {
    static const cf_pair pairs[] = {{"server.port", "9443"},
                                    {"server.host", "web1"},
                                    {"tls.enabled", "true"},
                                    {"big", "5000000000"},
                                    {"neg", "-1"}};
    return cf::Builder().memory("file", CF_RANK_USER_FILE, pairs, 5).create();
}

static void test_cxx11() {
    cf::Config a = make();
    check(static_cast<bool>(a), "an explicit bool");
    cf::Config b = std::move(a);
    check(!a && b, "a move takes the handle and leaves none");
    cf::Config d = b.derive("admin", {{"server.port", "7000"}, {"log.level", "debug"}});
    check(d && d.get<int>("server.port", 0) == 7000, "pairs in braces");
    check(b.get<int>("server.port", 0) == 9443,
          "and the snapshot derived from is unchanged");

    cf::Builder bl;
    cf::Builder moved = std::move(bl);
    check(moved.defineInt("x", 1).create().get<int>("x", 0) == 1, "a builder moves");
}

static void test_find() {
    cf::Config cfg = make();
    check(cf::find<int>(cfg, "server.port") == 9443, "a value that is there");
    check(!cf::find<int>(cfg, "server.nope"), "nullopt when it is not");
    check(!cf::find<int>(cfg, "server.host"), "nullopt when it is another type");
    check(!cf::find<std::int32_t>(cfg, "big") && cf::find<std::int64_t>(cfg, "big"),
          "nullopt rather than a truncated integer");
    check(!cf::find<unsigned>(cfg, "neg") && cf::find<int>(cfg, "neg") == -1,
          "and rather than a negative one wrapped");
    check(cf::find<std::string_view>(cfg, "server.host") == "web1", "text as a view");
    check(cf::find<std::string>(cfg, "server.host") == std::string("web1"),
          "or as a string");
    check(cf::find<bool>(cfg, "tls.enabled") == true, "a bool");
    std::string key = "server.port.extra";
    check(cf::find<int>(cfg, std::string_view(key).substr(0, 11)) == 9443,
          "a key that is a view into a longer string");
    std::string huge(CF_KEY_MAX + 10, 'k');
    check(!cf::find<int>(cfg, huge), "and one longer than any key");
}

static void test_value_and_items() {
    cf::Config cfg = make();
    check(std::get<cf_i64>(cf::value(cfg, "server.port")) == 9443,
          "a variant of the type");
    check(std::get<std::string_view>(cf::value(cfg, "server.host")) == "web1",
          "text in the variant");
    check(std::holds_alternative<std::monostate>(cf::value(cfg, "nope")),
          "nothing for an absent key");

    int n = 0;
    for (auto [key, value] : cf::items(cfg, "server")) {
        ++n;
        if (key == "server.port") {
            check(std::get<cf_i64>(value) == 9443, "a structured binding");
        }
    }
    check(n == 2, "every key under the prefix");
    int t = 0;
    for (auto [key, value] : cf::children(cfg, 0)) {
        if (key == "server") {
            t += std::holds_alternative<std::monostate>(value) ? 1 : 0;
        }
    }
    check(t == 1, "a table among the children has no value");
}

struct Server {
    std::string host;
    int         port = 0;
};
struct Settings {
    Server server;
    bool   tls     = false;
    long   workers = 0;
};

inline const auto settings_fields = cf::fields(
    cf::field<&Settings::server, &Server::host>("server.host", "0.0.0.0"),
    cf::field<&Settings::server, &Server::port>("server.port", 8080, 1, 65535),
    cf::field<&Settings::tls>("tls.enabled", false),
    cf::field<&Settings::workers>("server.workers", 4L, 1L, 64L));

/* D-75: a field list of keys that can be constants is one, and the
   compiler checks each key as the library would (CF_TEST_REFUSED below). */
constexpr auto checked_fields = cf::fields(
    cf::field<&Settings::server, &Server::port>("server.port", 8080, 1, 65535),
    cf::field<&Settings::workers>("server.workers", 4L, 1L, 64L));

static void test_fields() {
    static const cf_pair pairs[] = {{"server.port", "9443"}, {"tls.enabled", "true"}};
    static_assert(decltype(settings_fields)::size() == 4, "four fields");
    static_assert(decltype(checked_fields)::size() == 2, "a constant list");
    cf::Config constant = cf::Builder().define(checked_fields).create();
    check(constant.get<int>("server.port", 0) == 8080,
          "a constant field list is a contract too");
    cf::Config cfg = cf::Builder()
                         .memory("file", CF_RANK_USER_FILE, pairs, 2)
                         .define(settings_fields)
                         .create();
    check(cfg.valid(), "the field list is the contract");
    auto s = settings_fields.read<Settings>(cfg);
    check(s.server.port == 9443 && s.server.host == "0.0.0.0", "a path of members");
    check(s.tls && s.workers == 4L, "and one member");

    static const cf_pair bad[]   = {{"server.workers", "65"}};
    cf::Config           refused = cf::Builder()
                                       .memory("file", CF_RANK_USER_FILE, bad, 1)
                                       .define(settings_fields)
                                       .create();
    check(!refused, "a field's range refuses");
}

int main() {
    test_cxx11();
    test_find();
    test_value_and_items();
    test_fields();
    return cf_test_done();
}

/* What must not compile (D-75): a constant field list with a key the
   library would refuse. */
#if defined(CF_TEST_REFUSED) && CF_TEST_REFUSED == 1
constexpr auto refused_fields =
    cf::fields(cf::field<&Settings::workers>("Server.Workers", 4L, 1L, 64L));
#endif
