#ifndef COWEL_STATIC_STRING_HPP
#define COWEL_STATIC_STRING_HPP

#include <algorithm>
#include <array>
#include <span>
#include <string_view>

#include "cowel/util/assert.hpp"
#include "cowel/util/meta.hpp"

namespace cowel {

template <char_like Char, std::size_t capacity>
struct Basic_Static_String {
    using array_type = std::array<Char, capacity>;

private:
    array_type m_buffer {};
    std::size_t m_length = 0;

public:
    [[nodiscard]]
    constexpr Basic_Static_String(array_type array, std::size_t length)
        : m_buffer { array }
        , m_length { length }
    {
        COWEL_ASSERT(length <= capacity);
    }

    [[nodiscard]]
    constexpr Basic_Static_String(std::basic_string_view<Char> str)
        : m_length { str.length() }
    {
        COWEL_ASSERT(str.length() <= capacity);
        std::ranges::copy(str, m_buffer.data());
    }

    constexpr Basic_Static_String() = default;

    [[nodiscard]]
    constexpr bool empty() const noexcept
    {
        return m_length == 0;
    }

    [[nodiscard]]
    constexpr std::size_t size() const noexcept
    {
        return m_length;
    }

    [[nodiscard]]
    constexpr std::size_t length() const noexcept
    {
        return m_length;
    }

    [[nodiscard]]
    constexpr Char* data() noexcept
    {
        return m_buffer.data();
    }

    [[nodiscard]]
    constexpr const Char* data() const noexcept
    {
        return m_buffer.data();
    }

    constexpr void clear() noexcept
    {
        m_length = 0;
    }

    [[nodiscard]]
    constexpr typename array_type::iterator begin() noexcept
    {
        return m_buffer.begin();
    }

    [[nodiscard]]
    constexpr typename array_type::const_iterator begin() const noexcept
    {
        return m_buffer.begin();
    }

    [[nodiscard]]
    constexpr typename array_type::iterator end() noexcept
    {
        return m_buffer.end();
    }

    [[nodiscard]]
    constexpr typename array_type::const_iterator end() const noexcept
    {
        return m_buffer.end();
    }

    constexpr void remove_prefix(std::size_t n)
    {
        // TODO: Use std::ranges overload once available.
        COWEL_ASSERT(n <= m_length);
        std::shift_left(m_buffer.begin(), m_buffer.end(), std::ptrdiff_t(n));
        m_length -= n;
    }

    constexpr void remove_suffix(std::size_t n)
    {
        COWEL_ASSERT(n <= m_length);
        m_length -= n;
    }

    [[nodiscard]]
    constexpr std::span<Char> as_span()
    {
        return { m_buffer.data(), m_length };
    }

    [[nodiscard]]
    constexpr std::span<const Char> as_span() const
    {
        return { m_buffer.data(), m_length };
    }

    [[nodiscard]]
    constexpr std::basic_string_view<Char> as_string() const
    {
        return { m_buffer.data(), m_length };
    }

    [[nodiscard]]
    constexpr operator std::basic_string_view<Char>() const
    {
        return as_string();
    }

    [[nodiscard]]
    friend constexpr bool
    operator==(const Basic_Static_String& x, const Basic_Static_String& y) noexcept
    {
        return x.as_string() == y.as_string();
    }

    [[nodiscard]]
    friend constexpr bool
    operator==(const Basic_Static_String& x, std::basic_string_view<Char> y) noexcept
    {
        return x.as_string() == y;
    }

    [[nodiscard]]
    friend constexpr bool
    operator<=>(const Basic_Static_String& x, const Basic_Static_String& y) noexcept
    {
        return x.as_string() <=> y.as_string();
    }

    [[nodiscard]]
    friend constexpr bool
    operator<=>(const Basic_Static_String& x, std::basic_string_view<Char> y) noexcept
    {
        return x.as_string() <=> y;
    }
};

template <std::size_t capacity>
using Static_String = Basic_Static_String<char, capacity>;
template <std::size_t capacity>
using Static_String8 = Basic_Static_String<char8_t, capacity>;

} // namespace cowel

#endif
