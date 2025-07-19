#include <gtest/gtest.h>

#include "cowel/util/char_sequence.hpp"
#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/char_sequence_ops.hpp"
#include "cowel/util/strings.hpp"

namespace cowel {
namespace {

using namespace std::string_view_literals;

TEST(Char_Sequence, empty)
{
    Char_Sequence8 chars;
    ASSERT_TRUE(chars.empty());
    ASSERT_EQ(chars.size(), 0);
    ASSERT_EQ(chars.length(), 0);
    ASSERT_EQ(chars.as_string_view(), std::u8string_view {});
    ASSERT_EQ(chars.extract({}), 0);
    // wrapping in u8string_view because for some reason,
    // trying to print std::u8string results in a linker error
    ASSERT_EQ(as_u8string_view(to_string(chars)), u8"");
}

TEST(Char_Sequence, zero_capacity_static)
{
    Char_Sequence8 chars = Static_String8<0> {};
    ASSERT_TRUE(chars.empty());
}

TEST(Char_Sequence, string_view)
{
    constexpr std::u8string_view str = u8"awoo";

    Char_Sequence8 chars = str;
    ASSERT_FALSE(chars.empty());
    ASSERT_EQ(chars.size(), str.length());
    ASSERT_EQ(chars.length(), str.length());

    ASSERT_EQ(chars.as_string_view(), str);

    char8_t buffer[str.length()];
    ASSERT_EQ(chars.extract({}), 0);
    chars.extract(buffer);
    ASSERT_EQ(as_u8string_view(buffer), str);
    ASSERT_TRUE(chars.empty());
    ASSERT_EQ(chars.size(), 0);
    ASSERT_EQ(chars.length(), 0);
}

TEST(Char_Sequence, single_code_unit)
{
    Char_Sequence8 chars { u8'x' };
    ASSERT_EQ(chars.length(), 1);
    ASSERT_EQ(chars.as_string_view(), u8"x");

    char8_t buffer;
    chars.extract({ &buffer, 1 });
    ASSERT_EQ(buffer, u8'x');
    ASSERT_TRUE(chars.empty());
}

TEST(Char_Sequence, repeated_code_unit)
{
    Char_Sequence8 chars { 7, u8'x' };
    ASSERT_EQ(chars.length(), 7);
    ASSERT_EQ(chars.as_contiguous(), nullptr);

    using array_type = std::array<char8_t, 5>;
    array_type buffer;
    chars.extract(buffer);
    ASSERT_EQ(buffer, (array_type { u8'x', u8'x', u8'x', u8'x', u8'x' }));
    ASSERT_EQ(chars.length(), 2);
}

TEST(Char_Sequence, static_string)
{
    constexpr std::u8string_view str = u8"awoo"sv;
    constexpr Static_String8<4> static_str = str;
    static_assert(static_str == str);

    Char_Sequence8 chars { static_str };
    ASSERT_EQ(chars.length(), str.length());
    ASSERT_EQ(chars.as_string_view(), str);

    using array_type = std::array<char8_t, 4>;
    array_type buffer;
    chars.extract(buffer);
    ASSERT_EQ(buffer, (array_type { u8'a', u8'w', u8'o', u8'o' }));
    ASSERT_TRUE(chars.empty());
}

TEST(Char_Sequence, transcoded_code_point)
{
    constexpr char32_t c = U'\N{GRINNING FACE}';
    constexpr std::u8string_view code_units = u8"\N{GRINNING FACE}"sv;

    static_assert(
        std::is_same_v<decltype(make_char_sequence(c)), Char_Sequence8>,
        "The following variable would dangle otherwise."
    );
    Char_Sequence8 chars = make_char_sequence(c);
    ASSERT_EQ(chars.length(), code_units.length());
    ASSERT_EQ(chars.as_string_view(), code_units);

    using array_type = std::array<char8_t, code_units.length()>;
    array_type buffer;
    chars.extract(buffer);
    ASSERT_EQ(as_u8string_view(buffer), code_units);
    ASSERT_TRUE(chars.empty());
}

TEST(Char_Sequence, repeated_code_point)
{
    constexpr char32_t c = U'\N{GRINNING FACE}';
    constexpr std::u8string_view code_units = u8"\N{GRINNING FACE}\N{GRINNING FACE}"sv;
    auto source = repeated_char_sequence(2, c);

    // chars = repeated_char_sequence(2, c)
    // would dangle.
    Char_Sequence8 chars = source;
    ASSERT_EQ(chars.length(), code_units.length());
    ASSERT_EQ(chars.as_contiguous(), nullptr);
    ASSERT_EQ(as_u8string_view(to_string(chars)), code_units);
}

TEST(Char_Sequence, joined)
{
    constexpr std::u8string_view parts[] { u8"awoo", u8"baka", u8"chan", u8"." };
    constexpr std::u8string_view joined = u8"awoobakachan.";

    auto source = joined_char_sequence(parts);

    Char_Sequence8 chars = source;
    ASSERT_EQ(chars.length(), joined.length());
    ASSERT_EQ(chars.as_contiguous(), nullptr);
    ASSERT_EQ(as_u8string_view(to_string(chars)), joined);
}

} // namespace
} // namespace cowel
