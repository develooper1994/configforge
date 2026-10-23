/*
 * configforge.hpp - the C++03 wrapper.
 *
 * A thin layer over the C API, not a second design. Every construct
 * here is valid C++03, so it compiles under GCC 3.4 and MSVC 2005
 * alike; a separate optional header adds the conveniences that need
 * C++11 or later.
 *
 *     cf::Config cfg = cf::app("myapp", argc, argv);
 *     if (!cfg) { std::fputs(cfg.error().message, stderr); return 1; }
 *     int port = cfg.get<int>("server.port", 8080);
 *
 * Config is a reference-counted handle, so it is copyable and cheap
 * without move semantics and a factory can return it by value. There
 * is no `auto`, no smart pointer and no string_view anywhere below.
 */

#ifndef CONFIGFORGE_HPP
#define CONFIGFORGE_HPP

#include "configforge/configforge.h"

#include <string>

namespace configforge {

/* ------------------------------------------------------------------ *
 * Error
 * ------------------------------------------------------------------ */

class Error {
public:
    Error() { cf_error_clear(&e_); e_.size = sizeof(cf_error); }
    explicit Error(const cf_error& e) : e_(e) {}

    int         code()    const { return e_.code; }
    const char* message() const { return e_.message; }
    const char* key()     const { return e_.key; }
    const char* source()  const { return e_.source; }
    const char* name()    const { return cf_code_name((cf_code)e_.code); }
    bool        ok()      const { return e_.code == CF_OK; }

    cf_error*       raw()       { return &e_; }
    const cf_error* raw() const { return &e_; }

private:
    cf_error e_;
};

/* ------------------------------------------------------------------ *
 * Config
 *
 * Reference counted rather than move-only, because C++03 has no move
 * and a non-copyable object cannot be returned from a factory without
 * the copy elision that older compilers do not promise.
 * ------------------------------------------------------------------ */

class Config {
public:
    Config() : c_(0), rc_(0) {}
    Config(const Config& o) : c_(o.c_), rc_(o.rc_) { retain(); }
    ~Config() { release(); }

    Config& operator=(const Config& o)
    {
        if (this != &o) {
            release();
            c_  = o.c_;
            rc_ = o.rc_;
            retain();
        }
        return *this;
    }

    /* Explicit rather than an operator bool, which in C++03 would let
       a Config be compared with an int by accident. */
    bool valid() const { return c_ != 0; }
    bool operator!() const { return c_ == 0; }

    const Error& error() const { return err_; }

    /* Reads do not fail. Everything that can go wrong was detected
       while the configuration was being built and reported there. */
    template <typename T> T get(const char* key, T fallback) const;

    std::string text(const char* key, const char* fallback = "") const
    {
        const char* p = cf_get_text(c_, key, fallback);
        return std::string(p != 0 ? p : "");
    }

    bool has(const char* key) const
    { return cf_has(c_, key) != CF_FALSE; }

    cf_type typeOf(const char* key) const
    { return cf_type_of(c_, key); }

    /* ---------------------------------------------------------- *
     * Walking a prefix
     *
     * Written as begin/end over a real iterator type rather than a
     * callback, because that is valid C++03 *and* it makes range-for
     * work on C++11 and later without a second interface. A wrapper
     * that covers half its library is worse than none: it lets someone
     * believe they can stay in C++ and then drops them into C.
     * ---------------------------------------------------------- */

    class Entry {
    public:
        Entry() : it_(0) {}
        explicit Entry(const cf_iter* it) : it_(it) {}

        std::string key() const
        { return std::string(it_->key, it_->keylen); }
        std::string name() const
        { return std::string(it_->name, it_->namelen); }

        bool isTable() const { return it_->value == 0; }
        cf_type type() const
        { return it_->value ? it_->value->type : CF_TYPE_NULL; }

        std::string text() const
        {
            char tmp[CF_VALUE_TEXT_MAX];
            if (it_->value == 0) {
                return std::string();
            }
            if (it_->value->type == CF_TYPE_TEXT) {
                return std::string(it_->value->u.t.p, it_->value->u.t.n);
            }
            return std::string(tmp, cf_value_to_text(it_->value, tmp,
                                                     sizeof tmp));
        }

    private:
        const cf_iter* it_;
    };

    class Range {
    public:
        class iterator {
        public:
            iterator() : done_(true) {}
            explicit iterator(const Range* r) : done_(false), it_(r->start_)
            { advance(); }

            bool operator!=(const iterator& o) const
            { return done_ != o.done_; }
            iterator& operator++() { advance(); return *this; }
            Entry operator*() const { return Entry(&it_); }

        private:
            void advance()
            { if (!done_ && !cf_iter_next(&it_)) { done_ = true; } }
            bool    done_;
            cf_iter it_;
        };

