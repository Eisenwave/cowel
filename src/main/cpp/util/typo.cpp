#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory_resource>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "cowel/util/levenshtein.hpp"
#include "cowel/util/levenshtein_utf8.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/typo.hpp"
#include "cowel/util/unicode.hpp"

namespace cowel {

namespace {

[[nodiscard]]
Distant<std::size_t> closest_match_ascii(
    std::span<const std::u8string_view> haystack,
    std::u8string_view needle,
    std::pmr::memory_resource* memory
)
{
    std::pmr::u32string hay32 { memory };
    std::pmr::vector<std::size_t> matrix_data { memory };

    Distant<std::size_t> best_match;

    for (std::size_t i = 0; i < haystack.size(); ++i) {
        const std::u8string_view hay = haystack[i];
        const auto distance = [&] -> std::size_t {
            if (is_ascii(hay)) {
                return code_unit_levenshtein_distance(hay, needle, memory);
            }
            const auto needle32
                = needle | std::views::transform([](char8_t c) { return char32_t(c); });
            hay32.clear();
            hay32.reserve(hay.size());
            std::ranges::copy(utf8::Code_Point_View { hay }, std::back_inserter(hay32));

            matrix_data.resize((hay32.size() + 1) * (needle32.size() + 1));

            return levenshtein_distance(hay32, needle32, std::span { matrix_data });
        }();

        if (distance < best_match.distance) {
            best_match.value = i;
            best_match.distance = distance;
        }
    }

    return best_match;
}

} // namespace

Distant<std::size_t> closest_match(
    std::span<const std::u8string_view> haystack,
    std::u8string_view needle,
    std::pmr::memory_resource* memory
)
{
    if (is_ascii(needle)) {
        return closest_match_ascii(haystack, needle, memory);
    }

    const std::pmr::u32string needle32 = detail::to_utf32(needle, memory);
    std::pmr::u32string hay32 { memory };
    std::pmr::vector<std::size_t> matrix_data { memory };

    Distant<std::size_t> best_match;

    for (std::size_t i = 0; i < haystack.size(); ++i) {
        const std::u8string_view hay = haystack[i];
        const auto distance = [&] -> std::size_t {
            hay32.clear();
            hay32.reserve(hay.size());
            std::ranges::copy(utf8::Code_Point_View { hay }, std::back_inserter(hay32));
            matrix_data.resize((hay32.size() + 1) * (needle32.size() + 1));
            return detail::levenshtein_distance32(hay32, needle32, matrix_data);
        }();

        if (distance < best_match.distance) {
            best_match.value = i;
            best_match.distance = distance;
        }
    }

    return best_match;
}

} // namespace cowel
