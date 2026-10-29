/*
 * test_wrapper_access.cpp - the three ways the C++03 wrapper reads a
 * value without repeating its default: a declared Key, a Setting named
 * a part at a time, and a Binding that fills the program's own struct.
 * Built as C++98, like every wrapper consumer.
 */

#include <configforge/configforge.hpp>

#include <cstdio>
#include <cstring>
#include <string>

static int  g_fail = 0, g_run = 0;
static void check(bool c, const char* what)
{
    ++g_run;
    if (!c) {
        ++g_fail;
        std::printf("FAIL: %s\n", what);
    }
}

static const cf::Key<int>         kPort("server.port", 8080, 1, 65535);
static const cf::Key<std::string> kHost("server.host", "0.0.0.0");
static const cf::Key<bool>        kTls("tls.enabled", false);
static const cf::Key<long>        kWorkers("server.workers", 4L, 1L, 64L);

static void test_keys()
{
    static const cf_pair pairs[] = {{"server.port", "9443"}, {"tls.enabled", "true"}};
    cf::Config cfg = cf::Builder()
                         .memory("file", CF_RANK_USER_FILE, pairs, 2)
                         .define(kPort)
                         .define(kHost)
                         .define(kTls)
                         .define(kWorkers)
                         .create();
    check(cfg.valid(), "a contract made of keys builds");
    check(cfg[kPort] == 9443, "a set key reads as its type");
    check(cfg[kHost] == "0.0.0.0", "an unset key reads as its default");
    check(cfg.get(kTls), "a bool key");
    check(cfg[kWorkers] == 4L, "a long key");

    static const cf_pair bad[] = {{"server.port", "99999"}};
    cf::Config refused = cf::Builder()
                             .memory("file", CF_RANK_USER_FILE, bad, 1)
                             .define(kPort)
                             .create();
    check(!refused && std::strstr(refused.error().message(), "65535") != 0,
          "and the key's range is the contract's");
}

static void test_settings()
{
    static const cf_pair pairs[] = {
        {"server.port", "9443"}, {"server.host", "web1"}, {"tls.enabled", "true"}};
    cf::Config cfg = cf::Builder()
                         .memory("file", CF_RANK_USER_FILE, pairs, 3)
                         .define(kPort)
                         .define(kWorkers)
                         .create();
    check(cfg.valid(), "a configuration to read by name");

    int         port    = cfg["server"]["port"];
    std::string host    = cfg["server.host"];
    long        workers = cfg["server"]["workers"];
    check(port == 9443, "named a part at a time, typed where it is read");
    check(host == "web1", "text by its whole name");
    check(workers == 4L, "a defined key's default, with no fallback passed");
    check(cfg["tls"]["enabled"] ? true : false, "a bool in a condition");
    check(cfg["server"]["port"].key() == "server.port", "the parts join with dots");
    check(cfg["server.port"].exists() && !cfg["server.nope"].exists(),
          "exists() tells a missing key from a zero");
    check(cfg["server.nope"].as<int>() == 0 && cfg["server.nope"].as<int>(7) == 7,
          "a missing key reads as T(), or as the fallback asked for");
    check(cfg["server.port"].type() == CF_TYPE_INT, "with its type");
    check(cfg["server.port"].explain().find("file") != std::string::npos,
          "and it can say why");

    cf::Setting kept = cfg["server"];
    cfg              = cf::Config();
    check(kept["port"].as<int>() == 9443, "a Setting keeps its configuration alive");
}

struct Server {
    std::string host;
    int         port;
};
struct Settings {
    Server server;
    bool   tls;
    long   workers;
};

static void test_binding()
{
    static const cf_pair pairs[] = {{"server.port", "9443"}, {"tls.enabled", "true"}};
    cf::Binding<Settings> bind;
    bind.field("server.host", &Settings::server, &Server::host, "0.0.0.0")
        .field("server.port", &Settings::server, &Server::port, 8080, 1, 65535)
        .field("tls.enabled", &Settings::tls, false)
        .field("server.workers", &Settings::workers, 4L, 1L, 64L);
    check(bind.size() == 4, "four fields");

    cf::Config cfg = cf::Builder()
                         .memory("file", CF_RANK_USER_FILE, pairs, 2)
                         .define(bind)
                         .create();
    check(cfg.valid(), "the binding is the contract");
    Settings settings = bind.read(cfg);
    check(settings.server.port == 9443, "settings.server.port");
    check(settings.server.host == "0.0.0.0", "a default from the binding");
    check(settings.tls, "a bool field");
    check(settings.workers == 4L, "a long field");

    static const cf_pair bad[] = {{"server.port", "0"}};
    cf::Config refused = cf::Builder()
                             .memory("file", CF_RANK_USER_FILE, bad, 1)
                             .define(bind)
                             .create();
    check(!refused, "a field's range refuses as a definition's does");
}

int main()
{
    test_keys();
    test_settings();
    test_binding();
    std::printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
