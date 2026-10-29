/*
 * configforge.hpp - the C++ interface, one header for every standard.
 *
 * A thin layer over the C API, not a second design. The core is valid
 * C++03, so it compiles under GCC 3.4 and MSVC 2005 alike; what a later
 * standard can say is added by the standard the program is compiled as
 * (D-47) - under C++11 move, an explicit bool and pairs in braces, under
 * C++17 std::optional, std::variant and structured bindings, under C++20
 * concepts, views and std::format, under C++23 std::expected. Nothing a
 * program writes says which; CF_CPP11, CF_CPP17, CF_CPP20 and
 * CF_CPP_EXPECTED say what is on.
 *
 *     cf::Config cfg = cf::app("myapp", argc, argv);
 *     if (!cfg) { std::fputs(cfg.error().message(), stderr); return 1; }
 *     int port = cfg["server"]["port"];
 *
 * Config is a reference-counted handle, so it is copyable and cheap
 * without move semantics and a factory can return it by value. The
 * C++03 core has no `auto`, no smart pointer and no string_view.
 */

#ifndef CONFIGFORGE_HPP
#define CONFIGFORGE_HPP

#include "configforge/configforge.h"

#include <string>
#include <vector>

/* The standard the including program is compiled as. MSVC reports 199711L
   in __cplusplus unless asked otherwise and says the truth in
   _MSVC_LANG. One header for every standard (D-47): under C++11 the
   classes below gain what C++03 cannot say - move, an explicit bool, a
   list of pairs in braces - and nothing they already have changes; the
   C++17 and C++20 parts at the end add functions over them. */
#if defined(_MSVC_LANG) && _MSVC_LANG > __cplusplus
#define CF_CPLUSPLUS _MSVC_LANG
#else
#define CF_CPLUSPLUS __cplusplus
#endif
#if CF_CPLUSPLUS >= 201103L
#define CF_CPP11 1
#include <functional>
#include <initializer_list>
#else
#define CF_CPP11 0
#endif

/* C++17 and C++20 by what the library has, not only by what the
   compiler says: GCC 12 says C++20 and has no <format>, and a header
   that refused to compile there would be worse than one that offers
   C++17's part. */
#if CF_CPP11 && defined(__has_include)
#if __has_include(<version>)
#include <version>
#endif
#endif
#if CF_CPLUSPLUS >= 201703L && defined(__has_include)
#if __has_include(<optional>) && __has_include(<string_view>) && __has_include(<variant>)
#define CF_CPP17 1
#endif
#endif
#ifndef CF_CPP17
#define CF_CPP17 0
#endif
#if CF_CPP17 && CF_CPLUSPLUS >= 202002L && defined(__cpp_concepts) && \
    defined(__cpp_lib_ranges) && defined(__cpp_lib_format) && defined(__cpp_lib_span)
