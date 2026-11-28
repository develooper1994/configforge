/*
 * serve.c - a running program answers one request for its report (D-123).
 *
 * The program's own loop accepted a connection and calls this on it: one
 * request is read, one answer written, and the call returns. No thread,
 * no listener, no state between calls; the descriptor stays the
 * program's. The wire is an HTTP/1.0 subset, GET /report, so that
 * curl --unix-socket reaches it. The answer is always the value-blind
 * report.
 */

#include "configforge/configforge.h"
#include "configforge/cf_alloc.h"
#include "configforge/cf_error.h"
#include "configforge/cf_platform.h"
#include "configforge/cf_port.h"
#include "configforge/cf_report.h"

#include <string.h>

#define SRV_HEAD_MAX 4096
#define SRV_LINE_MAX 1024

/* What the request was, as far as it decides the answer. */
typedef enum srv_verdict {
    SRV_REPORT,
    SRV_NOT_FOUND,
    SRV_BAD_METHOD,
    SRV_BAD_REQUEST
} srv_verdict;

/* Wiped through a volatile pointer, which a compiler cannot see through. */
static void srv_wipe(char* p, size_t n) {
    volatile char* v = p;
    while (n > 0) {
        *v++ = 0;
        --n;
    }
}

/* Is there a blank line in buf[0, n)? \r\n\r\n or \n\n. */
static cf_bool srv_has_blank(const char* buf, size_t n) {
    size_t i;
    for (i = 0; i + 1 < n; ++i) {
        if (buf[i] == '\n' && buf[i + 1] == '\n') {
            return CF_TRUE;
        }
        if (i + 3 < n && buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' &&
            buf[i + 3] == '\n') {
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}

/* Read until the blank line or SRV_HEAD_MAX bytes. 1 with the bytes in
   *got, 0 when the peer closed with nothing, -1 on failure, -2 on timeout.
   A request that ends without its blank line is judged by what it holds. */
static int srv_read_request(int fd, char* buf, size_t* got, long timeout_ms) {
    size_t used = 0;
    while (used < SRV_HEAD_MAX && !srv_has_blank(buf, used)) {
        long k = cf_platform_fd_read(fd, buf + used, SRV_HEAD_MAX - used, timeout_ms);
        if (k == -2) {
            return -2;
        }
        if (k < 0) {
            return -1;
        }
        if (k == 0) {
            if (used == 0) {
                return 0;
            }
            break;
        }
        used += (size_t)k;
    }
    *got = used;
    return 1;
}

/* The request line, judged. */
static srv_verdict srv_parse(const char* buf, size_t n) {
    size_t line = 0;
    size_t m;
    size_t t;
    size_t v;

    while (line < n && buf[line] != '\n') {
        ++line;
    }
    if (line == n) {
        /* No end of line at all: the first line is longer than the buffer
           or the request stopped; neither is a request. */
        return SRV_BAD_REQUEST;
    }
    if (line > 0 && buf[line - 1] == '\r') {
        --line;
    }
    if (line > SRV_LINE_MAX) {
        return SRV_BAD_REQUEST;
    }
    if (!srv_has_blank(buf, n)) {
        return SRV_BAD_REQUEST; /* headers over SRV_HEAD_MAX */
    }
    /* METHOD SP TARGET SP HTTP/1.x */
    m = 0;
    while (m < line && buf[m] != ' ') {
        ++m;
    }
    if (m == 0 || m >= line) {
        return SRV_BAD_REQUEST;
    }
    t = m + 1;
    v = t;
    while (v < line && buf[v] != ' ') {
        ++v;
    }
    if (v == t || v >= line || line - (v + 1) < 5 ||
        memcmp(buf + v + 1, "HTTP/", 5) != 0) {
        return SRV_BAD_REQUEST;
    }
    if (m != 3 || memcmp(buf, "GET", 3) != 0) {
        return SRV_BAD_METHOD;
    }
    if (v - t == 7 && memcmp(buf + t, "/report", 7) == 0) {
        return SRV_REPORT;
    }
    return SRV_NOT_FOUND;
}

/* One answer: the head built in a small buffer, then the body. */
static cf_bool srv_answer(int fd, long timeout_ms, const char* status,
                          const char* extra, const char* type, const char* body,
                          size_t n) {
    char   head[256];
    size_t h = cf_fmt(head, sizeof head,
                      "HTTP/1.0 %s\r\n%sContent-Type: %s\r\nContent-Length: %zu\r\n"
                      "Connection: close\r\n\r\n",
                      status, extra, type, n);
    if (h == 0 || h >= sizeof head) {
        return CF_FALSE;
    }
    if (!cf_platform_fd_write(fd, head, h, timeout_ms)) {
        return CF_FALSE;
    }
    return n == 0 ? CF_TRUE : cf_platform_fd_write(fd, body, n, timeout_ms);
}

static cf_bool srv_text(int fd, long timeout_ms, const char* status, const char* extra,
                        const char* body) {
    return srv_answer(fd, timeout_ms, status, extra, "text/plain", body, strlen(body));
}

/* The report, built whole before the first byte goes out. */
static cf_bool srv_report(const cf_config* c, int fd, const cf_serve_options* o,
                          cf_error* err) {
    const cf_allocator* a;
    char*               buf;
    size_t              n;
    cf_bool             ok;

    n = cf_report(c, o->info, NULL, 0);
    if (n == 0) {
        ok = srv_text(fd, o->timeout_ms, "501 Not Implemented", "",
                      "no report in this build\n");
        if (!ok) {
            (void)cf_error_set(err, CF_E_SOURCE, NULL, NULL,
                               "cf_serve_report: cannot write the answer");
        }
        return ok;
    }
    a   = cf_default_allocator();
    buf = (a != NULL) ? (char*)a->alloc(a->ctx, n + 1) : NULL;
    if (buf == NULL) {
        (void)cf_error_set(err, CF_E_MEMORY, NULL, NULL,
                           "cf_serve_report: out of memory for the report");
        return CF_FALSE;
    }
    (void)cf_report(c, o->info, buf, n + 1);
    ok = srv_answer(fd, o->timeout_ms, "200 OK", "Cache-Control: no-store\r\n",
                    "application/json", buf, n);
    srv_wipe(buf, n + 1);
    a->release(a->ctx, buf);
    if (!ok) {
        (void)cf_error_set(err, CF_E_SOURCE, NULL, NULL,
                           "cf_serve_report: cannot write the answer");
    }
    return ok;
}

static cf_bool srv_serve(const cf_config* c, int fd, const cf_serve_options* o,
                         char* req, cf_error* err) {
    size_t      got = 0;
    int         r;
    srv_verdict v;
    cf_bool     ok;

    r = srv_read_request(fd, req, &got, o->timeout_ms);
    if (r == -2) {
        return cf_error_set(err, CF_E_SOURCE, NULL, NULL,
                            "cf_serve_report: no request within %I ms",
                            (cf_i64)o->timeout_ms);
    }
    if (r == 0) {
        return cf_error_set(err, CF_E_SOURCE, NULL, NULL,
                            "cf_serve_report: the peer closed before a request");
    }
    if (r < 0) {
        return cf_error_set(err, CF_E_SOURCE, NULL, NULL,
                            "cf_serve_report: cannot read the request");
    }
    v = srv_parse(req, got);
    if (v == SRV_REPORT) {
        return srv_report(c, fd, o, err);
    }
    if (v == SRV_NOT_FOUND) {
        ok = srv_text(fd, o->timeout_ms, "404 Not Found", "", "not found\n");
    } else if (v == SRV_BAD_METHOD) {
        ok = srv_text(fd, o->timeout_ms, "405 Method Not Allowed", "Allow: GET\r\n",
                      "method not allowed\n");
    } else {
        ok = srv_text(fd, o->timeout_ms, "400 Bad Request", "", "bad request\n");
    }
    if (!ok) {
        (void)cf_error_set(err, CF_E_SOURCE, NULL, NULL,
                           "cf_serve_report: cannot write the answer");
    }
    return ok;
}

void cf_serve_options_init(cf_serve_options* o) {
    if (o == NULL) {
        return;
    }
    o->size       = sizeof *o;
    o->info       = NULL;
    o->timeout_ms = 2000;
}

cf_bool cf_serve_report(const cf_config* c, int fd, const cf_serve_options* opt,
                        cf_error* err) {
    cf_serve_options        defaults;
    const cf_serve_options* o = opt;
    const cf_allocator*     a;
    char*                   req;
    int                     was;
    cf_bool                 ok;

    if (!cf_platform_has_fd_io()) {
        return cf_error_set(err, CF_E_CAPABILITY, NULL, NULL,
                            "cf_serve_report needs a POSIX descriptor");
    }
    if (c == NULL) {
        return cf_error_set(err, CF_E_SOURCE, NULL, NULL,
                            "cf_serve_report: no configuration");
    }
    if (o == NULL) {
        cf_serve_options_init(&defaults);
        o = &defaults;
    } else if (o->size < sizeof(cf_serve_options)) {
        return cf_error_set(err, CF_E_CAPABILITY, NULL, NULL,
                            "cf_serve_report: options did not start from "
                            "CF_SERVE_OPTIONS_INIT");
    }
    if (o->info != NULL && o->info->size >= sizeof(cf_report_info) &&
        o->info->mode == CF_REPORT_FULL) {
        return cf_error_set(err, CF_E_POLICY, NULL, NULL,
                            "cf_serve_report: a report served on a descriptor is "
                            "value-blind");
    }
    if (!cf_platform_fd_is_socket(fd)) {
        return cf_error_set(err, CF_E_SOURCE, NULL, NULL,
                            "cf_serve_report: the descriptor is not a socket");
    }
    a   = cf_default_allocator();
    req = (a != NULL) ? (char*)a->alloc(a->ctx, SRV_HEAD_MAX) : NULL;
    if (req == NULL) {
        return cf_error_set(err, CF_E_MEMORY, NULL, NULL,
                            "cf_serve_report: out of memory for the request");
    }
    was = cf_platform_fd_nonblock(fd, 1);
    if (was < 0) {
        a->release(a->ctx, req);
        return cf_error_set(err, CF_E_SOURCE, NULL, NULL,
                            "cf_serve_report: cannot read the request");
    }
    ok = srv_serve(c, fd, o, req, err);
    if (was == 0) {
        (void)cf_platform_fd_nonblock(fd, 0);
    }
    a->release(a->ctx, req);
    return ok;
}
