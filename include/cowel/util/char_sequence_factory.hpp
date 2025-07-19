#ifndef COWEL_CHAR_SEQUENCES_HPP
#define COWEL_CHAR_SEQUENCES_HPP

#include <algorithm>
#include <initializer_list>
#include <span>
#include <string_view>

#include "cowel/util/assert.hpp"
#include "cowel/util/char_sequence.hpp"
#include "cowel/util/static_string.hpp"
#include "cowel/util/unicode.hpp"

namespace cowel {

template <typename T, typename Char>
concept basic_string_like = requires(const T& const_str, T& str, std::size_t n) {
    { const_str.size() } -> std::convertible_to<std::size_t>;
    { const_str.data() } -> std::convertible_to<const Char*>;
    { const_str.empty() } -> std::convertible_to<bool>;
    str.remove_prefix(n);
};

template <typename T>
concept u8string_like = basic_string_like<T, char8_t>;

static_assert(u8string_like<std::u8string_view>);
static_assert(u8string_like<Static_String8<4>>);

/// @brief A char source which obtains characters repeatedly from the same string.
template <u8string_like String>
struct Repeated_String_Like_Char_Source {
private:
    std::size_t m_offset = 0;
    String m_string;

public:
    [[nodiscard]]
    constexpr Repeated_String_Like_Char_Source(const String& str)
        : m_string { str }
    {
    }

    [[nodiscard]]
    constexpr Repeated_String_Like_Char_Source(String&& str)
        : m_string { std::move(str) }
    {
    }

    // NOLINTNEXTLINE(modernize-use-nodiscard)
    constexpr void operator()(std::span<char8_t> buffer, std::size_t n)
    {
        if (n == 0) {
            return;
        }
        COWEL_ASSERT(!m_string.empty());

        std::size_t i = 0;

        const std::size_t first_n = std::min(m_string.size() - m_offset, buffer.size());
        std::ranges::copy_n(m_string.data(), std::ptrdiff_t(first_n), buffer.data() + i);
        i += first_n;

        const std::size_t full_copies = (buffer.size() - i) / m_string.size();
        for (std::size_t j = 0; j < full_copies; ++j) {
            std::ranges::copy_n(
                m_string.data(), std::ptrdiff_t(m_string.size()), buffer.data() + i
            );
        }
        i += m_string.size() * full_copies;

        const std::size_t tail_n = buffer.size() - i;
        std::ranges::copy_n(m_string.data(), std::ptrdiff_t(tail_n), buffer.data() + i);
        i += tail_n;

        COWEL_ASSERT(i == n);
    }
};

using Repeated_String_View_Char_Source = Repeated_String_Like_Char_Source<std::u8string_view>;
template <std::size_t n>
using Repeated_Static_String_Char_Source = Repeated_String_Like_Char_Source<Static_String8<n>>;

static_assert(char_source8<Repeated_String_View_Char_Source>);
static_assert(char_source8<Repeated_Static_String_Char_Source<4>>);

/// @brief A character source which obtains characters from a span of string views.
struct Joined_Char_Source {
private:
    std::span<const std::u8string_view> m_parts;
    std::size_t m_offset_in_front = 0;

public:
    Joined_Char_Source(std::span<const std::u8string_view> parts)
        : m_parts { parts }
    {
    }

    constexpr void operator()(std::span<char8_t> buffer, std::size_t n)
    {
        COWEL_ASSERT(n <= buffer.size());
        std::size_t i = 0;

        while (i < n) {
            COWEL_ASSERT(!m_parts.empty());

            const std::u8string_view current = m_parts.front();
            const std::size_t remaining = std::min(n, current.size() - m_offset_in_front);
            std::ranges::copy_n(current.data(), std::ptrdiff_t(remaining), buffer.data() + i);
            i += remaining;
            m_offset_in_front += remaining;
            if (m_offset_in_front == current.size()) {
                m_offset_in_front = 0;
                m_parts = m_parts.subspan(1);
            }
        }
    }
};

/// @brief A character source which obtains characters from the UTF-8 code units of a given UTF-32
/// string.
struct Code_Points_Char_Source {
private:
    std::u32string_view m_string;
    std::size_t m_offset_in_code_point = 0;

public:
    [[nodiscard]]
    Code_Points_Char_Source(std::u32string_view str)
        : m_string { str }
    {
    }

