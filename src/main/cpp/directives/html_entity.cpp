#include <vector>

#include "cowel/util/char_sequence.hpp"
#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/from_chars.hpp"
#include "cowel/util/html_entities.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/directive_processing.hpp"

using namespace std::string_view_literals;

namespace cowel {
namespace {

[[nodiscard]]
std::u32string_view as_string_view(const std::array<char32_t, 2>& array)
{
    const std::size_t length = array[0] == 0 ? 0 : array[1] == 0 ? 1 : 2;
    return { array.data(), length };
}

void check_arguments(const ast::Directive& d, Context& context)
{
    if (!d.get_arguments().empty()) {
        const File_Source_Span pos = d.get_arguments().front().get_source_span();
        context.try_warning(
            diagnostic::ignored_args, pos, u8"Arguments to this directive are ignored."sv
        );
    }
}

[[nodiscard]]
std::array<char32_t, 2> get_code_points_from_digits(
    std::u8string_view digits,
    int base,
    const ast::Directive& d,
    Context& context
)
{
    std::optional<std::uint32_t> value = from_chars<std::uint32_t>(digits, base);
    if (!value) {
        const std::u8string_view message = base == 10
            ? u8"Expected a sequence of decimal digits."sv
            : u8"Expected a sequence of hexadecimal digits."sv;
        context.try_error(diagnostic::c::digits, d.get_source_span(), message);
        return {};
    }

    const auto code_point = char32_t(*value);
    if (!is_scalar_value(code_point)) {
        context.try_error(
            diagnostic::c::nonscalar, d.get_source_span(),
            u8"The given hex sequence is not a Unicode scalar value. "
            u8"Therefore, it cannot be encoded as UTF-8."sv
        );
        return {};
    }

    return { code_point };
}

[[nodiscard]]
std::array<char32_t, 2>
get_code_points(std::u8string_view trimmed_text, const ast::Directive& d, Context& context)
{
    if (trimmed_text.empty()) {
        context.try_error(
            diagnostic::c::blank, d.get_source_span(),
            u8"Expected an HTML character reference, but got a blank string."sv
        );
        return {};
    }
    if (trimmed_text[0] == u8'#') {
        const int base
            = trimmed_text.starts_with(u8"#x") || trimmed_text.starts_with(u8"#X") ? 16 : 10;
        return get_code_points_from_digits(trimmed_text.substr(2), base, d, context);
    }
    const std::array<char32_t, 2> result = code_points_by_character_reference_name(trimmed_text);
    if (result[0] == 0) {
        context.try_error(
            diagnostic::c::name, d.get_source_span(), u8"Invalid named HTML character."sv
        );
    }
    return result;
}

} // namespace

Content_Status
Char_By_Entity_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context)
    const
{
    check_arguments(d, context);

    try_enter_paragraph(out);

    std::pmr::vector<char8_t> data { context.get_transient_memory() };
    const auto input_status = to_plaintext(data, d.get_content(), context);
    if (input_status != Content_Status::ok) {
        return input_status;
    }

    const std::u8string_view trimmed_text = trim_ascii_blank(as_u8string_view(data));
    const std::array<char32_t, 2> code_points = get_code_points(trimmed_text, d, context);
    if (code_points[0] == 0) {
        return try_generate_error(out, d, context);
    }
    switch (out.get_language()) {
    case Output_Language::none: {
        break;
    }
    case Output_Language::text: {
        out.write(make_char_sequence(as_string_view(code_points)), Output_Language::text);
        break;
    }
    case Output_Language::html: {
        out.write(u8'&', Output_Language::html);
        out.write(trimmed_text, Output_Language::html);
        out.write(u8';', Output_Language::html);
        break;
    }
    default: {
        return Content_Status::ok;
    }
    }
    return Content_Status::ok;
}

} // namespace cowel
