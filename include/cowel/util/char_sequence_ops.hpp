#ifndef COWEL_CHAR_SEQUENCE_UTIL_HPP
#define COWEL_CHAR_SEQUENCE_UTIL_HPP

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "cowel/util/char_sequence.hpp"

namespace cowel {

namespace detail {

template <typename Seq_Container>
inline void append_impl(Seq_Container& out, Char_Sequence8 chars)
{
    if (chars.empty()) {
        return;
    }
    if (const std::u8string_view sv = chars.as_string_view(); !sv.empty()) {
        out.insert(out.end(), sv.begin(), sv.end());
        return;
    }
    // TODO: use resize_and_overwrite for strings
    const std::size_t initial_size = out.size();
    out.resize(initial_size + chars.size());
    chars.extract(std::span { out }.subspan(initial_size));
}

} // namespace detail

inline void append(std::pmr::vector<char8_t>& out, Char_Sequence8 chars)
{
    detail::append_impl(out, chars);
}

template <typename Traits, typename Alloc>
inline void append(std::basic_string<char8_t, Traits, Alloc>& out, Char_Sequence8 chars)
{
    detail::append_impl(out, chars);
}

[[nodiscard]]
inline std::pmr::u8string to_string(Char_Sequence8 chars, std::pmr::memory_resource* memory)
{
    std::pmr::u8string result { memory };
    append(result, chars);
    return result;
}

[[nodiscard]]
inline std::u8string to_string(Char_Sequence8 chars)
{
    std::u8string result;
    append(result, chars);
    return result;
}

} // namespace cowel

#endif
