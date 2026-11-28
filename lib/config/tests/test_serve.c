/*
 * test_serve.c - cf_serve_report: one request, one answer, on a descriptor
 * the program's own loop accepted (D-123, C-10a). C89.
 */

/* Before any header: what socketpair, shutdown and fcntl are declared by. */
#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200112L /* NOLINT(bugprone-reserved-identifier) */
#endif

#include <configforge/configforge.h>
#include <configforge/cf_platform_file.h>

#include <stdio.h>
#include <string.h>

#include "cf_test.h"

#if CF_PLATFORM_POSIX && CF_WITH_REPORT

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#define OUT_CAP 65536
#define REQ "GET /report HTTP/1.0\r\n\r\n"

static cf_config* make(void) {
    static const cf_pair pairs[] = {{"db.password", "MARKER-7731"}};
    cf_error             err     = CF_ERROR_INIT;
    cf_builder*          b       = cf_build(NULL);
    cf_config*           c;

    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "db.password", NULL, CF_SENSITIVE);
    cf_builder_memory(b, "mem", 10, pairs, 1);
    c = cf_builder_create(b, &err);
    check(c != NULL, "the configuration builds");
    return c;
}

static void pair(int sv[2]) {
    int size = 262144;
    check(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0, "a socket pair");
    setsockopt(sv[0], SOL_SOCKET, SO_SNDBUF, &size, sizeof size);
    setsockopt(sv[0], SOL_SOCKET, SO_RCVBUF, &size, sizeof size);
    setsockopt(sv[1], SOL_SOCKET, SO_SNDBUF, &size, sizeof size);
    setsockopt(sv[1], SOL_SOCKET, SO_RCVBUF, &size, sizeof size);
}

static void put(int fd, const char* s, size_t n) {
    while (n > 0) {
        long k = (long)write(fd, s, n);
        if (k <= 0) {
            check(0, "the request is written");
            return;
        }
        s += k;
        n -= (size_t)k;
    }
}

/* Write `request` to one end, serve on the other, read the answer to the end. */
static void ask(const cf_config* c, const char* request, size_t rn,
                const cf_serve_options* opt, char* out, size_t cap, cf_bool* ok,
                cf_error* err) {
    int    sv[2];
    size_t used = 0;
    long   k;

    pair(sv);
    put(sv[0], request, rn);
    *ok = cf_serve_report(c, sv[1], opt, err);
    shutdown(sv[1], SHUT_WR);
    while (used + 1 < cap && (k = (long)read(sv[0], out + used, cap - 1 - used)) > 0) {
        used += (size_t)k;
    }
    out[used] = '\0';
    close(sv[0]);
    close(sv[1]);
}

static void get(const cf_config* c, const char* request, const cf_serve_options* opt,
                char* out, cf_bool* ok, cf_error* err) {
    ask(c, request, strlen(request), opt, out, OUT_CAP, ok, err);
}

static const char* body_of(const char* out) {
    const char* p = strstr(out, "\r\n\r\n");
    return p != NULL ? p + 4 : out + strlen(out);
}

static void case_a(const cf_config* c, char* out) {
    cf_error    err = CF_ERROR_INIT;
    cf_bool     ok  = CF_FALSE;
    const char* at;
    char        want[64];

    get(c, "GET /report HTTP/1.0\r\n\r\n", NULL, out, &ok, &err);
    check(ok, "a: GET /report is answered");
    check(strncmp(out, "HTTP/1.0 200 OK\r\n", 17) == 0, "a: 200 OK, HTTP/1.0");
    has(out, "Content-Type: application/json", "a: JSON");
    has(out, "Connection: close", "a: closes");
    has(out, "Cache-Control: no-store", "a: not cached");
    at = body_of(out);
    sprintf(want, "Content-Length: %lu\r\n", (unsigned long)strlen(at));
    has(out, want, "a: the length is the body's");
    check(at[0] == '{', "a: the body is JSON");
    has(at, "\"configforge_report\": 1", "a: a report");
    has(at, "\"mode\": \"blind\"", "a: blind");
    has(at, "\"server.port\"", "a: the keys");
    lacks(out, "MARKER-7731", "a: never the secret");
}

