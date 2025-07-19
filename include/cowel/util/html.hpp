#ifndef COWEL_HTML_HPP
#define COWEL_HTML_HPP

#include <concepts>
#include <string_view>

#include "ulight/impl/ascii_algorithm.hpp"

#include "cowel/util/html_entities.hpp"

namespace cowel {

template <typename F>
concept string_or_char_consumer = requires(F& f, std::u8string_view s, char8_t c) {
    f(s);
    f(c);
};

template <string_or_char_consumer Out, std::invocable<char8_t> Predicate>
void append_html_escaped(Out& out, std::u8string_view text, Predicate p)
{
    while (!text.empty()) {
        const std::size_t safe_length = ulight::ascii::length_if_not(text, p);
        out(text.substr(0, safe_length));
        text.remove_prefix(safe_length);
        if (text.empty()) {
            break;
        }
        out(html_entity_of(text.front()));
        text.remove_prefix(1);
    }
}

/// @brief Appends text to the vector where code units in `charset`
/// are replaced with their corresponding HTML entities.
/// For example, if `charset` includes `&`, `&amp;` is appended in its stead.
///
/// `charset` shall be a subset of the entities supported by `html_entity_of`.
template <string_or_char_consumer Out>
void append_html_escaped_of(Out& out, std::u8string_view text, std::u8string_view charset)
{
    append_html_escaped(out, text, [&](char8_t c) { return charset.contains(c); });
}

} // namespace cowel

#endif
