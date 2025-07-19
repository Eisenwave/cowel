#ifndef COWEL_URL_ENCODE_HPP
#define COWEL_URL_ENCODE_HPP

#include <iterator>
#include <string_view>

#include "cowel/util/html.hpp"
#include "ulight/impl/ascii_chars.hpp"
#include "ulight/impl/chars.hpp"

#include "cowel/util/chars.hpp"

namespace cowel {

using ulight::Charset256;
using ulight::is_ascii_alphanumeric_set;
using ulight::detail::to_charset256;

inline constexpr auto is_url_reserved_set = to_charset256(u8"!#$&'()*+,/:;=?@[]");

/// @brief Returns `true` if `c` is a "reserved character" in a URL.
/// That is, a character which may have special meaning within a URL.
///
/// Note that this does not include control characters and many other characters which
/// have to be percent-encoded in all circumstances.
[[nodiscard]]
constexpr bool is_url_reserved(char8_t c) noexcept
{
    // https://en.wikipedia.org/wiki/Percent-encoding
    return is_url_reserved_set.contains(c);
}

[[nodiscard]]
constexpr bool is_url_reserved(char32_t c) noexcept
{
    return is_ascii(c) && is_url_reserved(char8_t(c));
}

inline constexpr auto is_url_unreserved_set = is_ascii_alphanumeric_set | to_charset256(u8"-_~");

/// @brief Returns `true` if `c` is an "unreserved character" in a URL.
/// That is, a character which does not need to be percent-encoded.
[[nodiscard]]
constexpr bool is_url_unreserved(char8_t c) noexcept
{
    return is_url_unreserved_set.contains(c);
}

[[nodiscard]]
constexpr bool is_url_unreserved(char32_t c) noexcept
{
    return is_ascii(c) && is_url_unreserved(char8_t(c));
}

inline constexpr auto is_url_always_encoded_set = ~(is_url_unreserved_set | is_url_reserved_set);

/// @brief Returns `true` if `c` is a character that is always percent-encoded in URLs.
/// That is, control characters, double quotes, whitespace, etc.
[[nodiscard]]
constexpr bool is_url_always_encoded(char8_t c) noexcept
{
    return is_url_always_encoded_set.contains(c);
}

[[nodiscard]]
constexpr bool is_url_always_encoded(char32_t c) noexcept
{
    return is_ascii(c) && is_url_unreserved(char8_t(c));
}

namespace detail {

[[nodiscard]]
constexpr char8_t to_ascii_digit(int value)
{
    return value < 10 ? char8_t(int(u8'0') + value) //
                      : char8_t(int(u8'a') + (value - 10));
}

} // namespace detail

/// @brief URL-encodes a given string `str`.
/// No Unicode-decode of `str` is performed;
/// instead, non-ASCII code units are all written directly, with no special encoding.
/// For any ASCII code units `c` where `filter(c)` yields `true`,
/// code units are percent-encoded.
/// @param out The output iterator, accepting `char8_t`.
/// @param str The string to encode.
/// @param filter Returns `true` for code units that should be URL-encoded.
template <std::output_iterator<char8_t> Output, typename F>
    requires std::is_invocable_r_v<bool, F, char8_t>
void url_encode_ascii_if(Output out, std::u8string_view str, F filter)
{
    for (char8_t c : str) {
        if (!is_ascii(c) || !filter(c)) {
            *out++ = c;
            continue;
        }
        *out++ = u8'%';
        *out++ = detail::to_ascii_digit((c >> 4) & 0xf);
        *out++ = detail::to_ascii_digit((c >> 0) & 0xf);
    }
}

template <string_or_char_consumer Output, typename F>
    requires std::is_invocable_r_v<bool, F, char8_t>
void url_encode_ascii_if(Output out, std::u8string_view str, F filter)
{
    for (char8_t c : str) {
        if (!is_ascii(c) || !filter(c)) {
            out(c);
            continue;
        }
        out(u8'%');
        out(detail::to_ascii_digit((c >> 4) & 0xf));
        out(detail::to_ascii_digit((c >> 0) & 0xf));
    }
}

} // namespace cowel

#endif