        Range(const cf_config* c, const char* prefix, bool children)
        {
            if (children) {
                cf_iter_children(&start_, c, prefix);
            } else {
                cf_iter_keys(&start_, c, prefix);
            }
        }
        iterator begin() const { return iterator(this); }
        iterator end() const { return iterator(); }

    private:
        friend class iterator;
        cf_iter start_;
    };

    /* Every key under a prefix. */
    Range keys(const char* prefix = 0) const
    { return Range(c_, prefix, false); }

    /* Each distinct immediate child, once: how a program discovers what
       is configured rather than being told in advance. */
    Range children(const char* prefix) const
    { return Range(c_, prefix, true); }

    size_t arrayCount(const char* key) const
    { return cf_array_count(c_, key); }

    /* ---------------------------------------------------------- *
     * Writing and reporting
     * ---------------------------------------------------------- */

    /* An overrides file: what a source configured, and nothing nobody
       touched. Throws nothing; an empty string means the error is in
       `err`. */
    std::string emit(const char* format,
                     cf_emit_what what = CF_EMIT_CONFIGURED,
                     Error* err = 0) const
    {
        Error  local;
        Error& e = (err != 0) ? *err : local;
        size_t n = cf_emit(c_, format, what, 0, 0, 0, e.raw());
        if (n == 0) {
            return std::string();
        }
        std::string s(n, '\0');
        cf_emit(c_, format, what, 0, &s[0], n + 1, e.raw());
        return s;
    }

    std::string report(const cf_report_info& info) const
    {
        size_t n = cf_report(c_, &info, 0, 0);
        if (n == 0) {
            return std::string();
        }
        std::string s(n, '\0');
        cf_report(c_, &info, &s[0], n + 1);
        return s;
    }

    std::string explain(const char* key) const { return render(key); }
    std::string inspect() const                { return render(0); }

    /* What changed since `before`, a line per key with where each new
       value came from and secrets redacted (cf_diff_text): the line a
       reload should log. */
    std::string changesSince(const Config& before) const
    {
        size_t n = cf_diff_text(before.c_, c_, 0, 0);
        if (n == 0) {
            return std::string();
        }
        std::string s(n, '\0');
        cf_diff_text(before.c_, c_, &s[0], n + 1);
        return s;
    }

    cf_u64 digest() const { return cf_digest(c_); }
    size_t count()  const { return cf_count(c_); }

    /* Whether a file it was read from has moved on: a hint worth acting
       on, not a guarantee. See cf_changed. */
    bool changed() const { return cf_changed(c_) != CF_FALSE; }

    const cf_config* raw() const { return c_; }

private:
    friend class Builder;

    Config(cf_config* c, const Error& e) : c_(c), rc_(0), err_(e)
    {
        if (c_ != 0) {
            rc_ = new int(1);
        }
    }

    void retain()  { if (rc_ != 0) { ++*rc_; } }
    void release()
    {
        if (rc_ != 0 && --*rc_ == 0) {
            delete rc_;
            cf_config_free(c_);
        }
        rc_ = 0;
        c_  = 0;
    }

    std::string render(const char* key) const
    {
        size_t n = key ? cf_explain(c_, key, 0, 0) : cf_inspect(c_, 0, 0);
        if (n == 0) {
            return std::string();
        }
        std::string s(n, '\0');
        /* &s[0] is contiguous in practice on every C++03 library and
           guaranteed from C++11; the terminator is written into the
           extra byte std::string keeps, which is why the buffer is
           handed over as n + 1. */
        if (key) {
            cf_explain(c_, key, &s[0], n + 1);
        } else {
            cf_inspect(c_, &s[0], n + 1);
        }
        return s;
    }

    cf_config* c_;
    int*       rc_;
    Error      err_;
};

template <> inline bool Config::get<bool>(const char* key, bool f) const
{ return cf_get_bool(c_, key, f ? CF_TRUE : CF_FALSE) != CF_FALSE; }

template <> inline int Config::get<int>(const char* key, int f) const
{ return (int)cf_get_int(c_, key, (cf_i64)f); }

template <> inline long Config::get<long>(const char* key, long f) const
{ return (long)cf_get_int(c_, key, (cf_i64)f); }

template <> inline cf_i64 Config::get<cf_i64>(const char* key, cf_i64 f) const
{ return cf_get_int(c_, key, f); }

#if CF_WITH_REAL
template <> inline double Config::get<double>(const char* key, double f) const
{ return cf_get_real(c_, key, f); }
#endif

template <> inline std::string
Config::get<std::string>(const char* key, std::string f) const
{
    const char* p = cf_get_text(c_, key, 0);
    return (p != 0) ? std::string(p) : f;
}

/* ------------------------------------------------------------------ *
 * Builder
 *
 * Chained, because in C++ a definition block can read as a block. The
 * first failure is still held and reported once by create(), so the
 * chain has no error checks in the middle of it.
 * ------------------------------------------------------------------ */

class Builder {
public:
    Builder() : b_(cf_build(0)) {}
    explicit Builder(const char* appName, int argc, char* const* argv)
        : b_(cf_app_builder(appName, argc, argv)) {}
    ~Builder() { if (b_ != 0) { cf_builder_abort(b_); } }

