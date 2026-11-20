/*
 * test_gen_cpp.cpp - `configforge gen --schema --lang cpp` writes the contract
 * the schema is, and the two ways a program reads it: a cf::Key for each key
 * and a struct filled through a cf::Binding.
 *
 * The build generates gen_contract.hpp from tests/doctor/contract.schema.json
 * with the gen program it built, and compiles it here as C++98 with every
 * warning the project's own code gets.
 */

#include "gen_contract.hpp"

#include <cstring>
#include <string>

#include "../support/cf_test.h"

/* A builder, either way: the generated function or the schema itself,
   given these pairs. */
static cf_config* built(bool generated, const cf_pair* pairs, size_t n, cf_error* err) {
    cf_builder* b = cf_build(NULL);

    if (generated) {
        check(gen_contract::define(b), "the generated contract defines on a builder");
    } else {
        check(cf_builder_schema_file(b, CF_CONTRACT_SCHEMA), "the schema reads");
    }
    cf_builder_unknown(b, CF_UNKNOWN_REJECT);
    cf_builder_memory(b, "memory", CF_RANK_USER_FILE, pairs, n);
    return cf_builder_create(b, err);
}

static void test_same_schema() {
    static const cf_pair pairs[] = {{"server.token", "t"}, {"plugins.x.y", "1"}};
    static char          a[16384], b[16384];
    cf_error             err = CF_ERROR_INIT;
    cf_config*           from_schema;
    cf_config*           from_code;

    from_schema = built(false, pairs, 2, &err);
    check(from_schema != NULL, "the schema's contract builds");
    from_code = built(true, pairs, 2, &err);
    check(from_code != NULL, "the generated contract builds");
    if (from_schema != NULL && from_code != NULL) {
        size_t na = cf_schema(from_schema, "contract", a, sizeof a);
        size_t nb = cf_schema(from_code, "contract", b, sizeof b);
        check(na > 0 && na < sizeof a && na == nb && std::memcmp(a, b, na) == 0,
              "and both write the same JSON Schema");
    }
    cf_config_free(from_schema);
    cf_config_free(from_code);
}

/* The pairs every test below starts from: the required key and a port. */
static cf::Config configured(const char* port) {
    const cf_pair pairs[2] = {{"server.token", "t"}, {"server.port", port}};

    return cf::Builder()
        .define(gen_contract::Contract())
        .memory("memory", CF_RANK_USER_FILE, pairs, 2)
        .create();
}

static void test_keys() {
    const cf::Config cfg = configured("9443");

    check(cfg.valid(), "a Config built through the generated Contract");
    check(cfg.get(gen_contract::keys::server_port) == 9443, "a Key reads what is set");
    check(cfg.get(gen_contract::keys::server_host) == "0.0.0.0",
          "and what is not, as its default");
    check(!cfg.get(gen_contract::keys::server_tls), "a bool's default");
    check(cfg.get(gen_contract::keys::server_token) == "t", "a text read");
#if CF_WITH_REAL
    check(cfg.get(gen_contract::keys::ratio) == 0.25, "a real's default");
#endif
}

static void test_binding() {
    const cf::Config                    cfg = configured("9443");
    cf::Binding<gen_contract::Settings> b;
    gen_contract::bind(b);
    const gen_contract::Settings s = b.read(cfg);

    check(s.server.port == 9443, "a field of a struct inside Settings");
    check(s.server.host == "0.0.0.0", "a text field's default");
    check(!s.server.tls, "a bool field's default");
    check(s.server.token == "t", "a field the configuration sets");
#if CF_WITH_REAL
    check(s.ratio == 0.25, "a field of Settings itself");
#endif
}

static void test_refusal() {
    const cf::Config cfg = configured("70000");

    check(!cfg.valid(), "a port out of range is refused at create");
    if (!cfg.valid()) {
        has(cfg.error().message(), "server.port", "and the error names the key");
    }
}

int main() {
    test_same_schema();
    test_keys();
    test_binding();
    test_refusal();
    return cf_test_done();
}
