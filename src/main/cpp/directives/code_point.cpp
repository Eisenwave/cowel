#include <expected>
#include <string_view>
#include <vector>

#include "cowel/util/char_sequence.hpp"
#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/code_point_names.hpp"
#include "cowel/util/from_chars.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/to_chars.hpp"
#include "cowel/util/unicode.hpp"

#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/directive_arguments.hpp"
#include "cowel/directive_processing.hpp"

using namespace std::string_view_literals;

namespace cowel {
namespace {

[[nodiscard]]
Result<char32_t, Content_Status> code_point_by_generated_digits(
    std::span<const ast::Content> content,
    const File_Source_Span& source_span,
    Context& context
)
{
    std::pmr::vector<char8_t> data { context.get_transient_memory() };
    const auto status = to_plaintext(data, content, context);
    if (status != Content_Status::ok) {
        return status;
    }

    const std::u8string_view digits = trim_ascii_blank(as_u8string_view(data));
    if (digits.empty()) {
        context.try_error(
            diagnostic::U::blank, source_span,
            u8"Expected a sequence of hexadecimal digits, but got a blank string."sv
        );
        return Content_Status::error;
    }

    const std::optional<std::uint32_t> value = from_chars<std::uint32_t>(digits, 16);
    if (!value) {
        const std::u8string_view message[] {
            u8"Expected a sequence of hexadecimal digits, but got \""sv,
            digits,
            u8"\"."sv,
        };
        context.try_error(diagnostic::U::digits, source_span, joined_char_sequence(message));
        return Content_Status::error;
    }

    const auto code_point = char32_t(*value);
    if (!is_scalar_value(code_point)) {
        const Characters8 chars = to_characters8(code_point);
        const std::u8string_view message[] {
            u8"The computed code point U+",
            chars.as_string(),
            u8" is not a Unicode scalar value. ",
            u8"Therefore, it cannot be encoded as UTF-8.",
        };
        context.try_error(diagnostic::U::nonscalar, source_span, joined_char_sequence(message));
        return Content_Status::error;
    }

    return code_point;
}

[[nodiscard]]
Result<char32_t, Content_Status> code_point_by_generated_name(
    std::span<const ast::Content> content,
    const File_Source_Span& source_span,
    Context& context
)
{
    constexpr auto error_point = char32_t(-1);

    std::pmr::vector<char8_t> name { context.get_transient_memory() };
    const auto status = to_plaintext(name, content, context);
    if (status != Content_Status::ok) {
        return status;
    }
    const auto name_string = as_u8string_view(name);

    const std::u8string_view digits = trim_ascii_blank(name_string);
    if (digits.empty()) {
        context.try_error(
            diagnostic::N::blank, source_span,
            u8"Expected the name of a Unicode code point, but got a blank string."sv
        );
        return Content_Status::error;
    }

    const char32_t code_point = code_point_by_name(name_string);
    if (code_point == error_point) {
        const std::u8string_view message[] {
            u8"Expected an (all caps) name of a Unicode code point, but got \"",
            name_string,
            u8"\".",
        };
        context.try_error(diagnostic::N::invalid, source_span, joined_char_sequence(message));
        return Content_Status::error;
    }

    return code_point;
}

} // namespace

[[nodiscard]]
Content_Status
Code_Point_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context)
    const
{
    // TODO: this warning should use some blanket method
    if (!d.get_arguments().empty()) {
        const File_Source_Span pos = d.get_arguments().front().get_source_span();
        context.try_warning(
            diagnostic::ignored_args, pos, u8"Arguments to this directive are ignored."sv
        );
    }

    const Result<char32_t, Content_Status> code_point = get_code_point(d, context);
    if (!code_point) {
        COWEL_ASSERT(code_point.error() != Content_Status::ok);
        return try_generate_error(out, d, context, code_point.error());
    }

    try_enter_paragraph(out);

    out.write(make_char_sequence(*code_point), Output_Language::text);
    return Content_Status::ok;
}

