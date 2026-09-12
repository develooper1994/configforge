/* test_keyspace.c - flat key space, provenance chain, determinism. */
#include "configforge/cf_keyspace.h"
#include <stdio.h>
#include <string.h>

static int  g_fail = 0, g_run = 0;
static void check(int c, const char* w) {
    ++g_run;
    if (!c) {
        ++g_fail;
        printf("FAIL: %s\n", w);
    }
}

/* Precedence ranks, lower wins. */
enum { SRC_CLI = 0, SRC_ENV = 1, SRC_USER = 2, SRC_SYS = 3, SRC_DEFAULT = 4 };

static void add_int(cf_keyspace* ks, const char* k, int src, cf_i64 n, const char* at) {
    cf_value v;
    cf_value_int(&v, n);
    check(cf_ks_add(ks, k, strlen(k), src, &v, at), "add int");
}

static void add_text(cf_keyspace* ks, const char* k, int src, const char* s,
                     const char* at) {
    cf_value v;
    cf_value_text(&v, s, strlen(s));
    check(cf_ks_add(ks, k, strlen(k), src, &v, at), "add text");
}

static void test_key_validation(void) {
    check(cf_key_valid("server.port", 11), "dotted key valid");
    check(cf_key_valid("a", 1), "single segment valid");
    check(cf_key_valid("server.listeners.0.port", 23), "index segment valid");
    check(cf_key_valid("tls_min-version", 15), "underscore and dash valid");

    check(!cf_key_valid("", 0), "empty rejected");
    check(!cf_key_valid(".a", 2), "leading dot rejected");
    check(!cf_key_valid("a.", 2), "trailing dot rejected");
    check(!cf_key_valid("a..b", 4), "empty segment rejected");
    check(!cf_key_valid("a b", 3), "space rejected");

    /* Uppercase is rejected rather than folded. Folding is how two
       distinct keys quietly become one, and in a Turkish locale a
       libc fold would not even agree with itself across systems. */
    check(!cf_key_valid("Server.Port", 11), "uppercase rejected");
    check(!cf_key_valid("SERVER_PORT", 11), "env spelling rejected as a key");

    check(cf_key_segments("a.b.c", 5) == 3, "three segments");
    check(cf_key_segments("a", 1) == 1, "one segment");
    check(cf_key_segments("bad key", 7) == 0, "invalid key has no segments");
}

static void test_precedence(cf_arena* ar) {
    cf_keyspace     ks;
    const cf_entry* e;
    const cf_value* v;

    check(cf_ks_init(&ks, ar, 4), "keyspace init");

    /* Deliberately out of precedence order, to prove the chain is
       ordered by rank and not by arrival. */
    add_int(&ks, "server.port", SRC_DEFAULT, 8080, NULL);
    add_int(&ks, "server.port", SRC_USER, 3000, "app.toml:14:3");
    add_int(&ks, "server.port", SRC_ENV, 9090, "MYAPP_SERVER_PORT");
    add_text(&ks, "server.host", SRC_DEFAULT, "0.0.0.0", NULL);

    cf_ks_resolve(&ks);

    v = cf_ks_get(&ks, "server.port", 11);
    check(v != NULL && v->type == CF_TYPE_INT && v->u.i == 9090,
          "highest precedence wins");

    e = cf_ks_find(&ks, "server.port", 11);
    check(e != NULL, "entry found");
    check(e->selected != NULL && e->selected->source == SRC_ENV,
          "selected is the env candidate");

    /* The whole point: the losers are still there, in order, with the
       reason they lost. A resolver that discarded them could not
       explain itself afterwards. */
    {
        const cf_candidate* c = e->chain;
        check(c != NULL && c->source == SRC_ENV && c->outcome == CF_OUT_SELECTED,
              "chain[0] env selected");
        check(c->at != NULL && strcmp(c->at, "MYAPP_SERVER_PORT") == 0,
              "chain[0] keeps its location");
        c = c->next;
        check(c != NULL && c->source == SRC_USER && c->outcome == CF_OUT_SHADOWED,
              "chain[1] user shadowed");
        check(c->at != NULL && strcmp(c->at, "app.toml:14:3") == 0,
              "chain[1] keeps file:line:col");
        check(c->value.u.i == 3000, "shadowed value still readable");
        c = c->next;
        check(c != NULL && c->source == SRC_DEFAULT && c->outcome == CF_OUT_SHADOWED,
              "chain[2] default shadowed");
        check(c->next == NULL, "chain ends");
    }

    check(cf_ks_count(&ks) == 2, "two distinct keys");
    check(cf_ks_get(&ks, "no.such.key", 11) == NULL, "absent key is NULL");
}

static void test_rejected_is_skipped(cf_arena* ar) {
    cf_keyspace     ks;
    const cf_entry* e;
    const cf_value* v;
    cf_value        bad;

    cf_ks_init(&ks, ar, 4);
    cf_value_int(&bad, 1);
    /* Rejection is recorded when the value is read, not patched in
       afterwards: that is where a conversion or range failure actually
       happens. */
    check(cf_ks_reject(&ks, "a.b", 3, SRC_ENV, &bad, "ENV", "out of range"),
          "rejected candidate recorded");
    add_int(&ks, "a.b", SRC_DEFAULT, 2, NULL);

    cf_ks_resolve(&ks);

    v = cf_ks_get(&ks, "a.b", 3);
    check(v != NULL && v->u.i == 2, "rejected candidate is skipped");

    e = cf_ks_find(&ks, "a.b", 3);
    check(e->chain->outcome == CF_OUT_REJECTED, "rejection survives resolve");
    check(e->chain->reason != NULL, "rejection reason retained");
}

