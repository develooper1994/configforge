/*
 * configforge.hpp - the C++03 wrapper.
 *
 * A thin layer over the C API, not a second design. Every construct
 * here is valid C++03, so it compiles under GCC 3.4 and MSVC 2005
 * alike. Under C++11 the same classes gain move, an explicit bool and
 * pairs in braces; configforge17.hpp and configforge20.hpp add the
 * interfaces of those standards beside these classes (D-47).
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
#include <vector>

/* The standard the including program is compiled as. MSVC reports 199711L
   in __cplusplus unless asked otherwise and says the truth in
   _MSVC_LANG. Under C++11 the classes below gain what C++03 cannot say -
   move, an explicit bool, a list of pairs in braces - and nothing they
   already have changes; configforge17.hpp and configforge20.hpp add the
   interfaces of their standards. */
#if defined(_MSVC_LANG) && _MSVC_LANG > __cplusplus
#define CF_CPLUSPLUS _MSVC_LANG
#else
#define CF_CPLUSPLUS __cplusplus
#endif
#if CF_CPLUSPLUS >= 201103L
#define CF_CPP11 1
#include <initializer_list>
#else
#define CF_CPP11 0
#endif

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
 * Key
 *
 * A key declared once, with its type and default, and then used both
 * to define the contract and to read it - so the default is written in
 * one place and the reader cannot ask for the wrong type:
 *
 *     static const cf::Key<int> port("server.port", 8080, 1, 65535);
 *     cf::Config cfg = cf::Builder().define(port).create();
 *     int p = cfg[port];
 *
 * T is one of bool, int, long, cf_i64, double and std::string; any
 * other is refused at compile time, by Builder::define having no
 * overload for it. A key without a range has lo == hi, as in
 * cf_builder_int.
 * ------------------------------------------------------------------ */

template <typename T> class Key {
public:
    explicit Key(const char* name, T dflt = T(), unsigned flags = 0)
        : name_(name), dflt_(dflt), lo_(), hi_(), flags_(flags) {}
    Key(const char* name, T dflt, T lo, T hi, unsigned flags = 0)
        : name_(name), dflt_(dflt), lo_(lo), hi_(hi), flags_(flags) {}

    const char* name() const     { return name_; }
    const T&    fallback() const { return dflt_; }
    const T&    lo() const       { return lo_; }
    const T&    hi() const       { return hi_; }
    unsigned    flags() const    { return flags_; }

private:
    const char* name_;
    T           dflt_;
    T           lo_, hi_;
    unsigned    flags_;
};

namespace detail {
/* cf_i64 where it is a type of its own, and a stand-in nobody passes
   where it is long, so the overloads below are never one twice. */
template <typename A, typename B> struct is_same { enum { value = 0 }; };
template <typename A> struct is_same<A, A> { enum { value = 1 }; };
struct no_i64 {
    operator cf_i64() const { return 0; }
};
template <int same_as_long> struct pick_i64 { typedef cf_i64 type; };
template <> struct pick_i64<1> { typedef no_i64 type; };
typedef pick_i64<is_same<cf_i64, long>::value>::type i64;

/* The definition for each type a Key may have; the builder copies the
   key and a text default, so neither has to outlive it. */
inline void define_key(cf_builder* b, const Key<bool>& k)
{ cf_builder_bool(b, k.name(), k.fallback() ? CF_TRUE : CF_FALSE, k.flags()); }
inline void define_key(cf_builder* b, const Key<int>& k)
{ cf_builder_int(b, k.name(), k.fallback(), k.lo(), k.hi(), k.flags()); }
inline void define_key(cf_builder* b, const Key<long>& k)
{ cf_builder_int(b, k.name(), k.fallback(), k.lo(), k.hi(), k.flags()); }
inline void define_key(cf_builder* b, const Key<i64>& k)
{ cf_builder_int(b, k.name(), k.fallback(), k.lo(), k.hi(), k.flags()); }
inline void define_key(cf_builder* b, const Key<std::string>& k)
{ cf_builder_text(b, k.name(), k.fallback().c_str(), k.flags()); }
#if CF_WITH_REAL
inline void define_key(cf_builder* b, const Key<double>& k)
{ cf_builder_real(b, k.name(), k.fallback(), k.lo(), k.hi(), k.flags()); }
#endif

/* Keeps a template argument out of deduction, so a field's default
   may be "0.0.0.0" for a std::string member. */
template <typename T> struct same { typedef T type; };

/* The reads Config::get makes, one overload a type. A const char*
   would otherwise convert to bool and read as a pointer made of it, so
   it gets an overload whose result nothing can be cast from: read text
   as std::string. */
struct read_text_as_std_string {};
read_text_as_std_string read(const cf_config* c, const char* key, const char* f);
inline bool read(const cf_config* c, const char* key, bool f)
{ return cf_get_bool(c, key, f ? CF_TRUE : CF_FALSE) != CF_FALSE; }
inline int read(const cf_config* c, const char* key, int f)
{ return (int)cf_get_int(c, key, (cf_i64)f); }
inline long read(const cf_config* c, const char* key, long f)
{ return (long)cf_get_int(c, key, (cf_i64)f); }
inline cf_i64 read(const cf_config* c, const char* key, i64 f)
{ return cf_get_int(c, key, f); }
#if CF_WITH_REAL
inline double read(const cf_config* c, const char* key, double f)
{ return cf_get_real(c, key, f); }
#endif
inline std::string read(const cf_config* c, const char* key, const std::string& f)
{
    const char* p = cf_get_text(c, key, 0);
    return (p != 0) ? std::string(p) : f;
}
} /* namespace detail */

