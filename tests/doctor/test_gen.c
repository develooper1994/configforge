/*
 * test_gen.c - `doctor gen --schema` writes the same contract the schema
 * is: a builder given the generated function and one given the schema
 * itself write the same JSON Schema back (cf_schema), refuse the same
 * file, and need the same required key.
 *
 * The build generates gen_contract.c from tests/doctor/contract.schema.json
 * with the Doctor it built, and compiles it into this program with every
 * warning the project's own code gets.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#include "../unit/cf_test.h"

cf_bool gen_contract(cf_builder* b);

/* A configuration from the contract, either way, and these pairs. */
static cf_config* built(int generated, const cf_pair* pairs, size_t n, cf_error* err) {
    cf_builder* b = cf_build(NULL);

    if (generated) {
        check(gen_contract(b), "the generated contract defines on a builder");
    } else {
        check(cf_builder_schema_file(b, CF_CONTRACT_SCHEMA), "the schema reads");
    }
    cf_builder_unknown(b, CF_UNKNOWN_REJECT);
    cf_builder_memory(b, "memory", CF_RANK_USER_FILE, pairs, n);
    return cf_builder_create(b, err);
}

static void test_same_schema(void) {
    static const cf_pair pairs[] = {{"server.token", "t"}, {"plugins.x.y", "1"}};
    static char          a[16384], b[16384];
    cf_error             err = CF_ERROR_INIT;
    cf_config*           from_schema;
    cf_config*           from_code;
    size_t               na, nb;

    from_schema = built(0, pairs, 2, &err);
    check(from_schema != NULL, "the schema's contract builds");
    if (from_schema == NULL) {
        printf("  %s\n", err.message);
    }
    from_code = built(1, pairs, 2, &err);
    check(from_code != NULL, "the generated contract builds");
    if (from_code == NULL) {
        printf("  %s\n", err.message);
    }
    if (from_schema != NULL && from_code != NULL) {
        na = cf_schema(from_schema, "contract", a, sizeof a);
        nb = cf_schema(from_code, "contract", b, sizeof b);
        check(na > 0 && na < sizeof a && na == nb && memcmp(a, b, na) == 0,
              "and both write the same JSON Schema");
        if (na != nb || memcmp(a, b, na) != 0) {
            printf("--- from the schema:\n%s\n--- from the code:\n%s\n", a, b);
        }
        check(cf_get_int(from_code, "server.port", 0) == 8080, "with the defaults");
#if CF_WITH_REAL
        check(cf_get_real(from_code, "ratio", 0.0) == 0.25, "a real one too");
#endif
        check_text(cf_get_text(from_code, "tags.1", ""), "b\xc3\xa9",
                   "and a list's, past ASCII");
    }
    cf_config_free(from_schema);
    cf_config_free(from_code);
}

static void test_same_refusals(void) {
    static const cf_pair typo[]    = {{"server.token", "t"}, {"server.prot", "1"}};
    static const cf_pair range[]   = {{"server.token", "t"}, {"server.port", "70000"}};
    static const cf_pair missing[] = {{"server.port", "80"}};
    int                  generated;

    for (generated = 0; generated <= 1; ++generated) {
        cf_error   err = CF_ERROR_INIT;
        cf_config* c   = built(generated, typo, 2, &err);
        check(c == NULL, "an unknown key is refused");
        cf_config_free(c);
        c = built(generated, range, 2, &err);
        check(c == NULL, "a value out of range is refused");
        cf_config_free(c);
        c = built(generated, missing, 1, &err);
        check(c == NULL, "a required key is required");
        cf_config_free(c);
    }
}

int main(void) {
    test_same_schema();
    test_same_refusals();
    return cf_test_done();
}