static void case_b(const cf_config* c, char* out) {
    cf_error         err  = CF_ERROR_INIT;
    cf_bool          ok   = CF_FALSE;
    cf_report_info   info = CF_REPORT_INFO_INIT;
    cf_serve_options opt  = CF_SERVE_OPTIONS_INIT;
    int              sv[2];
    char             one;

    info.application = "myapp";
    info.profile     = "prod";
    opt.info         = &info;
    get(c, "GET /report HTTP/1.0\r\n\r\n", &opt, out, &ok, &err);
    check(ok, "b: answered with a name");
    has(out, "\"profile\": \"prod\"", "b: the profile");
    has(out, "\"name\": \"myapp\"", "b: the application");
    lacks(out, "MARKER-7731", "b: never the secret");

    info.mode = CF_REPORT_FULL;
    cf_error_clear(&err);
    pair(sv);
    put(sv[0], REQ, strlen(REQ));
    ok = cf_serve_report(c, sv[1], &opt, &err);
    shutdown(sv[1], SHUT_WR);
    check(!ok, "b: a full report is refused");
    check(err.code == CF_E_POLICY, "b: as policy");
    has(err.message, "value-blind", "b: and says why");
    check(read(sv[0], &one, 1) == 0, "b: nothing was written");
    close(sv[0]);
    close(sv[1]);
}

static void case_c(const cf_config* c, char* out) {
    cf_error    err = CF_ERROR_INIT;
    cf_bool     ok  = CF_FALSE;
    static char big[5001];

    get(c, "GET /elsewhere HTTP/1.1\r\nHost: x\r\n\r\n", NULL, out, &ok, &err);
    check(ok, "c: another target is answered");
    has(out, "HTTP/1.0 404 Not Found\r\n", "c: 404");
    check(strcmp(body_of(out), "not found\n") == 0, "c: the body");

    get(c, "GET /report?x=1 HTTP/1.0\r\n\r\n", NULL, out, &ok, &err);
    check(ok, "c: a query string is answered");
    has(out, "404 Not Found", "c: 404 with a query");

    get(c, "POST /report HTTP/1.0\r\n\r\n", NULL, out, &ok, &err);
    check(ok, "c: another method is answered");
    has(out, "HTTP/1.0 405 Method Not Allowed\r\n", "c: 405");
    has(out, "Allow: GET\r\n", "c: Allow");
    check(strcmp(body_of(out), "method not allowed\n") == 0, "c: 405 body");

    get(c, "GET /report\r\n\r\n", NULL, out, &ok, &err);
    check(ok, "c: no version is answered");
    has(out, "HTTP/1.0 400 Bad Request\r\n", "c: 400 without a version");
    check(strcmp(body_of(out), "bad request\n") == 0, "c: 400 body");

    memset(big, 'a', sizeof big);
    memcpy(big, "GET /", 5);
    memcpy(big + 2000, " HTTP/1.0\r\n\r\n", 14);
    get(c, big, NULL, out, &ok, &err);
    check(ok, "c: a long first line is answered");
    has(out, "400 Bad Request", "c: 400 for a long first line");

    memset(big, 'a', sizeof big);
    memcpy(big, "GET /report HTTP/1.0\r\nX: ", 25);
    ask(c, big, 5000, NULL, out, OUT_CAP, &ok, &err);
    check(ok, "c: long headers are answered");
    has(out, "400 Bad Request", "c: 400 for long headers");
}

static void case_d(const cf_config* c, char* out) {
    cf_error err = CF_ERROR_INIT;
    cf_bool  ok  = CF_FALSE;

    get(c, "GET /report HTTP/1.0\n\n", NULL, out, &ok, &err);
    check(ok, "d: a bare newline ends the headers");
    check(strncmp(out, "HTTP/1.0 200 OK\r\n", 17) == 0, "d: 200");
    lacks(out, "MARKER-7731", "d: never the secret");
}

static void case_e(const cf_config* c) {
    cf_error         err = CF_ERROR_INIT;
    cf_serve_options opt = CF_SERVE_OPTIONS_INIT;
    int              sv[2];
    int              before;
    int              after;

    opt.timeout_ms = 50;
    pair(sv);
    put(sv[0], "GET /rep", 8);
    before = fcntl(sv[1], F_GETFL);
    check(!cf_serve_report(c, sv[1], &opt, &err), "e: a request that stops is refused");
    after = fcntl(sv[1], F_GETFL);
    has(err.message, "no request within 50 ms", "e: the message");
    check(err.code == CF_E_SOURCE, "e: as a source error");
    check(before == after, "e: the flags are restored");
    close(sv[0]);
    close(sv[1]);
}