#define CF_CPP20 1
#else
#define CF_CPP20 0
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
    friend class Live;

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
       of keys (the C++17 part's Fields). */
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

    /* The C builder, handed over uncreated: what a cf_live build
       function returns (Live). This one is empty afterwards. */
    cf_builder* release()
    {
        cf_builder* b = b_;
        b_            = 0;
        return b;
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

/* ------------------------------------------------------------------ *
 * Live
 *
 * A configuration that reloads (cf_live.h): get() on any thread, the
 * rest on the one that drives the reloads.
 *
 *     static cf_builder* build(void*)
 *     {
 *         return cf::Builder("myapp", 0, 0)
 *             .defineInt("server.port", 8080, 1, 65535, CF_RESTART)
 *             .release();
 *     }
 *
 *     cf::Live live(CF_LIVE_AUTO, build);
 *     cf::Config cfg = live.get();     // one request, one snapshot
 *     live.poll();                     // in the loop
 *
 * Under C++11 the build may be any callable returning a cf::Builder,
 * and the check any callable with cf_live_check_fn's arguments.
 * ------------------------------------------------------------------ */

class Live {
public:
    Live(cf_live_mode mode, cf_live_build_fn build, void* user = 0,
         cf_live_check_fn check = 0, const cf_allocator* a = 0)
        : err_(), l_(cf_live_open(mode, build, check, user, a, err_.raw()))
#if CF_CPP11
        , fn_(nullptr)
#endif
    {}
#if CF_CPP11
    using BuildFn = std::function<Builder()>;
    using CheckFn = std::function<bool(const cf_config*, const cf_config*, cf_error*)>;

    Live(cf_live_mode mode, BuildFn build, CheckFn check = nullptr)
        : err_(), l_(0), fn_(new Fns{std::move(build), std::move(check)})
    {
        l_ = cf_live_open(mode, &Live::trampoline, fn_->check ? &Live::checker : nullptr,
                          fn_, nullptr, err_.raw());
    }
    Live(Live&& o) noexcept : err_(o.err_), l_(o.l_), fn_(o.fn_)
    {
        o.l_  = 0;
        o.fn_ = nullptr;
    }
    Live& operator=(Live&&) = delete;
    Live(const Live&)            = delete;
    Live& operator=(const Live&) = delete;
#endif
    ~Live()
    {
        cf_live_close(l_);
#if CF_CPP11
        delete fn_;
#endif
    }

    bool         valid() const { return l_ != 0; }
    const Error& error() const { return err_; }

    /* The current snapshot; any thread. */
    Config get() const { return Config(cf_live_get(l_), Error()); }

    cf_live_result poll(Error* e = 0)   { return cf_live_poll(l_, out(e)); }
    cf_live_result reload(Error* e = 0) { return cf_live_reload(l_, out(e)); }
    cf_live_result wait(long timeoutMs, Error* e = 0)
    { return cf_live_wait(l_, timeoutMs, out(e)); }
    cf_live_result approve(Error* e = 0) { return cf_live_approve(l_, out(e)); }
    void           reject() { cf_live_reject(l_); }

    /* The snapshot waiting for approval; invalid when none is. */
    Config pending() const
    {
        cf_config* p = const_cast<cf_config*>(cf_live_pending(l_));
        cf_config_ref(p);
        return Config(p, Error());
    }

    cf_u64 generation() const { return cf_live_generation(l_); }

    /* The CF_RESTART keys waiting for a restart, a line each. */
    std::string restarts() const
    {
        size_t n = cf_live_restarts(l_, 0, 0);
        if (n == 0) {
            return std::string();
        }
        std::string s(n, '\0');
        cf_live_restarts(l_, &s[0], n + 1);
        return s;
    }

private:
#if !CF_CPP11
    Live(const Live&);
    Live& operator=(const Live&);
#endif
    cf_error* out(Error* e) { return (e != 0) ? e->raw() : err_.raw(); }

    Error    err_;
    cf_live* l_;
#if CF_CPP11
    struct Fns {
        BuildFn build;
        CheckFn check;
    };
    static cf_builder* trampoline(void* user)
    { return static_cast<Fns*>(user)->build().release(); }
    static cf_bool checker(void* user, const cf_config* running, const cf_config* next,
                           cf_error* err)
    { return static_cast<Fns*>(user)->check(running, next, err) ? CF_TRUE : CF_FALSE; }
    Fns* fn_;
#endif
};

} /* namespace configforge */

/* ================================================================== *
 * C++17: find<T> as std::optional, value() as a std::variant, items()
 * for structured bindings, and fields() over member pointers.
 *
 *     auto port = cf::find<int>(cfg, "server.port");    // std::optional
 *     for (auto [key, value] : cf::items(cfg, "server")) { ... }
 *
 *     inline const auto settings_fields = cf::fields(
 *         cf::field<&Settings::server, &Server::port>("server.port", 8080, 1, 65535),
 *         cf::field<&Settings::tls>("tls.enabled", false));
 *     auto settings = settings_fields.read<Settings>(cfg);
 *
 * Keys are std::string_view, copied to a terminated buffer for the call
 * (one under CF_KEY_MAX bytes stays on the stack). Text comes back as a
 * std::string_view into the configuration, valid while it is.
 * ================================================================== */

#if CF_CPP17
#include <cstddef>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace configforge {

namespace detail {

/* A string_view is not terminated, and the C interface reads up to the
   terminator. */
class key_text {
public:
    explicit key_text(std::string_view k)
    {
        if (k.size() < sizeof buf_) {
            std::memcpy(buf_, k.data(), k.size());
            buf_[k.size()] = '\0';
            p_             = buf_;
        } else {
            big_ = std::string(k);
            p_   = big_.c_str();
        }
    }
    key_text(const key_text&)            = delete;
    key_text& operator=(const key_text&) = delete;
    const char* c_str() const { return p_; }

private:
    char        buf_[CF_KEY_MAX + 1];
    std::string big_;
    const char* p_;
};

template <typename> inline constexpr bool always_false = false;

template <typename> struct member_of;
template <typename C, typename M> struct member_of<M C::*> {
    using owner = C;
    using type  = M;
};

/* settings.*&Settings::server.*&Server::port, for any length of path. */
template <typename S, typename M> auto& walk(S& s, M m) { return s.*m; }
template <typename S, typename M, typename... R> auto& walk(S& s, M m, R... r)
{
    return walk(s.*m, r...);
}

} /* namespace detail */

/* ------------------------------------------------------------------ *
 * find: a value that may not be there
 *
 * std::nullopt when the key is absent, holds another type, or holds an
 * integer T cannot represent - never a wrapped or truncated value. T is
 * bool, any integer type, float or double, std::string or
 * std::string_view.
 * ------------------------------------------------------------------ */

template <typename T> std::optional<T> find(const Config& c, std::string_view key)
{
    detail::key_text k(key);
    if constexpr (std::is_same_v<T, bool>) {
        cf_bool v = CF_FALSE;
        if (cf_try_bool(c.raw(), k.c_str(), &v) == CF_OK) {
            return v != CF_FALSE;
        }
    } else if constexpr (std::is_integral_v<T>) {
        cf_i64 v = 0;
        if (cf_try_int(c.raw(), k.c_str(), &v) == CF_OK) {
            if constexpr (std::is_signed_v<T>) {
                if (v >= (cf_i64)std::numeric_limits<T>::min() &&
                    v <= (cf_i64)std::numeric_limits<T>::max()) {
                    return (T)v;
                }
            } else {
                if (v >= 0 && (cf_u64)v <= (cf_u64)std::numeric_limits<T>::max()) {
                    return (T)v;
                }
            }
        }
    } else if constexpr (std::is_floating_point_v<T>) {
#if CF_WITH_REAL
        double v = 0.0;
        if (cf_try_real(c.raw(), k.c_str(), &v) == CF_OK) {
            return (T)v;
        }
#endif
    } else if constexpr (std::is_same_v<T, std::string> ||
                         std::is_same_v<T, std::string_view>) {
        const char* p = 0;
        size_t      n = 0;
        if (cf_try_text(c.raw(), k.c_str(), &p, &n) == CF_OK) {
            return T(p, n);
        }
    } else {
        static_assert(detail::always_false<T>, "cf::find reads bool, integers, "
                                               "reals, std::string and std::string_view");
    }
    return std::nullopt;
}

/* ------------------------------------------------------------------ *
 * value: whatever type the key has
 * ------------------------------------------------------------------ */

using Value = std::variant<std::monostate, bool, cf_i64, double, std::string_view>;

inline Value to_value(const cf_value* v)
{
    if (v == 0) {
        return std::monostate();
    }
    switch (v->type) {
    case CF_TYPE_BOOL:
        return v->u.b != CF_FALSE;
    case CF_TYPE_INT:
        return v->u.i;
#if CF_WITH_REAL
    case CF_TYPE_REAL:
        return v->u.r;
#endif
    case CF_TYPE_TEXT:
        return std::string_view(v->u.t.p, v->u.t.n);
    default:
        return std::monostate();
    }
}

inline Value value(const Config& c, std::string_view key)
{
    detail::key_text k(key);
    switch (cf_type_of(c.raw(), k.c_str())) {
    case CF_TYPE_BOOL:
        return *find<bool>(c, key);
    case CF_TYPE_INT:
        return *find<cf_i64>(c, key);
#if CF_WITH_REAL
    case CF_TYPE_REAL:
        return *find<double>(c, key);
#endif
    case CF_TYPE_TEXT:
        return *find<std::string_view>(c, key);
    default:
        return std::monostate();
    }
}

/* ------------------------------------------------------------------ *
 * items: every key under a prefix, for a structured binding
 *
 *     for (auto [key, value] : cf::items(cfg, "server")) { ... }
 *
 * items() yields each whole key; children() yields each immediate
 * child's name, the part after the prefix, since a child that is a
 * table has no key of its own. Both, and any text, are views into the
 * configuration.
 * ------------------------------------------------------------------ */

struct Item {
    std::string_view key;
    Value            value;
};

class Items {
public:
    class iterator {
    public:
        /* An input iterator, so the C++20 part can make Items a view. */
        using value_type      = Item;
        using difference_type = std::ptrdiff_t;

        iterator() = default;
        iterator(const cf_iter& start, bool names) : it_(start), done_(false), names_(names)
        {
            advance();
        }
        bool      operator!=(const iterator& o) const { return done_ != o.done_; }
        bool      operator==(const iterator& o) const { return done_ == o.done_; }
        iterator& operator++()
        {
            advance();
            return *this;
        }
        void operator++(int) { advance(); }
        Item operator*() const
        {
            return Item{names_ ? std::string_view(it_.name, it_.namelen)
                               : std::string_view(it_.key, it_.keylen),
                        to_value(it_.value)};
        }

    private:
        void advance()
        {
            if (!done_ && !cf_iter_next(&it_)) {
                done_ = true;
            }
        }
        cf_iter it_{};
        bool    done_  = true;
        bool    names_ = false;
    };

    Items() = default;
    Items(const Config& c, const char* prefix, bool children) : c_(c), names_(children)
    {
        if (children) {
            cf_iter_children(&start_, c_.raw(), prefix);
        } else {
            cf_iter_keys(&start_, c_.raw(), prefix);
        }
    }
    iterator begin() const { return iterator(start_, names_); }
    iterator end() const { return iterator(); }

private:
    Config  c_; /* keeps what the views point into */
    cf_iter start_{};
    bool    names_ = false;
};

inline Items items(const Config& c, const char* prefix = 0) { return Items(c, prefix, false); }

/* Each immediate child once; a table's value is std::monostate. */
inline Items children(const Config& c, const char* prefix) { return Items(c, prefix, true); }

/* ------------------------------------------------------------------ *
 * Fields: the program's struct, by member pointers known at compile time
 *
 * The C++17 form of Binding: the field list is a value, not a heap of
 * objects, it can be declared once beside the struct as an inline
 * constant, and a nested member is a path of member pointers. As with
 * Binding, the list is also the contract (Builder::define(fields)).
 * ------------------------------------------------------------------ */

template <auto First, auto... Rest> class Field {
public:
    using owner = typename detail::member_of<decltype(First)>::owner;
    using type  = typename detail::member_of<
        std::tuple_element_t<sizeof...(Rest), std::tuple<decltype(First), decltype(Rest)...>>>::
        type;

    explicit Field(const Key<type>& k) : key_(k) {}

    const Key<type>& key() const { return key_; }
    void             defineInto(cf_builder* b) const { detail::define_key(b, key_); }
    void readInto(const Config& c, owner& out) const { detail::walk(out, First, Rest...) = c.get(key_); }

private:
    Key<type> key_;
};

template <auto First, auto... Rest>
Field<First, Rest...>
field(const char* key, typename Field<First, Rest...>::type dflt = {}, unsigned flags = 0)
{
    return Field<First, Rest...>(Key<typename Field<First, Rest...>::type>(key, dflt, flags));
}

template <auto First, auto... Rest>
Field<First, Rest...> field(const char* key, typename Field<First, Rest...>::type dflt,
                            typename Field<First, Rest...>::type lo,
                            typename Field<First, Rest...>::type hi, unsigned flags = 0)
{
    return Field<First, Rest...>(
        Key<typename Field<First, Rest...>::type>(key, dflt, lo, hi, flags));
}

template <typename... F> class Fields {
public:
    explicit Fields(F... f) : f_(std::move(f)...) {}

    void defineInto(cf_builder* b) const
    {
        std::apply([b](const F&... f) { (f.defineInto(b), ...); }, f_);
    }

    template <typename S> void read(const Config& c, S& out) const
    {
        std::apply([&](const F&... f) { (f.readInto(c, out), ...); }, f_);
    }
    template <typename S> S read(const Config& c) const
    {
        S out{};
        read(c, out);
        return out;
    }

    static constexpr std::size_t size() { return sizeof...(F); }

private:
    std::tuple<F...> f_;
};

template <typename... F> Fields<F...> fields(F... f) { return Fields<F...>(std::move(f)...); }

} /* namespace configforge */
#endif /* CF_CPP17 */

