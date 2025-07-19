#include <string_view>
#include <vector>

#include "cowel/util/strings.hpp"

#include "cowel/policy/syntax_highlight.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/directive_arguments.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/theme_to_css.hpp"

using namespace std::string_view_literals;

namespace cowel {
namespace {

void diagnose(
    Syntax_Highlight_Error error,
    std::u8string_view lang,
    const ast::Directive& d,
    Context& context
)
{
    if (!context.emits(Severity::warning)) {
        return;
    }
    switch (error) {
    case Syntax_Highlight_Error::unsupported_language: {
        if (lang.empty()) {
            context.try_warning(
                diagnostic::highlight_language, d.get_source_span(),
                u8"Syntax highlighting was not possible because no language was given, "
                u8"and automatic language detection was not possible. "
                u8"Please use \\tt{...} or \\pre{...} if you want a code (block) "
                u8"without any syntax highlighting."sv
            );
            break;
        }
        const std::u8string_view message[] {
            u8"Unable to apply syntax highlighting because the specified language \"",
            lang,
            u8"\" is not supported.",
        };
        context.emit_warning(
            diagnostic::highlight_language, d.get_source_span(), joined_char_sequence(message)
        );
        break;
    }
    case Syntax_Highlight_Error::bad_code: {
        const std::u8string_view message[] {
            u8"Unable to apply syntax highlighting because the code is not valid "
            u8"for the specified language \"",
            lang,
            u8"\".",
        };
        context.emit_warning(
            diagnostic::highlight_malformed, d.get_source_span(), joined_char_sequence(message)
        );
        break;
    }
    case Syntax_Highlight_Error::other: {
        const std::u8string_view message[] {
            u8"Unable to apply syntax highlighting because of an internal error.",
            lang,
            u8"\".",
        };
        context.emit_warning(
            diagnostic::highlight_error, d.get_source_span(), joined_char_sequence(message)
        );
        break;
    }
    }
}

} // namespace

Content_Status
Code_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context) const
{
    Argument_Matcher args { parameters, context.get_transient_memory() };
    args.match(d.get_arguments());

    const Greedy_Result<String_Argument> lang
        = get_string_argument(lang_parameter, d, args, context);
    if (status_is_break(lang.status())) {
        return lang.status();
    }

    const Greedy_Result<String_Argument> prefix
        = get_string_argument(prefix_parameter, d, args, context);
    if (status_is_break(prefix.status())) {
        return prefix.status();
    }

    const Greedy_Result<String_Argument> suffix
        = get_string_argument(suffix_parameter, d, args, context);
    if (status_is_break(suffix.status())) {
        return suffix.status();
    }

    const auto borders = m_display == Directive_Display::in_line
        ? Greedy_Result<bool> { true }
        : get_yes_no_argument(
              borders_parameter, diagnostic::codeblock::borders_invalid, d, args, context, true
          );
    if (status_is_break(borders.status())) {
        return borders.status();
    }

    const auto nested = m_display == Directive_Display::in_line
        ? Greedy_Result<bool> { false }
        : get_yes_no_argument(
              nested_parameter, diagnostic::code::nested_invalid, d, args, context, false
          );
    if (status_is_break(nested.status())) {
        return nested.status();
    }
    const Content_Status args_status = status_concat(
        lang.status(), prefix.status(), suffix.status(), borders.status(), nested.status()
    );

    ensure_paragraph_matches_display(out, m_display);

    // All content written to out is HTML anyway,
    // so we don't need an extra HTML_Content_Policy here.
    HTML_Writer writer { out };
    const bool has_tags = !*nested;

    if (has_tags) {
        Attribute_Writer attributes = writer.open_tag_with_attributes(m_tag_name);
        if (!*borders) {
            COWEL_ASSERT(m_display != Directive_Display::in_line);
            attributes.write_class(u8"borderless");
        }
        attributes.end();
    }

    const bool should_trim = m_pre_compat_trim == Pre_Trimming::yes;

    Vector_Text_Sink buffer_sink { Output_Language::html, context.get_transient_memory() };
    auto& chosen_sink = should_trim ? buffer_sink : static_cast<Text_Sink&>(out);

    Syntax_Highlight_Policy highlight_policy //
        { context.get_transient_memory(), prefix->string, suffix->string };
    const auto highlight_status = consume_all(highlight_policy, d.get_content(), context);

    const Result<void, Syntax_Highlight_Error> result
        = highlight_policy.write_highlighted(chosen_sink, context, lang->string);
    if (!result) {
        diagnose(result.error(), lang->string, d, context);
    }
    if (should_trim) {
        // https://html.spec.whatwg.org/dev/grouping-content.html#the-pre-element
        // Leading newlines immediately following <pre> are stripped anyway.
        // The same applies to any elements styled "white-space: pre".
        // In general, it is best to remove these.
        // To ensure portability, we need to trim away newlines (if any).
        auto inner_html = as_u8string_view(*buffer_sink);
        while (inner_html.starts_with(u8'\n')) {
            inner_html.remove_prefix(1);
        }
        while (inner_html.ends_with(u8'\n')) {
            inner_html.remove_suffix(1);
        }
        out.write(inner_html, Output_Language::html);
    }

    if (has_tags) {
        writer.close_tag(m_tag_name);
    }

    return status_concat(args_status, highlight_status);
}

Content_Status
Highlight_As_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context)
    const
{
    Argument_Matcher args { parameters, context.get_transient_memory() };

    std::pmr::vector<char8_t> name_data { context.get_transient_memory() };
    const Result<bool, Content_Status> has_name_result
        = argument_to_plaintext(name_data, d, args, name_parameter, context);
    if (!has_name_result) {
        return has_name_result.error();
    }
    if (!*has_name_result) {
        context.try_error(
            diagnostic::hl::name_missing, d.get_source_span(),
            u8"A name parameter is required to specify the kind of highlight to apply."sv
        );
        return try_generate_error(out, d, context);
    }
    const auto name_string = as_u8string_view(name_data);

    const std::optional<ulight::Highlight_Type> type = highlight_type_by_long_string(name_string);
    if (!type) {
        const std::u8string_view message[] {
            u8"The given highlight name \"",
            name_string,
            u8"\" is not a valid ulight highlight name (long form).",
        };
        context.try_error(
            diagnostic::hl::name_invalid, d.get_source_span(), joined_char_sequence(message)
        );
        return try_generate_error(out, d, context);
    }

    const std::u8string_view short_name = ulight::highlight_type_short_string_u8(*type);
    COWEL_ASSERT(!short_name.empty());

    HTML_Content_Policy policy { out };
    HTML_Writer writer { policy };
    writer
        .open_tag_with_attributes(u8"h-") //
        .write_attribute(u8"data-h", short_name)
        .end();
    const Content_Status result = consume_all(policy, d.get_content(), context);
    if (status_is_break(result)) {
        return result;
    }
    writer.close_tag(u8"h-");
    return result;
}

} // namespace cowel