    constexpr void operator()(std::span<char8_t> buffer, std::size_t n)
    {
        COWEL_ASSERT(n <= buffer.size());
        std::size_t i = 0;

        while (i < n) {
            COWEL_ASSERT(!m_string.empty());

            const char32_t current_point = m_string.front();
            const auto current = utf8::encode8_unchecked(current_point);

            const std::size_t remaining
                = std::min(n, std::size_t(current.length) - m_offset_in_code_point);
            std::ranges::copy_n(
                current.code_units.data(), std::ptrdiff_t(remaining), buffer.data() + i
            );
            i += remaining;
            m_offset_in_code_point += remaining;
            if (m_offset_in_code_point == std::size_t(current.length)) {
                m_offset_in_code_point = 0;
                m_string.remove_prefix(1);
            }
        }
    }
};

/// @brief A class template which is convertible to a `Char_Sequence`.
/// This is necessary because `Char_Sequence` is non-owning,
/// so any state we have in the `make_` functions cannot be referenced
/// without immediately creating a dangling reference.
template <char_source8 Source>
struct Deferred_Char_Sequence {
    std::size_t size;
    Source source;

    [[nodiscard]]
    constexpr operator Char_Sequence8()
    {
        return { size, source };
    }
};

/// @brief Creates a `Char_Sequence` containing `str`.
[[nodiscard]]
constexpr Char_Sequence8 make_char_sequence(std::u8string_view str)
{
    return Char_Sequence8 { str };
}

/// @brief Creates a `Char_Sequence` containing `n` repetitions of `n`.
[[nodiscard]]
constexpr Deferred_Char_Sequence<Repeated_String_View_Char_Source>
make_char_sequence(std::size_t n, std::u8string_view str)
{
    return { n, str };
}

/// @brief Creates a `Char_Sequence` containing `strings`, concatenated.
[[nodiscard]]
constexpr Deferred_Char_Sequence<Joined_Char_Source>
joined_char_sequence(std::span<const std::u8string_view> strings)
{
    const auto total_length = [&] {
        std::size_t sum = 0;
        for (const std::u8string_view& str : strings) {
            sum += str.size();
        }
        return sum;
    }();
    return { total_length, strings };
}

/// @brief Creates a `Char_Sequence` containing `strings`, concatenated.
[[nodiscard]]
constexpr Deferred_Char_Sequence<Joined_Char_Source>
joined_char_sequence(std::initializer_list<std::u8string_view> strings)
{
    return joined_char_sequence(std::span<const std::u8string_view> { strings });
}

/// @brief Creates a `Char_Sequence` containing a single code unit `c`.
[[nodiscard]]
constexpr Char_Sequence8 make_char_sequence(char8_t c)
{
    return Char_Sequence8 { c };
}

/// @brief Creates a `Char_Sequence` containing `n` repetitions of a code unit `c`.
[[nodiscard]]
constexpr Char_Sequence8 repeated_char_sequence(std::size_t n, char8_t c)
{
    return Char_Sequence8 { n, c };
}

/// @brief Creates a `Char_Sequence` containing the UTF-8 encoded contents of `str`.
[[nodiscard]]
constexpr Deferred_Char_Sequence<Code_Points_Char_Source> make_char_sequence(std::u32string_view str
)
{
    const std::size_t n = utf8::count_code_units_unchecked(str);
    return { n, Code_Points_Char_Source { str } };
}

/// @brief Creates a `Char_Sequence` containing a single code point `c`.
[[nodiscard]]
constexpr Char_Sequence8 make_char_sequence(char32_t c)
{
    const utf8::Code_Units_And_Length encoded = utf8::encode8_unchecked(c);
    const auto string = Static_String8<4> { encoded.code_units, std::size_t(encoded.length) };
    COWEL_ASSERT(!string.empty());
    return Char_Sequence8 { string };
}

/// @brief Creates a `Char_Sequence` containing `n` repetitions of a code point `c`.
[[nodiscard]]
constexpr Deferred_Char_Sequence<Repeated_Static_String_Char_Source<4>>
repeated_char_sequence(std::size_t n, char32_t c)
{
    const utf8::Code_Units_And_Length encoded = utf8::encode8_unchecked(c);
    const auto string = Static_String8<4> { encoded.code_units, std::size_t(encoded.length) };
    COWEL_ASSERT(!string.empty());
    return { string.length() * n, string };
}

} // namespace cowel

#endif
