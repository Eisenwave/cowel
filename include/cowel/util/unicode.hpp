#ifndef COWEL_UNICODE_HPP
#define COWEL_UNICODE_HPP

#include "ulight/impl/unicode.hpp"

namespace cowel::utf8 {

using ulight::utf8::Code_Point_And_Length;
using ulight::utf8::Code_Point_Iterator;
using ulight::utf8::Code_Point_Iterator_Sentinel;
using ulight::utf8::Code_Point_View;
using ulight::utf8::code_points_unchecked;
using ulight::utf8::Code_Units_And_Length;
using ulight::utf8::decode;
using ulight::utf8::decode_and_length;
using ulight::utf8::decode_and_length_or_replacement;
using ulight::utf8::decode_and_length_unchecked;
using ulight::utf8::decode_unchecked;
using ulight::utf8::encode8_unchecked;
using ulight::utf8::Error_Code;
using ulight::utf8::error_code_message;
using ulight::utf8::is_valid;
using ulight::utf8::sequence_length;
using ulight::utf8::Unicode_Error;

/// @brief Returns the length of `str`, in code units, when encoded.
[[nodiscard]]
constexpr std::size_t count_code_units_unchecked(std::u32string_view str)
{
    std::size_t result = 0;
    for (const char32_t c : str) {
        const auto [_, code_units] = utf8::encode8_unchecked(c);
        result += std::size_t(code_units);
    }
    return result;
}

} // namespace cowel::utf8

#endif
