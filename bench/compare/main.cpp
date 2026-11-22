// cfb_compare - one cell of the comparison: one library reading one document
// (D-111, docs/plan/P-word-at-a-time.md, P-1b).
//
//   cfb_compare --list
//   cfb_compare --lib <name> --doc <file> --mode speed|count|memory|probe
//               [--reps N] [--strict|--loose] [--cold]
//
// One JSON object on one line, except `--mode probe`, which prints one text
// line a library, format and mode. The documents are read as files, made by
// `cf_bench --emit-docs`: this program generates nothing, so every library is
// given the same bytes. `--mode count` parses once, in cfb_cell, which is
// what `valgrind --tool=callgrind --toggle-collect=cfb_cell` counts.
#include "touch.h"

#include <sys/resource.h>
#include <time.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

std::vector<const Adapter*>& cfb_registry();

namespace {

double now() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

// One document: the bytes, with 64 bytes of padding that are zero (simdjson
// reads past the end; the rest ignore it). `many-tiny` is a .bin of documents
// of 20 to 64 bytes and an .idx of their lengths, four bytes each.
struct Input {
    std::string         path;
    char*               data = nullptr;
    size_t              n    = 0;
    std::vector<size_t> lens; // many-tiny: one length a document
};

bool read_whole(const std::string& path, char** data, size_t* n) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f)
        return false;
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    char* b = static_cast<char*>(std::malloc((size_t)sz + 64));
    if (!b) {
        std::fclose(f);
        return false;
    }
    size_t got = std::fread(b, 1, (size_t)sz, f);
    std::fclose(f);
    std::memset(b + got, 0, 64);
    *data = b;
    *n    = got;
    return got == (size_t)sz;
}

bool load(Input& in, const std::string& path) {
    in.path = path;
    if (!read_whole(path, &in.data, &in.n))
        return false;
    if (path.size() > 4 && path.compare(path.size() - 4, 4, ".bin") == 0) {
        std::string idx = path.substr(0, path.size() - 4) + ".idx";
        char*       d;
        size_t      n;
        if (!read_whole(idx, &d, &n))
            return false;
        for (size_t i = 0; i + 4 <= n; i += 4) {
            const unsigned char* u = reinterpret_cast<const unsigned char*>(d + i);
            in.lens.push_back((size_t)u[0] | ((size_t)u[1] << 8) |
                              ((size_t)u[2] << 16) | ((size_t)u[3] << 24));
        }
        std::free(d);
    }
    return true;
}

bool parse_all(const Adapter* a, const Input& in, bool strict, Touch& t) {
    if (in.lens.empty())
        return a->parse(in.data, in.n, strict, t);
    const char* p  = in.data;
    bool        ok = true;
    for (size_t len : in.lens) {
        ok = a->parse(p, len, strict, t) && ok;
        p += len;
    }
    return ok;
}

} // namespace

// The one function that parses once; callgrind collects it and nothing else
// (--toggle-collect=cfb_cell): extern "C", so that its name is the plain one.
#ifdef CFB_NO_NOINLINE
#define CFB_NOINLINE
#else
#define CFB_NOINLINE __attribute__((noinline))
#endif

extern "C" CFB_NOINLINE bool cfb_cell(const Adapter* a, const Input* in, bool strict,
                                      Touch* t) {
    return parse_all(a, *in, strict, *t);
}

// Everything lazy in a library (a dispatch table, a first allocation) happens
// here, outside what is counted.
extern "C" __attribute__((noinline)) bool cfb_warm(const Adapter* a, const Input* in,
                                                   bool strict, Touch* t) {
    return parse_all(a, *in, strict, *t);
}

