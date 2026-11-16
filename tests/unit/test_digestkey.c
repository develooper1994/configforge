/*
 * test_digestkey.c - a secret's digest, keyed (D-85).
 *
 * SipHash-2-4 against the reference implementation's vectors, and what
 * the library does with a key a program gives its builder: every digest
 * a report and the fingerprint stand for a secret in is then
 * "sip24:<id>:<16 hex digits>", the same for one key and different for
 * another, and the unkeyed FNV-1a digest is what it was without one.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#include "cf_test.h"

static cf_u64 dk_pair(unsigned long hi, unsigned long lo) {
    return ((cf_u64)hi << 32) | (cf_u64)lo;
}

static void dk_vectors(void) {
    unsigned char key[16];
    unsigned char msg[15];
    unsigned      i;

    for (i = 0; i < 16; ++i) {
        key[i] = (unsigned char)i;
    }
    for (i = 0; i < 15; ++i) {
        msg[i] = (unsigned char)i;
    }
    check(cf_siphash24(key, msg, 0) == dk_pair(0x726fdb47UL, 0xdd0e0e31UL),
          "SipHash-2-4 of the empty message is the reference's");
    check(cf_siphash24(key, msg, 15) == dk_pair(0xa129ca61UL, 0x49be45e5UL),
          "SipHash-2-4 of the paper's 15 bytes is the reference's");
}

static const unsigned char dk_key_a[16] = {1, 2,  3,  4,  5,  6,  7,  8,
                                           9, 10, 11, 12, 13, 14, 15, 16};
static const unsigned char dk_key_b[16] = {16, 15, 14, 13, 12, 11, 10, 9,
                                           8,  7,  6,  5,  4,  3,  2,  1};

static cf_config* dk_build(const unsigned char* key, cf_error* err) {
    static const cf_pair pairs[] = {{"db.password", "1234"}, {NULL, NULL}};
    cf_builder*          b       = cf_build(NULL);

    cf_builder_memory(b, "mem", CF_RANK_USER_FILE, pairs, 1);
    cf_builder_text(b, "db.password", NULL, CF_SENSITIVE);
    cf_builder_text(b, "db.host", "localhost", 0);
    if (key != NULL) {
        cf_builder_digest_key(b, key, 16);
    }
    return cf_builder_create(b, err);
}

static size_t dk_report(cf_config* c, cf_report_mode mode, char* buf, size_t cap) {
    cf_report_info info = CF_REPORT_INFO_INIT;

    info.mode = mode;
    return cf_report(c, &info, buf, cap);
}

static const char dk_field[] = "\"digest\": \"";

/* The first "digest" of a report, as its text. */
static void dk_digest_of(cf_config* c, cf_report_mode mode, char* out, size_t cap) {
    static char buf[16384];
    const char* at;
    const char* end;

    out[0] = '\0';
    (void)dk_report(c, mode, buf, sizeof buf);
    at = strstr(buf, dk_field);
    if (at == NULL) {
        return;
    }
    at += sizeof dk_field - 1;
    end = strchr(at, '"');
    if (end != NULL && (size_t)(end - at) < cap) {
        memcpy(out, at, (size_t)(end - at));
        out[end - at] = '\0';
    }
}