class Setting;
template <typename S> class Binding;

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
#if CF_CPP11
    explicit operator bool() const { return c_ != 0; }

    /* A move takes the handle and leaves an invalid one, with no count
       to touch. */
    Config(Config&& o) noexcept : c_(o.c_), rc_(o.rc_), err_(o.err_)
    { o.c_ = 0; o.rc_ = 0; }
    Config& operator=(Config&& o) noexcept
    {
        if (this != &o) {
            release();
            c_   = o.c_;
            rc_  = o.rc_;
            err_ = o.err_;
            o.c_  = 0;
            o.rc_ = 0;
        }
        return *this;
    }
#endif

    const Error& error() const { return err_; }

    /* Reads do not fail. Everything that can go wrong was detected
       while the configuration was being built and reported there. */
    template <typename T> T get(const char* key, T fallback) const;

    /* A declared key, read as its own type with its own default. */
    template <typename T> T get(const Key<T>& k) const
    { return get<T>(k.name(), k.fallback()); }
    template <typename T> T operator[](const Key<T>& k) const
    { return get<T>(k.name(), k.fallback()); }

    /* A key by name, typed where it is used and not before:
       `int port = cfg["server"]["port"];`. See Setting. */
    Setting operator[](const char* key) const;

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

    /* The contract as a JSON Schema, for editors (cf_schema). */
    std::string schema(const char* title) const
    {
        size_t n = cf_schema(c_, title, 0, 0);
        if (n == 0) {
            return std::string();
        }
        std::string s(n, '\0');
        cf_schema(c_, title, &s[0], n + 1);
        return s;
    }

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

    /* ---------------------------------------------------------- *
     * Changing and keeping
     * ---------------------------------------------------------- */

    /* A new snapshot with `pairs` applied as a source named `source`
       (cf_derive). This one is unchanged, so a reader holding it keeps
       what it had; an invalid change gives an invalid Config whose
       error() says why. */
    Config derive(const char* source, const cf_pair* pairs, size_t count) const
    {
        Error e;
        cf_config* c = cf_derive(c_, source, pairs, count, e.raw());
        return Config(c, e);
    }
#if CF_CPP11
    /* cfg.derive("admin", {{"log.level", "debug"}}) */
    Config derive(const char* source, std::initializer_list<cf_pair> pairs) const
    { return derive(source, pairs.begin(), pairs.size()); }