/* ================================================================== *
 * C++20: a concept for the types a key can be read as, the keys under a
 * prefix as a view for the standard adaptors, std::format of a Setting,
 * a std::span of pairs; under C++23, where the library has
 * std::expected, lookup<T> says why a value is not there.
 *
 *     for (auto [key, value] : cf::items(cfg, "server")
 *                                  | std::views::filter(is_int)) { ... }
 *     std::string line = std::format("port {}", cfg["server.port"]);
 *     std::expected<int, cf::Error> port = cf::lookup<int>(cfg, "server.port");
 * ================================================================== */

#if CF_CPP20
#include <concepts>
#include <format>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
#include <expected>
#define CF_CPP_EXPECTED 1
#else
#define CF_CPP_EXPECTED 0
#endif

namespace configforge {

/* The types a key can be read as. */
template <typename T>
concept readable = std::same_as<T, bool> || std::integral<T> || std::floating_point<T> ||
                   std::same_as<T, std::string> || std::same_as<T, std::string_view>;

/* find<T>, constrained: cf::get_or<unsigned char>(...) compiles, a
   cf::get_or<std::vector<int>>(...) is refused by the concept's name. */
template <readable T> T get_or(const Config& c, std::string_view key, T fallback)
{
    std::optional<T> v = find<T>(c, key);
    return v ? *v : fallback;
}

/* A derived snapshot from any contiguous run of pairs. */
inline Config derive(const Config& c, const char* source, std::span<const cf_pair> pairs)
{
    return c.derive(source, pairs.data(), pairs.size());
}

/* The value as text - an int as digits, text as itself, an absent key
   as nothing - which is what std::format prints for a Setting. */
inline std::string to_text(const Value& v)
{
    return std::visit(
        [](const auto& x) -> std::string {
            using X = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<X, std::monostate>) {
                return std::string();
            } else if constexpr (std::is_same_v<X, bool>) {
                return x ? "true" : "false";
            } else if constexpr (std::is_same_v<X, std::string_view>) {
                return std::string(x);
            } else {
                return std::format("{}", x);
            }
        },
        v);
}