namespace {

void print_touch(const Touch& t) {
    std::printf("\"touch\":{\"containers\":%llu,\"strings\":%llu,\"string_bytes\":%llu,"
                "\"numbers\":%llu,\"bools\":%llu,\"nulls\":%llu,\"acc\":%llu}",
                (unsigned long long)t.containers, (unsigned long long)t.strings,
                (unsigned long long)t.string_bytes, (unsigned long long)t.numbers,
                (unsigned long long)t.bools, (unsigned long long)t.nulls,
                (unsigned long long)t.acc);
}

std::string base_name(const std::string& p) {
    size_t s = p.find_last_of("/\\");
    return s == std::string::npos ? p : p.substr(s + 1);
}

// "json-mixed-16.json" -> size 16
int size_of_name(const std::string& b) {
    size_t dot  = b.find_last_of('.');
    size_t dash = b.find_last_of('-', dot);
    if (dot == std::string::npos || dash == std::string::npos)
        return 0;
    return std::atoi(b.substr(dash + 1, dot - dash - 1).c_str());
}

std::string format_of(const std::string& b) {
    size_t      dot = b.find_last_of('.');
    std::string ext = dot == std::string::npos ? "" : b.substr(dot + 1);
    if (ext == "bin")
        return b.substr(0, b.find('-'));
    return ext;
}

const Adapter* find_adapter(const std::string& lib, const std::string& format) {
    for (const Adapter* a : cfb_registry())
        if ((lib == a->name || lib == a->lib) && format == a->format)
            return a;
    return nullptr;
}

int cmp_double(const void* x, const void* y) {
    double a = *static_cast<const double*>(x), b = *static_cast<const double*>(y);
    return a < b ? -1 : a > b ? 1 : 0;
}

// ---- the probes: eight documents a format a reader may or may not check ----

struct Probe {
    const char* format;
    const char* name;
    std::string doc; // empty: the format cannot say it
};

std::string rep(const std::string& s, int n) {
    std::string r;
    for (int i = 0; i < n; ++i)
        r += s;
    return r;
}

std::vector<Probe> probes() {
    std::vector<Probe> p;
    auto               add = [&](const char* f, const char* n, std::string d) {
        p.push_back({f, n, d});
    };
    // JSON
    add("json", "invalid-utf8", "{\"a\":\"\xff\xfe\"}");
    add("json", "duplicate-key", "{\"a\":1,\"a\":2}");
    add("json", "int-past-int64", "{\"a\":9223372036854775808}");
    add("json", "truncated", "{\"a\":[1,2");
    add("json", "trailing-text", "{\"a\":1} x");
    add("json", "nesting-100", "{\"a\":" + rep("[", 100) + rep("]", 100) + "}");
    add("json", "lone-surrogate", "{\"a\":\"\\ud800\"}");
    add("json", "nul-in-string", "{\"a\":\"x\\u0000y\"}");
    // YAML
    add("yaml", "invalid-utf8", "a: \"\xff\"\n");
    add("yaml", "duplicate-key", "a: 1\na: 2\n");
    add("yaml", "int-past-int64", "a: 9223372036854775808\n");
    add("yaml", "truncated", "a: [1, 2");
    add("yaml", "trailing-text", "a: 1\n- b\n");
    add("yaml", "nesting-100", "a: " + rep("[", 100) + rep("]", 100) + "\n");
    add("yaml", "lone-surrogate", "a: \"\\ud800\"\n");
    add("yaml", "nul-in-string", "a: \"x\\0y\"\n");
    // TOML
    add("toml", "invalid-utf8", "a = \"\xff\"\n");
    add("toml", "duplicate-key", "a = 1\na = 2\n");
    add("toml", "int-past-int64", "a = 9223372036854775808\n");
    add("toml", "truncated", "a = [1, 2");
    add("toml", "trailing-text", "a = 1\n]\n");
    add("toml", "nesting-100", "a = " + rep("[", 100) + rep("]", 100) + "\n");
    add("toml", "lone-surrogate", "a = \"\\ud800\"\n");
    add("toml", "nul-in-string", "a = \"x\\u0000y\"\n");
    // CBOR
    add("cbor", "invalid-utf8", std::string("\xa1\x61\x61\x61\xff", 5));
    add("cbor", "duplicate-key", std::string("\xa2\x61\x61\x01\x61\x61\x02", 7));
    add("cbor", "int-past-int64",
        std::string("\xa1\x61\x61\x1b\x80\x00\x00\x00\x00\x00\x00\x00", 12));
    add("cbor", "truncated", std::string("\xa1\x61\x61\x82\x01", 5));
    add("cbor", "trailing-text", std::string("\xa1\x61\x61\x01\x00", 5));
    add("cbor", "nesting-100",
        std::string("\xa1\x61\x61", 3) + rep(std::string("\x81", 1), 99) +
            std::string("\x01", 1));
    add("cbor", "lone-surrogate", std::string("\xa1\x61\x61\x63\xed\xa0\x80", 7));
    add("cbor", "nul-in-string", std::string("\xa1\x61\x61\x63x\x00y", 7));
    // INI
    add("ini", "invalid-utf8", "a = \xff\n");
    add("ini", "duplicate-key", "a = 1\na = 2\n");
    add("ini", "int-past-int64", "");
    add("ini", "truncated", "[sec\na = 1\n");
    add("ini", "trailing-text", "a = 1\nnoequals\n");
    std::string sec = "[a";
    for (int i = 1; i < 100; ++i)
        sec += ".a";
    add("ini", "nesting-100", sec + "]\nx = 1\n");
    add("ini", "lone-surrogate", "");
    add("ini", "nul-in-string", std::string("a = x\0y\n", 8));
    // XML
    add("xml", "invalid-utf8", "<r><a>\xff</a></r>");
    add("xml", "duplicate-key", "<r><a>1</a><b>1</b><a>2</a></r>");
    add("xml", "int-past-int64", "");
    add("xml", "truncated", "<r><a>1</a>");
    add("xml", "trailing-text", "<r><a>1</a></r>x");
    add("xml", "nesting-100",
        "<r>" + rep("<a>", 100) + "1" + rep("</a>", 100) + "</r>");
    add("xml", "lone-surrogate", "<r><a>&#xD800;</a></r>");
    add("xml", "nul-in-string", "<r><a>x&#0;y</a></r>");
    return p;
}

int do_probe(const std::string& lib) {
    std::vector<Probe> all = probes();
    for (const Adapter* a : cfb_registry()) {
        if (!lib.empty() && lib != a->name && lib != a->lib)
            continue;
        for (int strict = 1; strict >= 0; --strict) {
            std::printf("%s %s %s", a->name, a->format, strict ? "strict" : "loose");
            for (const Probe& pr : all) {
                if (std::strcmp(pr.format, a->format) != 0)
                    continue;
                if (pr.doc.empty()) {
                    std::printf(" %s=n/a", pr.name);
                    continue;
                }
                std::string buf = pr.doc;
                buf.append(64, '\0');
                Touch t;
                bool  ok = a->parse(buf.data(), pr.doc.size(), strict != 0, t);
                std::printf(" %s=%s", pr.name, ok ? "read" : "refuse");
            }
            std::printf("\n");
        }
    }
    return 0;
}

void usage() {
    std::fprintf(stderr,
                 "usage: cfb_compare --list | --lib <name> --doc <file> --mode "
                 "speed|count|memory|probe [--reps N] [--strict|--loose] [--cold]\n");
}

} // namespace

