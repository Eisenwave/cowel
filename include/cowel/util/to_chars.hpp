#ifndef COWEL_TO_CHARS_HPP
#define COWEL_TO_CHARS_HPP

#include <bit>
#include <charconv>
#include <cstddef>
#include <limits>
#include <system_error>
#include <type_traits>

#include "cowel/util/assert.hpp"
#include "cowel/util/chars.hpp"
#include "cowel/util/meta.hpp"
#include "cowel/util/static_string.hpp"

namespace cowel {

template <typename T>
concept digit_sequence = requires {
    std::numeric_limits<T>::digits;
    std::is_signed_v<T>;
};

template <typename T>
concept to_chars_able = requires(char* p, T x) { std::to_chars(p, p, x); };

template <typename T>
concept character_convertible = digit_sequence<T> && to_chars_able<T>;

template <typename Char, std::size_t capacity>
using Basic_Characters = Basic_Static_String<Char, capacity>;
template <std::size_t capacity>
using Characters = Static_String<capacity>;
template <std::size_t capacity>
using Characters8 = Static_String8<capacity>;

template <char_like Char = char, character_convertible T>
[[nodiscard]]
constexpr Basic_Characters<Char, std::numeric_limits<T>::digits + 1>
to_characters(const T& x, int base = 10, bool to_upper = false)
{
    COWEL_ASSERT(base >= 2 && base <= 36);

    if constexpr (std::is_same_v<Char, char>) {
        using array_type = typename decltype(to_characters<char>(x, base))::array_type;

        array_type chars {};
        auto* const buffer_start = chars.data();
        const auto result = std::to_chars(buffer_start, buffer_start + chars.size(), x, base);
        COWEL_ASSERT(result.ec == std::errc {});
        const auto result_length = std::size_t(result.ptr - buffer_start);
        if (to_upper) {
            for (char& c : chars) {
                c = char(to_ascii_upper(char8_t(c)));
            }
        }

        return { chars, result_length };
    }
    else {
        using result_type = Basic_Characters<Char, std::numeric_limits<T>::digits + 1>;
        const auto char_result = to_characters<char>(x, base, to_upper);
        return std::bit_cast<result_type>(char_result);
    }
}

template <character_convertible T>
[[nodiscard]]
constexpr Characters8<std::numeric_limits<T>::digits + 1>
to_characters8(const T& x, int base = 10, bool to_upper = false)
{
    return to_characters<char8_t>(x, base, to_upper);
}

} // namespace cowel

#endif
