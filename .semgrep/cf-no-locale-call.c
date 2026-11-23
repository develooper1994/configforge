/* Fixture for cf-no-locale-call: not compiled, not formatted. */
void fixture(const char* s, char* buf, unsigned n) {
    double d;
    int    c;
    // ruleid: cf-no-locale-call
    d = strtod(s, 0);
    // ruleid: cf-no-locale-call
    c = toupper(s[0]);
    // ruleid: cf-no-locale-call
    snprintf(buf, n, "%d", 1);
    // ruleid: cf-no-locale-call
    setlocale(0, "C");
    // ok: cf-no-locale-call
    d = cf_f64_from_text(s, 0);
    // ok: cf-no-locale-call
    c = my_isdigit_table[0];
    /* printf("not a call") */
    // ok: cf-no-locale-call
    c = cf_ascii_upper(s[0]);
}