#if CF_CPP_EXPECTED
/* A value, or why there is none: CF_E_KEY when the key is absent,
   CF_E_CONVERT when it holds another type or an integer T cannot
   represent, with the key in the error. */
template <readable T> std::expected<T, Error> lookup(const Config& c, std::string_view key)
{
    if (std::optional<T> v = find<T>(c, key)) {
        return *v;
    }
    detail::key_text k(key);
    Error            e;
    if (cf_has(c.raw(), k.c_str())) {
        cf_error_set(e.raw(), CF_E_CONVERT, k.c_str(), 0, "'%s' is a %s", k.c_str(),
                     cf_type_name(cf_type_of(c.raw(), k.c_str())));
    } else {
        cf_error_set(e.raw(), CF_E_KEY, k.c_str(), 0, "'%s' is not set", k.c_str());
    }
    return std::unexpected(e);
}
#endif

} /* namespace configforge */

/* Items is a view: it holds the configuration, is cheap to copy, and
   walks it in order. */
template <> inline constexpr bool std::ranges::enable_view<configforge::Items> = true;

/* std::format("{}", cfg["server.port"]) - the value, by the type the
   contract gave it. */
template <> struct std::formatter<configforge::Setting> : std::formatter<std::string_view> {
    auto format(const configforge::Setting& s, std::format_context& ctx) const
    {
        std::string t = configforge::to_text(configforge::value(s.config(), s.key()));
        return std::formatter<std::string_view>::format(t, ctx);
    }
};
#else
#define CF_CPP_EXPECTED 0
#endif /* CF_CPP20 */

namespace cf = configforge;

#endif /* CONFIGFORGE_HPP */