static void dk_keyed(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  a1  = dk_build(dk_key_a, &err);
    cf_config*  a2  = dk_build(dk_key_a, &err);
    cf_config*  b1  = dk_build(dk_key_b, &err);
    cf_config*  n1  = dk_build(NULL, &err);
    cf_config*  d   = NULL;
    char        da1[40], da2[40], db1[40], dn1[40], dd[40], plain[40], viaapi[40];
    char        fa[32], fa2[32], fb[32], fn[32], fd[32];
    cf_value    v;
    static char full[16384];

    if (a1 == NULL || a2 == NULL || b1 == NULL || n1 == NULL) {
        check(0, "the four configurations build");
        printf("  %s\n", err.message);
        cf_config_free(a1);
        cf_config_free(a2);
        cf_config_free(b1);
        cf_config_free(n1);
        return;
    }
    v.type  = CF_TYPE_TEXT;
    v.u.t.p = "1234";
    v.u.t.n = 4;
    (void)cf_ks_digest_text(cf_ks_value_digest(&v), plain, sizeof plain);
    check(cf_ks_secret_digest_text(&v, NULL, viaapi, sizeof viaapi) == 22 &&
              strcmp(viaapi, plain) == 0,
          "the shared function, unkeyed, is the FNV-1a digest");
    check(cf_ks_secret_digest_text(&v, dk_key_a, da1, sizeof da1) == 31 &&
              strncmp(da1, "sip24:", 6) == 0 && strlen(da1) == 31 && da1[14] == ':',
          "keyed, it is sip24:, eight hex digits of id, a colon and sixteen more");
    (void)cf_ks_secret_digest_text(&v, dk_key_b, db1, sizeof db1);
    check(strcmp(da1, db1) != 0 && strncmp(da1, db1, 14) != 0,
          "another key gives another id and another digest");
    check(cf_ks_secret_digest_text(&v, dk_key_a, da2, 10) == 31 && strlen(da2) == 9,
          "and writes as far as the buffer allows");

    if (cf_has_feature("report")) {
        dk_digest_of(a1, CF_REPORT_BLIND, da2, sizeof da2);
        check(strncmp(da2, "sip24:", 6) == 0 && strlen(da2) == 31,
              "a keyed blind report's digest is sip24: and 31 characters");
        check(strcmp(da1, da2) == 0, "and is the shared function's");
        dk_digest_of(a2, CF_REPORT_BLIND, dd, sizeof dd);
        check(strcmp(dd, da2) == 0, "the same in a second configuration, one key");
        dk_digest_of(b1, CF_REPORT_BLIND, dd, sizeof dd);
        check(strcmp(dd, db1) == 0 && strcmp(dd, da2) != 0,
              "another key gives another digest");
        dk_digest_of(n1, CF_REPORT_BLIND, dn1, sizeof dn1);
        check(strncmp(dn1, "fnv1a:", 6) == 0 && strcmp(dn1, plain) == 0,
              "no key gives the FNV-1a digest, as before");
    }

    (void)cf_fingerprint_format(cf_fingerprint(a1), fa, sizeof fa);
    (void)cf_fingerprint_format(cf_fingerprint(a2), fa2, sizeof fa2);
    (void)cf_fingerprint_format(cf_fingerprint(b1), fb, sizeof fb);
    (void)cf_fingerprint_format(cf_fingerprint(n1), fn, sizeof fn);
    check(strcmp(fa, fn) != 0, "the fingerprint differs with and without the key");
    check(strcmp(fa, fa2) == 0, "and is stable under one key");
    check(strcmp(fa, fb) != 0, "and differs under another");

    d = cf_derive(a1, "more", NULL, 0, &err);
    check(d != NULL, "a keyed configuration derives");
    if (d != NULL) {
        (void)cf_fingerprint_format(cf_fingerprint(d), fd, sizeof fd);
        check(strcmp(fd, fa) == 0, "a derived configuration keeps the key");
        if (cf_has_feature("report")) {
            dk_digest_of(d, CF_REPORT_BLIND, dd, sizeof dd);
            check(strcmp(dd, da1) == 0, "and its report's digest");
        }
        cf_config_free(d);
    }

    if (cf_has_feature("report")) {
        const char* at;

        (void)dk_report(a1, CF_REPORT_FULL, full, sizeof full);
        check(strstr(full, "\"digest\": \"sip24:") != NULL,
              "a full report holds the keyed digest of a secret");
        check(strstr(full, "1234") != NULL, "beside its value");
        at = strstr(full, dk_field);
        check(at != NULL && strncmp(at + sizeof dk_field - 1, da1, 31) == 0,
              "and it is the blind report's");
        (void)dk_report(n1, CF_REPORT_FULL, full, sizeof full);
        check(strstr(full, "\"digest\": \"fnv1a:") != NULL,
              "an unkeyed full report holds the FNV-1a digest of a secret");
    }

    cf_config_free(a1);
    cf_config_free(a2);
    cf_config_free(b1);
    cf_config_free(n1);
}

static void dk_refused(void) {
    cf_error    err  = CF_ERROR_INIT;
    cf_error    err2 = CF_ERROR_INIT;
    cf_builder* b    = cf_build(NULL);
    cf_config*  c;

    check(!cf_builder_digest_key(b, dk_key_a, 15), "a 15-byte key is refused");
    c = cf_builder_create(b, &err);
    check(c == NULL, "and the builder fails");
    check(err.code == CF_E_POLICY, "with the policy code");
    check_text(err.message, "a digest key is 16 bytes, not 15", "and says so");
    cf_config_free(c);

    b = cf_build(NULL);
    check(!cf_builder_digest_key(b, NULL, 16), "a NULL key is refused");
    c = cf_builder_create(b, &err2);
    check(c == NULL, "and the builder fails");
    check_text(err2.message, "a digest key is 16 bytes, not 0", "saying 0");
    cf_config_free(c);
}

int main(void) {
    dk_vectors();
    dk_keyed();
    dk_refused();
    return cf_test_done();
}