int main(int argc, char** argv) {
    std::string lib, doc, mode = "speed";
    int         reps   = 0;
    bool        strict = true, cold = false, list = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--lib" && i + 1 < argc)
            lib = argv[++i];
        else if (a == "--doc" && i + 1 < argc)
            doc = argv[++i];
        else if (a == "--mode" && i + 1 < argc)
            mode = argv[++i];
        else if (a == "--reps" && i + 1 < argc)
            reps = std::atoi(argv[++i]);
        else if (a == "--strict")
            strict = true;
        else if (a == "--loose")
            strict = false;
        else if (a == "--cold")
            cold = true;
        else if (a == "--list")
            list = true;
        else {
            usage();
            return 2;
        }
    }
    if (list) {
        for (const Adapter* a : cfb_registry())
            std::printf("{\"adapter\":\"%s\",\"lib\":\"%s\",\"format\":\"%s\","
                        "\"version\":\"%s\"}\n",
                        a->name, a->lib, a->format, a->version);
        return 0;
    }
    if (mode == "probe")
        return do_probe(lib);
    if (lib.empty() || doc.empty()) {
        usage();
        return 2;
    }
    std::string    b = base_name(doc);
    const Adapter* a = find_adapter(lib, format_of(b));
    if (!a) {
        std::fprintf(stderr, "cfb_compare: no adapter %s for %s\n", lib.c_str(),
                     format_of(b).c_str());
        return 3;
    }
    Input in;
    if (!load(in, doc)) {
        std::fprintf(stderr, "cfb_compare: cannot read %s\n", doc.c_str());
        return 3;
    }
    int size_mib = size_of_name(b);
    std::printf("{\"lib\":\"%s\",\"format\":\"%s\",\"doc\":\"%s\",\"size_mib\":%d,"
                "\"strict\":%s,",
                a->name, a->format, b.c_str(), size_mib, strict ? "true" : "false");

    if (mode == "count") {
        Touch warm, t;
        cfb_warm(a, &in, strict, &warm);
        bool ok = cfb_cell(a, &in, strict, &t);
        std::printf("\"mode\":\"count\",\"accepted\":%s,\"bytes\":%zu,",
                    ok ? "true" : "false", in.n);
        print_touch(t);
        std::printf("}\n");
        return 0;
    }
    if (mode == "memory") {
        rusage r0, r1;
        Touch  t;
        getrusage(RUSAGE_SELF, &r0);
        bool ok = cfb_cell(a, &in, strict, &t);
        getrusage(RUSAGE_SELF, &r1);
        std::printf("\"mode\":\"memory\",\"accepted\":%s,\"peak_growth_kib\":%ld,"
                    "\"maxrss_kib\":%ld,",
                    ok ? "true" : "false", r1.ru_maxrss - r0.ru_maxrss, r1.ru_maxrss);
        print_touch(t);
        std::printf("}\n");
        return 0;
    }
    if (mode != "speed") {
        usage();
        return 2;
    }

    // speed: one warm-up, then `reps` runs of at least 200 ms each, repeated
    // inside the run for a small document.
    if (reps <= 0)
        reps = size_mib >= 256 ? 3 : 9;
    Touch  t0;
    double w0    = now();
    bool   ok    = cfb_warm(a, &in, strict, &t0);
    double one   = now() - w0;
    long   inner = one >= 0.2 ? 1 : (long)(0.2 / (one > 1e-6 ? one : 1e-6)) + 1;
    if (inner > 100000)
        inner = 100000;
    std::vector<double> v;
    double              total_bytes = (double)(in.lens.empty() ? in.n : in.n);
    for (int r = 0; r < reps; ++r) {
        double read_secs = 0;
        if (cold) {
            // drop the page cache and read the file again: what a first read costs
            if (FILE* f = std::fopen("/proc/sys/vm/drop_caches", "w")) {
                std::fputs("3\n", f);
                std::fclose(f);
            }
            char*  d;
            size_t n;
            double r0 = now();
            if (read_whole(in.path, &d, &n))
                std::free(d);
            read_secs = now() - r0;
        }
        double t1 = now();
        for (long k = 0; k < inner; ++k) {
            Touch t;
            ok = parse_all(a, in, strict, t) && ok;
        }
        double secs = (now() - t1) / (double)inner + read_secs;
        v.push_back(total_bytes / 1048576.0 / secs);
    }
    std::qsort(v.data(), v.size(), sizeof(double), cmp_double);
    double best = v.back(), median = v[v.size() / 2];
    std::printf("\"mode\":\"%s\",\"accepted\":%s,\"mib_s\":%.2f,\"best_mib_s\":%.2f,"
                "\"reps\":%d,\"inner\":%ld,\"bytes\":%zu,",
                cold ? "cold" : "speed", ok ? "true" : "false", median, best, reps,
                inner, in.n);
    print_touch(t0);
    std::printf("}\n");
    return 0;
}