#endif

    /* Keep a change in a source the program granted (cf_write): the
       source must be able to be written, be granted, and the contract
       must accept every item, or nothing is written. */
    bool write(const char* target, const cf_write_item* items, size_t count,
               const char* meta = 0, Error* err = 0) const
    {
        Error  local;
        Error& e = (err != 0) ? *err : local;
        return cf_write(c_, target, items, count, meta, e.raw()) != CF_FALSE;
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

/* An overload a type, not a specialization: where cf_i64 is long (C++11
   and later on LP64 systems) a specialization for each would be one
   specialization twice. */
template <typename T> inline T Config::get(const char* key, T fallback) const
{ return (T)detail::read(c_, key, fallback); }

/* ------------------------------------------------------------------ *
 * Setting
 *
 * One key of a configuration, named a part at a time and typed where it
 * is read:
 *
 *     int         port = cfg["server"]["port"];
 *     std::string host = cfg["server.host"];
 *     if (cfg["tls"]["enabled"]) { ... }
 *
 * No fallback is passed, because the contract has one: a defined key
 * always has a value, and its default is the one written in the
 * definition. A key that is neither defined nor set reads as T() - 0,
 * false, "" - and exists() is how to tell. Compare with as<T>(), not
 * with ==: a conversion to any type cannot also choose one for a
 * comparison.
 *
 * It holds the Config, so it stays valid however long it is kept.
 * ------------------------------------------------------------------ */

class Setting {
public:
    Setting(const Config& c, const std::string& key) : c_(c), key_(key) {}

    Setting operator[](const char* child) const
    { return Setting(c_, key_.empty() ? std::string(child) : key_ + "." + child); }

    const std::string& key() const { return key_; }
    const Config&      config() const { return c_; }
    bool    exists() const { return c_.has(key_.c_str()); }
    cf_type type() const   { return c_.typeOf(key_.c_str()); }

    template <typename T> T as() const { return c_.get<T>(key_.c_str(), T()); }
    template <typename T> T as(const T& fallback) const
    { return c_.get<T>(key_.c_str(), fallback); }
    template <typename T> operator T() const { return as<T>(); }

    std::string text() const    { return c_.text(key_.c_str()); }
    std::string explain() const { return c_.explain(key_.c_str()); }

private:
    Config      c_;
    std::string key_;
};

inline Setting Config::operator[](const char* key) const
{ return Setting(*this, key != 0 ? key : ""); }

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
#if CF_CPP11
    Builder(Builder&& o) noexcept : b_(o.b_) { o.b_ = 0; }
    Builder& operator=(Builder&& o) noexcept
    {
        if (this != &o) {
            if (b_ != 0) {
                cf_builder_abort(b_);
            }
            b_   = o.b_;
            o.b_ = 0;
        }
        return *this;
    }
    Builder(const Builder&)            = delete;
    Builder& operator=(const Builder&) = delete;
#endif

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

    /* A directory of secrets, one file a key (cf_source_secrets). */
    Builder& secrets(const char* name, int rank, const char* path,
                     const char* prefix = 0, unsigned flags = 0)
    { if (b_) { cf_builder_secrets(b_, name, rank, path, prefix, flags); }
      return *this; }

    /* A key of the Windows Registry (cf_builder_registry). */
    Builder& registry(const char* name, int rank, const char* root, unsigned flags = 0)
    { if (b_) { cf_builder_registry(b_, name, rank, root, flags); }
      return *this; }

    /* A SQLite store (CF_WITH_SQLITE); cf_write writes one, granted. */
    Builder& store(const char* name, int rank, const char* path, unsigned flags = 0)
    { if (b_) { cf_builder_store(b_, name, rank, path, flags); }
      return *this; }

    /* Allow cf_write to write this source (cf_builder_writable). */
    Builder& writable(const char* source_name)
    { if (b_) { cf_builder_writable(b_, source_name); } return *this; }

    /* Pairs the caller owns, which must outlive the configuration. */
    Builder& memory(const char* name, int rank, const cf_pair* pairs, size_t count)
    { if (b_) { cf_builder_memory(b_, name, rank, pairs, count); }
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

    /* A full definition - an enum, a policy, a fallback - for what the
       define* shorthands do not say (cf_def_init, then its fields). */
    Builder& define(const cf_def& d)
    { if (b_) { cf_builder_define(b_, &d); } return *this; }

    /* A declared key (Key), defined with its type, default and range. */
    template <typename T> Builder& define(const Key<T>& k)
    { if (b_) { detail::define_key(b_, k); } return *this; }

    /* Every field of a Binding, or of anything else that defines a list
       of keys (configforge17.hpp's Fields). */
    template <typename D> Builder& define(const D& fields)
    { if (b_) { fields.defineInto(b_); } return *this; }

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
#if !CF_CPP11
    Builder(const Builder&);
    Builder& operator=(const Builder&);
#endif
    cf_builder* b_;
};

inline Config app(const char* name, int argc, char* const* argv)
{
    return Builder(name, argc, argv).create();
}

/* ------------------------------------------------------------------ *
 * Binding
 *
 * A plain struct of the program's own, filled from a configuration and
 * read as settings.server.port - the dot belongs to the program, which
 * C++ cannot add to cf::Config itself. Each field is written once, with
 * its key and default, and the same list defines the contract:
 *
 *     struct Server   { std::string host; int port; };
 *     struct Settings { Server server; bool tls; };
 *
 *     cf::Binding<Settings> bind;
 *     bind.field("server.host", &Settings::server, &Server::host, "0.0.0.0")
 *         .field("server.port", &Settings::server, &Server::port, 8080, 1, 65535)
 *         .field("tls.enabled", &Settings::tls, false);
 *     cf::Config cfg = cf::Builder(...).define(bind).create();
 *     Settings settings = bind.read(cfg);
 *
 * Not copyable, as a list of owned fields in C++03; keep one where the
 * struct is declared and share it by reference.
 * ------------------------------------------------------------------ */

template <typename S> class Binding {
public:
    Binding() {}
    ~Binding()
    {
        for (size_t i = 0; i < f_.size(); ++i) {
            delete f_[i];
        }
    }

    template <typename M>
    Binding& field(const char* key, M S::*m,
                   typename detail::same<M>::type dflt = M(), unsigned flags = 0)
    { return add(new Direct<M>(Key<M>(key, dflt, flags), m)); }

    template <typename M>
    Binding& field(const char* key, M S::*m, typename detail::same<M>::type dflt,
                   typename detail::same<M>::type lo,
                   typename detail::same<M>::type hi, unsigned flags = 0)
    { return add(new Direct<M>(Key<M>(key, dflt, lo, hi, flags), m)); }

    /* A field of a struct inside S: settings.server.port. */
    template <typename N, typename M>
    Binding& field(const char* key, N S::*outer, M N::*inner,
                   typename detail::same<M>::type dflt = M(), unsigned flags = 0)
    { return add(new Nested<N, M>(Key<M>(key, dflt, flags), outer, inner)); }

    template <typename N, typename M>
    Binding& field(const char* key, N S::*outer, M N::*inner,
                   typename detail::same<M>::type dflt,
                   typename detail::same<M>::type lo,
                   typename detail::same<M>::type hi, unsigned flags = 0)
    { return add(new Nested<N, M>(Key<M>(key, dflt, lo, hi, flags), outer, inner)); }

    /* Every field read from `c`; a field the configuration lacks keeps
       its declared default. */
    void read(const Config& c, S& out) const
    {
        for (size_t i = 0; i < f_.size(); ++i) {
            f_[i]->read(c, out);
        }
    }
    S read(const Config& c) const
    {
        S out = S();
        read(c, out);
        return out;
    }

    size_t size() const { return f_.size(); }

    /* Builder::define(binding) calls this. */
    void defineInto(cf_builder* b) const
    {
        for (size_t i = 0; i < f_.size(); ++i) {
            f_[i]->define(b);
        }
    }

private:
    Binding(const Binding&);
    Binding& operator=(const Binding&);

    struct Field {
        virtual ~Field() {}
        virtual void define(cf_builder* b) const = 0;
        virtual void read(const Config& c, S& out) const = 0;
    };
    template <typename M> struct Direct : Field {
        Direct(const Key<M>& k, M S::*m) : k_(k), m_(m) {}
        void define(cf_builder* b) const { detail::define_key(b, k_); }
        void read(const Config& c, S& out) const { out.*m_ = c.get(k_); }
        Key<M> k_;
        M S::*m_;
    };
    template <typename N, typename M> struct Nested : Field {
        Nested(const Key<M>& k, N S::*o, M N::*i) : k_(k), o_(o), i_(i) {}
        void define(cf_builder* b) const { detail::define_key(b, k_); }
        void read(const Config& c, S& out) const { (out.*o_).*i_ = c.get(k_); }
        Key<M> k_;
        N S::*o_;
        M N::*i_;
    };

    Binding& add(Field* f)
    {
        f_.push_back(f);
        return *this;
    }

    std::vector<Field*> f_;
};

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
#if CF_CPP11
    /* Movable from C++11: the descriptor has one owner, the one it was
       moved to. */
    Watcher(Watcher&& o) noexcept : err_(o.err_), w_(o.w_) { o.w_ = 0; }
    Watcher& operator=(Watcher&& o) noexcept
    {
        if (this != &o) {
            cf_watcher_close(w_);
            err_ = o.err_;
            w_   = o.w_;
            o.w_ = 0;
        }
        return *this;
    }
    Watcher(const Watcher&)            = delete;
    Watcher& operator=(const Watcher&) = delete;
#endif

    bool valid() const { return w_ != 0; }
    const Error& error() const { return err_; }

    int   fd() const     { return cf_watcher_fd(w_); }
    void* handle() const { return cf_watcher_handle(w_); }
    bool  drain()        { return cf_watcher_drain(w_) != CF_FALSE; }

    /* 1 woken and drained, 0 timed out, -1 failed. */
    int wait(long timeoutMs) { return cf_watcher_wait(w_, timeoutMs); }

private:
#if !CF_CPP11
    Watcher(const Watcher&);
    Watcher& operator=(const Watcher&);
#endif
    Error       err_; /* before w_: the constructor writes into it */
    cf_watcher* w_;
};

} /* namespace configforge */

namespace cf = configforge;

#endif /* CONFIGFORGE_HPP */
