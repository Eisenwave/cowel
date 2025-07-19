#include <array>
#include <cstddef>
#include <memory_resource>
#include <span>
#include <string_view>

#include <gtest/gtest.h>

#include "cowel/util/html_entities.hpp"
#include "cowel/util/html_writer.hpp"

#include "cowel/policy/capture.hpp"

namespace cowel {
namespace {

[[nodiscard]]
std::u8string_view as_view(std::span<const char8_t> span)
{
    return { span.data(), span.size() };
}

struct HTML_Writer_Test : testing::Test {
    std::pmr::monotonic_buffer_resource memory;
    Vector_Text_Sink out { Output_Language::html, &memory };
    HTML_Writer writer { out };
};

TEST_F(HTML_Writer_Test, empty)
{
    constexpr std::u8string_view expected;
    EXPECT_EQ(expected, as_view(*out));
}

TEST_F(HTML_Writer_Test, inner_html)
{
    constexpr std::u8string_view expected = u8"<html>Hello, world!</html>";

    writer.write_inner_html(u8"<html>Hello, world!</html>");

    EXPECT_EQ(expected, as_view(*out));
}

TEST_F(HTML_Writer_Test, inner_text)
{
    constexpr std::u8string_view expected = u8"&lt;hello&amp;";

    writer.write_inner_text(u8"<hello&");

    EXPECT_EQ(expected, as_view(*out));
}

TEST_F(HTML_Writer_Test, tag)
{
    constexpr std::u8string_view expected = u8"<b>Hello, world!</b>";

    writer.open_tag(u8"b");
    writer.write_inner_text(u8"Hello, world!");
    writer.close_tag(u8"b");

    EXPECT_EQ(expected, as_view(*out));
}

TEST_F(HTML_Writer_Test, empty_tag)
{
    constexpr std::u8string_view expected = u8"<br/>";

    writer.write_self_closing_tag(u8"br");

    EXPECT_EQ(expected, as_view(*out));
}

TEST_F(HTML_Writer_Test, empty_attributes)
{
    constexpr std::u8string_view expected = u8"<x a b=\"\" c></x>";

    writer.open_tag_with_attributes(u8"x")
        .write_empty_attribute(u8"a", Attribute_Style::double_if_needed)
        .write_empty_attribute(u8"b", Attribute_Style::always_double)
        .write_empty_attribute(u8"c", Attribute_Style::double_if_needed)
        .end();
    writer.close_tag(u8"x");

    EXPECT_EQ(expected, as_view(*out));
}

TEST_F(HTML_Writer_Test, attributes_with_values_quotes_if_needed)
{
    constexpr std::u8string_view expected = u8"<x id=name class='a b' hidden></x>";

    writer.open_tag_with_attributes(u8"x")
        .write_attribute(u8"id", u8"name", Attribute_Style::single_if_needed)
        .write_attribute(u8"class", u8"a b", Attribute_Style::single_if_needed)
        .write_attribute(u8"hidden", u8"", Attribute_Style::single_if_needed)
        .end();
    writer.close_tag(u8"x");

    EXPECT_EQ(expected, as_view(*out));
}

TEST_F(HTML_Writer_Test, attributes_with_values_always_quotes)
{
    constexpr std::u8string_view expected = u8"<x id='name' class='a b' hidden=''></x>";

    writer.open_tag_with_attributes(u8"x")
        .write_attribute(u8"id", u8"name", Attribute_Style::always_single)
        .write_attribute(u8"class", u8"a b", Attribute_Style::always_single)
        .write_attribute(u8"hidden", u8"", Attribute_Style::always_single)
        .end();
    writer.close_tag(u8"x");

    EXPECT_EQ(expected, as_view(*out));
}

TEST_F(HTML_Writer_Test, attributes_but_empty)
{
    constexpr std::u8string_view expected = u8"<br/>";

    writer.open_tag_with_attributes(u8"br").end_empty();

    EXPECT_EQ(expected, as_view(*out));
}

TEST_F(HTML_Writer_Test, attributes_escape)
{
    constexpr std::u8string_view expected = u8"<x id='&apos;'/>";

    writer.open_tag_with_attributes(u8"x")
        .write_attribute(u8"id", u8"'", Attribute_Style::single_if_needed)
        .end_empty();

    EXPECT_EQ(expected, as_view(*out));
}

TEST(HTML_Entities, empty)
{
    constexpr std::array<char32_t, 2> expected {};
    const std::array<char32_t, 2> actual = code_points_by_character_reference_name(u8"");

    EXPECT_EQ(expected, actual);
}

TEST(HTML_Entities, amp)
{
    constexpr std::array<char32_t, 2> expected { U'&' };
    const std::array<char32_t, 2> actual = code_points_by_character_reference_name(u8"amp");

    EXPECT_EQ(expected, actual);
}

TEST(HTML_Entities, bne)
{
    constexpr std::array<char32_t, 2> expected { U'\u003D', U'\u20E5' };
    const std::array<char32_t, 2> actual = code_points_by_character_reference_name(u8"bne");

    EXPECT_EQ(expected, actual);
}

TEST(HTML_Entities, all_found)
{
    constexpr std::array<char32_t, 2> unexpected {};

    for (const std::u8string_view name : html_character_names) {
        const std::array<char32_t, 2> result = code_points_by_character_reference_name(name);
        EXPECT_NE(result, unexpected);
        const std::size_t length = result[1] != 0 ? 2uz : 1uz;
        const std::u32string_view other_string { result.data(), length };
        const std::u32string_view result_string = string_by_character_reference_name(name);
        EXPECT_EQ(result_string, other_string);
    }
}

} // namespace
} // namespace cowel