[[nodiscard]]
Result<char32_t, Content_Status>
Char_By_Num_Behavior::get_code_point(const ast::Directive& d, Context& context) const
{
    return code_point_by_generated_digits(d.get_content(), d.get_source_span(), context);
}

[[nodiscard]]
Result<char32_t, Content_Status>
Char_By_Name_Behavior::get_code_point(const ast::Directive& d, Context& context) const
{
    return code_point_by_generated_name(d.get_content(), d.get_source_span(), context);
}

Content_Status
Char_Get_Num_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context)
    const
{
    static constexpr std::u8string_view parameters[] { u8"zfill", u8"base", u8"lower" };

    constexpr std::size_t default_zfill = 0;
    constexpr std::size_t min_zfill = 0;
    constexpr std::size_t max_zfill = 1024;

    constexpr std::size_t default_base = 16;
    constexpr std::size_t min_base = 2;
    constexpr std::size_t max_base = 16;

    Argument_Matcher args { parameters, context.get_transient_memory() };
    args.match(d.get_arguments());

    const Greedy_Result<std::size_t> zfill = get_integer_argument(
        u8"zfill", diagnostic::Udigits::zfill_not_an_integer, diagnostic::Udigits::zfill_range, //
        args, d, context, default_zfill, min_zfill, max_zfill
    );
    if (status_is_break(zfill.status())) {
        return zfill.status();
    }

    const Greedy_Result<std::size_t> base = get_integer_argument(
        u8"base", diagnostic::Udigits::base_not_an_integer, diagnostic::Udigits::base_range, //
        args, d, context, default_base, min_base, max_base
    );
    if (status_is_break(base.status())) {
        return base.status();
    }

    const Greedy_Result<bool> is_lower = get_yes_no_argument(
        u8"lower", diagnostic::Udigits::lower_invalid, d, args, context, false
    );
    if (status_is_break(is_lower.status())) {
        return is_lower.status();
    }
    const auto args_status = status_concat(zfill.status(), base.status(), is_lower.status());

    std::pmr::vector<char8_t> input { context.get_transient_memory() };
    const auto input_status = to_plaintext(input, d.get_content(), context);
    if (input_status != Content_Status::ok) {
        return status_concat(args_status, input_status);
    }

    const auto input_text = as_u8string_view(input);

    if (input_text.empty()) {
        context.try_error(
            diagnostic::Udigits::blank, d.get_source_span(),
            u8"Nothing can be generated because the input is empty."sv
        );
        return try_generate_error(out, d, context);
    }

    const std::expected<utf8::Code_Point_And_Length, utf8::Unicode_Error> decode_result
        = utf8::decode_and_length(input_text);
    if (!decode_result) {
        context.try_error(
            diagnostic::Udigits::malformed, d.get_source_span(),
            u8"The input could not be interpreted as a code point because it is malformed UTF-8."sv
        );
        return try_generate_error(out, d, context);
    }

    const auto [code_point, length] = *decode_result;
    if (std::size_t(length) != input.size()) {
        COWEL_ASSERT(std::size_t(length) <= input.size());
        context.try_warning(
            diagnostic::Udigits::ignored, d.get_source_span(),
            u8"Some of the code units inside were ignored because only the first given code point "
            u8"is converted. "
            u8"This can happen if you type a character inside \\Udigits that consist of multiple "
            u8"code points, like a country flag."sv
        );
    }

    try_enter_paragraph(out);

    const bool convert_to_upper = !*is_lower;
    const Characters8 chars
        = to_characters8(std::uint32_t(code_point), int(*base), convert_to_upper);
    const std::size_t pad_length = std::min(*zfill, chars.length());
    out.write(repeated_char_sequence(pad_length, u8'0'), Output_Language::text);
    out.write(chars.as_string(), Output_Language::text);

    return status_concat(args_status, input_status);
}

} // namespace cowel
