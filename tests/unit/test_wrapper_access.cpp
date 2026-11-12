/*
 * test_wrapper_access.cpp - the three ways the C++03 wrapper reads a
 * value without repeating its default: a declared Key, a Setting named
 * a part at a time, and a Binding that fills the program's own struct.
 * Built as C++98, like every wrapper consumer.
 */

#include <configforge/configforge.hpp>

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <stdlib.h> /* setenv, unsetenv, _putenv_s */
#include <string>

#include "cf_kv_codec.h"
#include "cf_test.h"

static const cf::Key<int>         kPort("server.port", 8080, 1, 65535);
static const cf::Key<std::string> kHost("server.host", "0.0.0.0");
static const cf::Key<bool>        kTls("tls.enabled", false);
static const cf::Key<long>        kWorkers("server.workers", 4L, 1L, 64L);

#if CF_CPP11
/* D-75: from C++11 a Key can be a constant, and as one it is checked by
   the compiler with the library's rule - a name the library would refuse,
   a range upside down or a default outside its own range does not compile
   (the CF_TEST_REFUSED cases at the end, which tests/unit/CMakeLists.txt
   builds expecting each to fail by name). */
constexpr cf::Key<int> kLimit("server.limit", 100, 1, 1000);
static_assert(kLimit.fallback() == 100, "a Key read at compile time");
static_assert(kLimit.lo() == 1 && kLimit.hi() == 1000, "its range too");
static_assert(cf::key_is_valid("server.port"), "the library's rule, at compile time");
static_assert(!cf::key_is_valid("Server.Port"), "refusing what the library does");
#endif

/* The wrapper's rule is the library's (cf_key_valid), in every standard:
   what refuses at compile time is what the library refuses at run time. */
static void test_key_rule() {
    static const char* const keys[] = {"server.port", "a",    "a-b_c.d9", "",    ".a",
                                       "a.",          "a..b", "A.b",      "a b", "a/b"};
    static char              longest[CF_KEY_MAX + 1];
    for (size_t i = 0; i < sizeof keys / sizeof keys[0]; ++i) {
        bool lib = cf_key_valid(keys[i], std::strlen(keys[i])) != CF_FALSE;
        check(cf::key_is_valid(keys[i]) == lib, keys[i]);
    }
    std::memset(longest, 'k', CF_KEY_MAX - 1);
    check(cf::key_is_valid(longest), "the longest key the build allows is a key");
    longest[CF_KEY_MAX - 1] = 'k';
    check(!cf::key_is_valid(longest), "and one byte more is not");
    check(!cf::key_is_valid(0), "nor is no key at all");

    /* Made at run time, a Key the compiler cannot check is the library's
       to refuse, at create, as it always was. */
    const char*      name = keys[7];
    cf::Key<int>     bad(name, 8080);
    const cf::Config c = cf::Builder().define(bad).create();
    check(!c && c.error().code() == CF_E_DEFINITION, "the run-time check remains");
    cf::Key<int>     outside("server.port", 0, 1, 65535);
    const cf::Config d = cf::Builder().define(outside).create();
    check(!d && d.error().code() == CF_E_DEFINITION, "for a range as for a name");
}

static void test_keys() {
    static const cf_pair pairs[] = {{"server.port", "9443"}, {"tls.enabled", "true"}};
    cf::Config           cfg     = cf::Builder()
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
    cf::Config           refused =
        cf::Builder().memory("file", CF_RANK_USER_FILE, bad, 1).define(kPort).create();
    check(!refused && std::strstr(refused.error().message(), "65535") != 0,
          "and the key's range is the contract's");
}

/* D-74: define(key, default), the type the default's, makes the same
   contract as the typed calls, byte for byte. */
static void test_one_define() {
    cf::Builder typed;
    typed.defineInt("server.port", 8080, 1, 65535)
        .defineText("server.host", "0.0.0.0")
        .defineBool("tls.enabled", false)
        .defineInt("server.limit", (cf_i64)1 << 40);
    cf::Builder one;
    one.define("server.port", 8080, 1, 65535)
        .define("server.host", std::string("0.0.0.0"))
        .define("tls.enabled", false)
        .define("server.limit", (cf_i64)1 << 40);
#if CF_WITH_REAL
    typed.defineReal("server.ratio", 0.5);
    one.define("server.ratio", 0.5);
#endif
    cf::Config a = typed.create();
    cf::Config b = one.create();
    check(a.valid() && b.valid(), "both spellings build");
    check(b.schema("t") == a.schema("t"), "and make the same contract");
    check(b[kPort] == 8080, "an int, from 8080");
}