    Builder& cli(const char* name, int rank, int argc, char* const* argv)
    { if (b_) { cf_builder_cli(b_, name, rank, argc, argv); } return *this; }

    Builder& env(const char* name, int rank, const char* prefix,
                 char* const* environment)
    { if (b_) { cf_builder_env(b_, name, rank, prefix, environment); }
      return *this; }

    Builder& file(const char* name, int rank, const char* path,
                  const char* format = 0, unsigned flags = 0)
    { if (b_) { cf_builder_file(b_, name, rank, path, format, flags); }
      return *this; }

    /* A file read by a codec the caller supplies (cf_builder_file_codec).
       A name of its own rather than a file() overload, which a literal 0
       for the format would make ambiguous. */
    Builder& fileWith(const char* name, int rank, const char* path,
                      const struct cf_codec* codec, unsigned flags = 0)
    { if (b_) { cf_builder_file_codec(b_, name, rank, path, codec, flags); }
      return *this; }

    Builder& dotenv(const char* name, int rank, const char* path,
                    const char* prefix, unsigned flags = 0)
    { if (b_) { cf_builder_dotenv(b_, name, rank, path, prefix, flags); }
      return *this; }

    /* A drop-in directory, conf.d style. */
    Builder& dir(const char* name, int rank, const char* path,
                 const char* format = 0, unsigned flags = 0)
    { if (b_) { cf_builder_dir(b_, name, rank, path, format, flags); }
      return *this; }

    Builder& replay(const char* name, int rank, const char* path)
    { if (b_) { cf_builder_replay(b_, name, rank, path); } return *this; }

    Builder& defineBool(const char* key, bool dflt, unsigned flags = 0)
    { if (b_) { cf_builder_bool(b_, key, dflt ? CF_TRUE : CF_FALSE, flags); }
      return *this; }

    Builder& defineInt(const char* key, cf_i64 dflt, cf_i64 lo = 0,
                       cf_i64 hi = 0, unsigned flags = 0)
    { if (b_) { cf_builder_int(b_, key, dflt, lo, hi, flags); } return *this; }

    Builder& defineText(const char* key, const char* dflt = 0,
                        unsigned flags = 0)
    { if (b_) { cf_builder_text(b_, key, dflt, flags); } return *this; }

#if CF_WITH_REAL
    Builder& defineReal(const char* key, double dflt, double lo = 0.0,
                        double hi = 0.0, unsigned flags = 0)
    { if (b_) { cf_builder_real(b_, key, dflt, lo, hi, flags); } return *this; }
#endif

    /* A key space the program cannot know in advance. */
    Builder& open(const char* prefix)
    { if (b_) { cf_builder_open(b_, prefix); } return *this; }

    Builder& unknown(cf_unknown_policy p)
    { if (b_) { cf_builder_unknown(b_, p); } return *this; }

    /* Consumes the builder either way, so there is one release path
       and no way to leak. */
    Config create()
    {
        Error    e;
        cf_config* c = cf_builder_create(b_, e.raw());
        b_ = 0;
        return Config(c, e);
    }

private:
    Builder(const Builder&);
    Builder& operator=(const Builder&);
    cf_builder* b_;
};

inline Config app(const char* name, int argc, char* const* argv)
{
    return Builder(name, argc, argv).create();
}

/* ------------------------------------------------------------------ *
 * Watcher
 *
 * The handle an event loop waits on (cf_watcher), owned and closed by
 * the destructor. Not copyable: it holds a kernel object, and two
 * owners of one descriptor is a double close. C++03 has no move, so it
 * lives where it is made and is passed by reference.
 *
 * A wake-up means "ask now": drain, then ask Config::changed().
 * ------------------------------------------------------------------ */

class Watcher {
public:
    explicit Watcher(const Config& c, const cf_allocator* a = 0)
        : err_(), w_(cf_watcher_open(c.raw(), a, err_.raw())) {}
    ~Watcher() { cf_watcher_close(w_); }

    bool valid() const { return w_ != 0; }
    const Error& error() const { return err_; }

    int   fd() const     { return cf_watcher_fd(w_); }
    void* handle() const { return cf_watcher_handle(w_); }
    bool  drain()        { return cf_watcher_drain(w_) != CF_FALSE; }

    /* 1 woken and drained, 0 timed out, -1 failed. */
    int wait(long timeoutMs) { return cf_watcher_wait(w_, timeoutMs); }

private:
    Watcher(const Watcher&);
    Watcher& operator=(const Watcher&);
    Error       err_; /* before w_: the constructor writes into it */
    cf_watcher* w_;
};

} /* namespace configforge */

namespace cf = configforge;

#endif /* CONFIGFORGE_HPP */
