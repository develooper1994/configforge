/*
 * configforge17.hpp - the C++17 interface, over the C++03 one.
 *
 * configforge.hpp stays the interface every C++ compiler has. This adds
 * what C++17 can say and C++03 cannot, as free functions and types
 * beside the classes rather than a second set of them, so a program can
 * mix the two and a library that takes a cf::Config takes either:
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
 */

#ifndef CONFIGFORGE17_HPP
#define CONFIGFORGE17_HPP

#include "configforge/configforge.hpp"

#if CF_CPLUSPLUS < 201703L
#error "configforge17.hpp needs C++17; configforge.hpp is the C++03 interface"
#endif

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
        /* An input iterator, so configforge20.hpp can make Items a view. */
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

#endif /* CONFIGFORGE17_HPP */