static void test_settings() {
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

static void test_binding() {
    static const cf_pair  pairs[] = {{"server.port", "9443"}, {"tls.enabled", "true"}};
    cf::Binding<Settings> bind;
    bind.field("server.host", &Settings::server, &Server::host, "0.0.0.0")
        .field("server.port", &Settings::server, &Server::port, 8080, 1, 65535)
        .field("tls.enabled", &Settings::tls, false)
        .field("server.workers", &Settings::workers, 4L, 1L, 64L);
    check(bind.size() == 4, "four fields");

    cf::Config cfg =
        cf::Builder().memory("file", CF_RANK_USER_FILE, pairs, 2).define(bind).create();
    check(cfg.valid(), "the binding is the contract");
    Settings settings = bind.read(cfg);
    check(settings.server.port == 9443, "settings.server.port");
    check(settings.server.host == "0.0.0.0", "a default from the binding");
    check(settings.tls, "a bool field");
    check(settings.workers == 4L, "a long field");

    static const cf_pair bad[] = {{"server.port", "0"}};
    cf::Config           refused =
        cf::Builder().memory("file", CF_RANK_USER_FILE, bad, 1).define(bind).create();
    check(!refused, "a field's range refuses as a definition's does");
}

static const cf_pair  g_v1[] = {{"server.port", "8080"}, {"log.level", "info"}};
static const cf_pair  g_v2[] = {{"server.port", "9443"}, {"log.level", "debug"}};
static const cf_pair* g_live = g_v1;

static cf_builder* build_live(void*) {
    return cf::Builder()
        .memory("mem", CF_RANK_USER_FILE, g_live, 2)
        .defineInt("server.port", 8080, 1, 65535, CF_RESTART)
        .defineText("log.level", "info")
        .release();
}

static void test_live() {
    g_live = g_v1;
    cf::Live live(CF_LIVE_APPROVE, build_live);
    check(live.valid() && live.generation() == 1, "a Live opens");
    g_live = g_v2;
    check(live.reload() == CF_LIVE_PENDING &&
              live.pending().text("log.level") == "debug",
          "a change waits, and can be looked at");
    check(live.get().text("log.level") == "info", "readers do not see it yet");
    check(live.approve() == CF_LIVE_SWAPPED && live.get().text("log.level") == "debug",
          "approved, they do");
    check(live.get()["server.port"].as<int>() == 8080 &&
              live.restarts() == "server.port\n",
          "the port waits for a restart");
    check(!live.pending().valid(), "nothing pending after");
    check(live.status().reloads == 1 && live.status().generation == 2 &&
              live.status().failures == 0,
          "and the status counts it");
    check(live.lastError().code() == CF_OK, "no failure, no error");
    {
        cf_live_options o = CF_LIVE_OPTIONS_INIT;
        o.build           = build_live;
        o.probation       = 2;
        g_live            = g_v1;
        cf::Live onp(o);
        g_live = g_v2;
        check(onp.valid() && onp.reload() == CF_LIVE_SWAPPED && onp.status().probation,
              "a Live from options puts a swap on probation");
        check(onp.rollback("a test") == CF_LIVE_ROLLED_BACK &&
                  onp.get().text("log.level") == "info" && onp.status().rollbacks == 1,
              "and rolls it back");
        check(!onp.confirm() && onp.reload() == CF_LIVE_SWAPPED && onp.confirm(),
              "confirm keeps the one readers have");
        check(onp.get().fingerprint() == cf_fingerprint(onp.get().raw()) &&
                  onp.get().fingerprintText().size() == 22,
              "a Config's fingerprint, as a number and as text");
    }
#if CF_CPP11
    g_live         = g_v1;
    int      built = 0;
    cf::Live fn(
        CF_LIVE_MANUAL,
        [&built]() {
            ++built;
            return std::move(cf::Builder().memory("mem", CF_RANK_USER_FILE, g_live, 2));
        },
        [](const cf_config*, const cf_config* next, cf_error* err) {
            if (cf_get_text(next, "log.level", "")[0] == 'd') {
                return cf_error_set(err, CF_E_POLICY, "log.level", 0, "no debug") != 0;
            }
            return true;
        });
    g_live = g_v2;
    check(fn.valid() && fn.reload() == CF_LIVE_FAILED && built == 2 &&
              std::string(fn.error().message()).find("no debug") != std::string::npos,
          "a lambda builds and a lambda refuses");

    /* A throw inside a build or a check must not unwind through C. */
    int      calls = 0;
    cf::Live thrower(
        CF_LIVE_MANUAL,
        [&calls]() {
            if (++calls == 2) {
                throw std::runtime_error("no builder today");
            }
            return std::move(cf::Builder().memory("mem", CF_RANK_USER_FILE, g_live, 2));
        },
        [&calls](const cf_config*, const cf_config*, cf_error*) -> bool {
            if (calls > 2) {
                throw std::runtime_error("no check today");
            }
            return true;
        });
    check(thrower.valid() && thrower.reload() == CF_LIVE_FAILED,
          "a build that throws is a build that failed");
    check(thrower.reload() == CF_LIVE_FAILED &&
              std::string(thrower.lastError().message()).find("no check today") !=
                  std::string::npos,
          "a check that throws refuses, saying what was thrown");
#endif
}

static void test_edit() {
    cf::Config cfg = cf::Builder().memory("mem", CF_RANK_USER_FILE, g_v1, 2).create();
    cf::Error  err;
    cf::Edit   e(cfg, "mem");
    e.set("server.port", 9443).set("log.level", "debug").unset("old.key");
    check(e.size() == 3, "an edit gathers its changes");
    check(!e.commit("note", &err) && err.code() == CF_E_CAPABILITY,
          "and a source that cannot be written refuses them");
    check(!e.commit("again"), "an edit commits once");
    {
        cf::Edit dropped(cfg, "mem");
        dropped.set("server.port", 1);
    } /* dropped without a commit: nothing written, nothing leaked */
}

static void test_lists() {
    static const cf_pair pairs[] = {{"tags.count", "1"}, {"tags.0", "source"}};
    cf_def               d;
    cf_value             value;
    cf_def_init(&d, CF_TYPE_TEXT, "tags");
    d.list_mode   = CF_LIST_APPEND;
    d.has_default = CF_TRUE;
    cf_value_text(&value, "base", 4);
    d.list_defaults      = &value;
    d.list_default_count = 1;
    cf::Config cfg =
        cf::Builder().define(d).memory("file", CF_RANK_USER_FILE, pairs, 2).create();
    check(cfg.valid(), "the full-definition C++ API accepts a list contract");
    check(cfg["tags.count"].as<int>() == 2 &&
              cfg["tags.0"].as<std::string>() == "base" &&
              cfg["tags.1"].as<std::string>() == "source",
          "existing C++ typed reads access the appended elements");
}

/* A codec of one's own, handed to a file and registered on the builder
   (cf_kv_codec.h, compiled as C++ here). */
static void test_codecs() {
    check(spill("cf_wrapper.kv", "server.port: 9443\n"), "a file in a foreign format");
    cf::Config one =
        cf::Builder()
            .fileWith("file", CF_RANK_USER_FILE, "cf_wrapper.kv", &kv_codec)
            .define(kPort)
            .create();
    check(one.valid() && one[kPort] == 9443, "fileWith reads one file through it");
    cf::Config all = cf::Builder()
                         .codec(&kv_codec)
                         .file("file", CF_RANK_USER_FILE, "cf_wrapper.kv")
                         .define(kPort)
                         .create();
    check(all.valid() && all[kPort] == 9443, "codec() makes its extension choose it");
    std::remove("cf_wrapper.kv");
}

#if CF_WITH_DIR
/* NULL unsets it. Windows' _putenv_s removes a variable set to "". */
static void creds_set(const char* value) {
#if defined(_WIN32)
    _putenv_s("CREDENTIALS_DIRECTORY", (value != 0) ? value : "");
#else
    if (value == 0) {
        unsetenv("CREDENTIALS_DIRECTORY");
    } else {
        setenv("CREDENTIALS_DIRECTORY", value, 1);
    }
#endif
}
#endif /* CF_WITH_DIR */

#define CF_T_TEXT2(x) #x
#define CF_T_TEXT(x) CF_T_TEXT2(x)

/* A directory of secrets and systemd's credentials, which are read the
   same way (D-80). The builds of this test for each standard run side by
   side in one directory, so each has a file, and a key, of its own, and
   none removes it: another may be reading the directory. */
static void test_directories() {
#if CF_WITH_DIR
    const std::string key  = "db.password-" CF_T_TEXT(CF_CXX_LEVEL);
    const std::string path = "cf_wcreds/" + key;
    check(spill(path.c_str(), "s3cret\n"), "a credential");

    cf::Config secrets = cf::Builder().secrets("s", CF_RANK_ENV, "cf_wcreds").create();
    const bool from_secrets = secrets.text(key.c_str()) == "s3cret";
    check(from_secrets, "secrets() reads a directory of them");

    creds_set("cf_wcreds");
    cf::Config        creds    = cf::Builder().credentials(0, CF_RANK_ENV).create();
    const bool        from_dir = creds.text(key.c_str()) == "s3cret";
    const std::string why      = creds.explain(key.c_str());
    check(from_dir, "credentials() reads the one $CREDENTIALS_DIRECTORY names");
    check(why.find("systemd credential " + key) != std::string::npos,
          "and explain() says so");

    creds_set(0);
    cf::Config optional =
        cf::Builder().credentials("c", CF_RANK_ENV, 0, CF_OPTIONAL).create();
    check(optional.valid(), "not run with credentials, and optional: it builds");
    cf::Config  refused = cf::Builder().credentials("c", CF_RANK_ENV).create();
    const char* named = std::strstr(refused.error().message(), "CREDENTIALS_DIRECTORY");
    check(!refused && named != 0, "not optional: an error that names the variable");
#endif
}

int main() {
    test_key_rule();
    test_directories();
    test_keys();
    test_one_define();
    test_settings();
    test_binding();
    test_live();
    test_edit();
    test_lists();
    test_codecs();
    return cf_test_done();
}

/* What must not compile (D-75), one case a build; each is expected to
   stop the compiler with the name of what is wrong in it. */
#if defined(CF_TEST_REFUSED)
#if CF_TEST_REFUSED == 1
constexpr cf::Key<int> kRefused("Server.Port", 8080);
#elif CF_TEST_REFUSED == 2
constexpr cf::Key<int> kRefused("server.port", 0, 1, 65535);
#elif CF_TEST_REFUSED == 3
constexpr cf::Key<int> kRefused("server.port", 5, 9, 1);
#endif
#endif
