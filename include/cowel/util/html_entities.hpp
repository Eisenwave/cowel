#ifndef COWEL_HTML_ENTITIES_HPP
#define COWEL_HTML_ENTITIES_HPP

#include <array>
#include <string_view>

namespace cowel {

/// @brief A lexicographically ordered array of HTML character names such as `u8"amp"`.
extern const std::array<std::u8string_view, 2231> html_character_names;

/// @brief Returns one or two code points given the name of an HTML charater reference.
/// See https://html.spec.whatwg.org/dev/named-characters.html#named-character-references.
///
/// If the name isn't a known named character reference, returns `{U+0000, U+0000}`.
/// If the name only corresponds to a single code point,
/// the second element in the array is `U+0000`.
///
/// For example, given `"amp"`, returns `{U+0026, U+0000}` (`&`),
/// and given `"caps"`, returns `{U+2229, U+FE00}` (`∩︀`)
[[nodiscard]]
std::array<char32_t, 2> code_points_by_character_reference_name(std::u8string_view name) noexcept;

/// @brief Like `code_points_by_character_reference_name`,
/// but returns a string of one or two code points when the character reference is recognized,
/// and an empty string otherwise.
[[nodiscard]]
std::u32string_view string_by_character_reference_name(std::u8string_view name) noexcept;

} // namespace cowel

#endif
