/*
 * test_wrapper_cxx20.cpp - configforge20.hpp: the readable concept,
 * Items as a view under the standard adaptors, std::format of a
 * Setting, a span of pairs, and, under C++23, lookup<T> as
 * std::expected.
 */

#include <configforge/configforge20.hpp>

#include <cstdio>
#include <ranges>
#include <string>
#include <vector>

static int  g_fail = 0, g_run = 0;
static void check(bool c, const char* what)
{
    ++g_run;
    if (!c) {
        ++g_fail;
        std::printf("FAIL: %s\n", what);
    }
}

static_assert(cf::readable<int> && cf::readable<std::string_view> && cf::readable<bool>);
static_assert(!cf::readable<std::vector<int>> && !cf::readable<const char*>);
static_assert(std::ranges::view<cf::Items> && std::ranges::input_range<cf::Items>);

static cf::Config make()
{
    static const cf_pair pairs[] = {{"server.port", "9443"},
                                    {"server.host", "web1"},
                                    {"server.workers", "8"},
                                    {"tls.enabled", "true"}};
    return cf::Builder().memory("file", CF_RANK_USER_FILE, pairs, 4).create();
}

int main()
{
    cf::Config cfg = make();
    check(cf::get_or<int>(cfg, "server.port", 0) == 9443, "get_or, there");
    check(cf::get_or<int>(cfg, "server.nope", 7) == 7, "get_or, the fallback");

    auto is_int = [](const cf::Item& i) { return std::holds_alternative<cf_i64>(i.value); };
    int  ints   = 0;
    for (auto [key, value] : cf::items(cfg, "server") | std::views::filter(is_int)) {
        ints += (int)std::get<cf_i64>(value) > 0 ? 1 : 0;
    }
    check(ints == 2, "the keys as a view, under an adaptor");
    auto names = cf::items(cfg) | std::views::transform([](const cf::Item& i) { return i.key; }) |
                 std::views::take(1);
    check(std::ranges::distance(names) == 1, "and under two");

    check(std::format("port {}", cfg["server"]["port"]) == "port 9443", "format an int");
    check(std::format("[{:>6}]", cfg["server.host"]) == "[  web1]", "format text, padded");
    check(std::format("{}", cfg["tls.enabled"]) == "true", "format a bool");
    check(std::format("{}", cfg["nope"]).empty(), "an absent key formats as nothing");

    std::vector<cf_pair> change = {{"server.port", "7000"}};
    cf::Config           d      = cf::derive(cfg, "admin", change);
    check(d && d.get<int>("server.port", 0) == 7000, "a derived snapshot from a span");

#if CF_CPP_EXPECTED
    auto port = cf::lookup<int>(cfg, "server.port");
    check(port && *port == 9443, "lookup, there");
    auto missing = cf::lookup<int>(cfg, "server.nope");
    check(!missing && missing.error().code() == CF_E_KEY, "lookup says it is not set");
    auto wrong = cf::lookup<int>(cfg, "server.host");
    check(!wrong && wrong.error().code() == CF_E_CONVERT &&
              std::string(wrong.error().message()).find("text") != std::string::npos,
          "and that it is another type");
#endif
    std::printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