static void case_f(const cf_config* c) {
    cf_error    err = CF_ERROR_INIT;
    cf_bool     ok  = CF_FALSE;
    static char out[OUT_CAP];
    int         sv[2];

    pair(sv);
    close(sv[0]);
    check(!cf_serve_report(c, sv[1], NULL, &err), "f: a closed peer is refused");
    has(err.message, "the peer closed before a request", "f: the message");
    close(sv[1]);

    get(c, "GET /report HTTP/1.0\r\n\r\n", NULL, out, &ok, &err);
    check(ok, "f: the process lives and the next call works");
}

static void case_g(const cf_config* c) {
    cf_error err = CF_ERROR_INIT;
    int      sv[2];

    pair(sv);
    put(sv[0], REQ, strlen(REQ));
    close(sv[0]);
    check(!cf_serve_report(c, sv[1], NULL, &err), "g: a peer that left is refused");
    has(err.message, "cannot write the answer", "g: the message");
    close(sv[1]);
}

static void case_h(const cf_config* c) {
    cf_error err = CF_ERROR_INIT;
    int      fd  = open("/dev/null", O_RDWR);
    int      p[2];

    check(fd >= 0, "h: /dev/null opens");
    check(!cf_serve_report(c, fd, NULL, &err), "h: a file is refused");
    has(err.message, "the descriptor is not a socket", "h: the message");
    close(fd);

    check(pipe(p) == 0, "h: a pipe");
    cf_error_clear(&err);
    check(!cf_serve_report(c, p[0], NULL, &err), "h: a pipe is refused");
    has(err.message, "the descriptor is not a socket", "h: the pipe's message");
    close(p[0]);
    close(p[1]);
}

static void case_i(const cf_config* c, char* out) {
    cf_error         err = CF_ERROR_INIT;
    cf_bool          ok  = CF_FALSE;
    cf_serve_options opt = CF_SERVE_OPTIONS_INIT;
    cf_serve_options o2;
    int              sv[2];

    opt.size = 0;
    pair(sv);
    check(!cf_serve_report(c, sv[1], &opt, &err), "i: a size of 0 is refused");
    has(err.message, "did not start from CF_SERVE_OPTIONS_INIT", "i: the message");
    check(err.code == CF_E_CAPABILITY, "i: as capability");
    close(sv[0]);
    close(sv[1]);

    get(c, "GET /report HTTP/1.0\r\n\r\n", NULL, out, &ok, &err);
    check(ok, "i: NULL is the defaults");

    cf_serve_options_init(&o2);
    check(o2.size == sizeof o2 && o2.info == NULL && o2.timeout_ms == 2000,
          "i: cf_serve_options_init sets the defaults");
    cf_serve_options_init(NULL);

    cf_error_clear(&err);
    pair(sv);
    check(!cf_serve_report(NULL, sv[1], NULL, &err), "i: no configuration is refused");
    check(err.code == CF_E_SOURCE, "i: as a source error");
    close(sv[0]);
    close(sv[1]);
}

static void case_j(const cf_config* c) {
    cf_error err = CF_ERROR_INIT;
    int      sv[2];

    pair(sv);
    put(sv[0], REQ, strlen(REQ));
    check(cf_serve_report(c, sv[1], NULL, &err), "j: answered");
    check(fcntl(sv[1], F_GETFD) != -1, "j: the descriptor is still open");
    close(sv[0]);
    close(sv[1]);
}

int main(void) {
    static char out[OUT_CAP];
    cf_config*  c = make();

    case_a(c, out);
    case_b(c, out);
    case_c(c, out);
    case_d(c, out);
    case_e(c);
    case_f(c);
    case_g(c);
    case_h(c);
    case_i(c, out);
    case_j(c);
    cf_config_free(c);
    return cf_test_done();
}

#elif CF_WITH_REPORT

/* No POSIX descriptor: the call says so (ACCEPT-WINDOWS). */
int main(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;

    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    c = cf_builder_create(b, &err);
    check(c != NULL, "the configuration builds");
    check(!cf_serve_report(c, 0, NULL, &err), "no descriptor I/O: false");
    check(err.code == CF_E_CAPABILITY, "as capability");
    has(err.message, "needs a POSIX descriptor", "and says what it needs");
    cf_config_free(c);
    return cf_test_done();
}

#else

int main(void) {
    printf("skipped: no CF_WITH_REPORT\n");
    return cf_test_done();
}

#endif