static void test_sorted_and_search(cf_arena* ar) {
    cf_keyspace ks;
    size_t      i;

    cf_ks_init(&ks, ar, 2);
    add_int(&ks, "z.z", SRC_DEFAULT, 1, NULL);
    add_int(&ks, "a.a", SRC_DEFAULT, 2, NULL);
    add_int(&ks, "m.m", SRC_DEFAULT, 3, NULL);
    add_int(&ks, "b.b", SRC_DEFAULT, 4, NULL);
    add_int(&ks, "c.c", SRC_DEFAULT, 5, NULL); /* forces a grow */
    cf_ks_resolve(&ks);

    check(cf_ks_count(&ks) == 5, "five keys after growth");
    check(cf_ks_is_sorted(&ks), "sorted invariant holds");
    for (i = 1; i < cf_ks_count(&ks); ++i) {
        const cf_entry* p = cf_ks_at(&ks, i - 1);
        const cf_entry* q = cf_ks_at(&ks, i);
        check(cf_key_cmp(p->key, p->keylen, q->key, q->keylen) < 0,
              "entries in byte order");
    }
    check(cf_ks_get(&ks, "m.m", 3) != NULL, "binary search finds middle");
    check(cf_ks_get(&ks, "a.a", 3) != NULL, "binary search finds first");
    check(cf_ks_get(&ks, "z.z", 3) != NULL, "binary search finds last");
    check(cf_ks_get(&ks, "n.n", 3) == NULL, "binary search rejects absent");
}

static void build(cf_keyspace* ks, cf_arena* ar, int reverse) {
    cf_ks_init(ks, ar, 4);
    if (reverse) {
        add_text(ks, "server.host", SRC_DEFAULT, "0.0.0.0", NULL);
        add_int(ks, "server.port", SRC_ENV, 9090, "E");
        add_int(ks, "server.port", SRC_DEFAULT, 8080, NULL);
    } else {
        add_int(ks, "server.port", SRC_DEFAULT, 8080, NULL);
        add_int(ks, "server.port", SRC_ENV, 9090, "OTHER_NAME");
        add_text(ks, "server.host", SRC_DEFAULT, "0.0.0.0", NULL);
    }
    cf_ks_resolve(ks);
}

static void test_determinism(cf_arena* ar) {
    cf_keyspace a, b;

    build(&a, ar, 0);
    build(&b, ar, 1);

    /* Same resolved configuration, different insertion order and
       different locations: the digest must agree, because it is over
       the resolved values and not over how they got there. */
    check(cf_ks_digest(&a) == cf_ks_digest(&b),
          "digest is independent of insertion order");

    {
        cf_keyspace c;
        cf_ks_init(&c, ar, 4);
        add_int(&c, "server.port", SRC_ENV, 9091, NULL);
        add_text(&c, "server.host", SRC_DEFAULT, "0.0.0.0", NULL);
        cf_ks_resolve(&c);
        check(cf_ks_digest(&a) != cf_ks_digest(&c),
              "digest changes when a value changes");
    }
    {
        cf_keyspace d;
        cf_ks_init(&d, ar, 4);
        add_text(&d, "server.port", SRC_ENV, "9090", NULL);
        add_text(&d, "server.host", SRC_DEFAULT, "0.0.0.0", NULL);
        cf_ks_resolve(&d);
        check(cf_ks_digest(&a) != cf_ks_digest(&d),
              "digest distinguishes int 9090 from text 9090");
    }
}

static void test_exhaustion(void) {
    static char buf[4096];
    cf_arena*   ar = cf_arena_fixed(buf, sizeof buf);
    cf_keyspace ks;
    int         added = 0;
    char        key[32];
    int         i;

    check(cf_ks_init(&ks, ar, 4), "init inside a fixed arena");
    /* A key space whose init failed must refuse work rather than
       crash: found by AddressSanitizer, not by reading. */
    {
        cf_keyspace dead;
        cf_value    v;
        static char tiny[8];
        cf_arena*   small = cf_arena_fixed(tiny, sizeof tiny);
        cf_value_int(&v, 1);
        dead.cands = NULL;
        check(!cf_ks_init(&dead, small, 4) || small == NULL,
              "init fails in a hopeless arena");
        check(!cf_ks_add(&dead, "a.b", 3, 0, &v, NULL),
              "a failed key space refuses adds");
    }
    for (i = 0; i < 500; ++i) {
        cf_value v;
        size_t   n = (size_t)cf_fmt(key, sizeof key, "k.%d", i);
        cf_value_int(&v, (cf_i64)i);
        if (!cf_ks_add(&ks, key, n, SRC_DEFAULT, &v, NULL)) {
            break;
        }
        ++added;
    }
    /* Exhaustion must be a clean refusal, not a crash and not silent
       growth. This is the freestanding contract. */
    check(added > 0, "some keys fit");
    check(added < 500, "arena refuses once full");
    cf_ks_resolve(&ks);
    /* Resolution needs scratch space of its own, so a space built to
       the very edge of a fixed arena may not be resolvable. Either
       outcome is acceptable; silently producing a wrong count is not. */
    check(cf_ks_count(&ks) == (size_t)added || cf_ks_count(&ks) == 0,
          "count is exact or resolution refused, never wrong");
}

int main(void) {
    cf_arena* ar = cf_arena_new(NULL, 0);

    test_key_validation();
    test_precedence(ar);
    test_rejected_is_skipped(ar);
    test_sorted_and_search(ar);
    test_determinism(ar);
    test_exhaustion();

    cf_arena_free(ar);
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
