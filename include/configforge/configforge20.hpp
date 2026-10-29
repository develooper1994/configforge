/*
 * configforge20.hpp - the C++20 interface, over the C++17 one.
 *
 * What C++20 adds to reading a configuration: a concept for the types a
 * key can be read as, so a wrong one is refused with its name rather
 * than inside a template; the keys under a prefix as a view, so the
 * standard range adaptors apply; a value formatted by std::format; and
 * a std::span of pairs for a derived snapshot. Under C++23, where the
 * library has std::expected, lookup<T> says why a value is not there.
 *
 *     for (auto [key, value] : cf::items(cfg, "server")
 *                                  | std::views::filter(is_int)) { ... }
 *     std::string line = std::format("port {}", cfg["server.port"]);
 *     std::expected<int, cf::Error> port = cf::lookup<int>(cfg, "server.port");
 */

#ifndef CONFIGFORGE20_HPP
#define CONFIGFORGE20_HPP

#include "configforge/configforge17.hpp"

#if CF_CPLUSPLUS < 202002L
#error "configforge20.hpp needs C++20; configforge17.hpp is the C++17 interface"
#endif

#include <concepts>
#include <format>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <version>
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

#endif /* CONFIGFORGE20_HPP */
