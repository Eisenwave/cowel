#include <string_view>
#include <vector>

#include "cowel/util/strings.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/directive_arguments.hpp"
#include "cowel/directive_processing.hpp"

using namespace std::string_view_literals;

namespace cowel {

Content_Status
WG21_Block_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context)
    const
{
    constexpr std::u8string_view tag = u8"wg21-block";

    warn_ignored_argument_subset(d.get_arguments(), context, Argument_Subset::positional);

    try_enter_paragraph(out);

    HTML_Writer writer { out };
    Attribute_Writer attributes = writer.open_tag_with_attributes(tag);
    const auto attributes_status = named_arguments_to_attributes(attributes, d, context);
    attributes.end();

    if (status_is_break(attributes_status)) {
        writer.close_tag(tag);
        return attributes_status;
    }

    writer.write_inner_html(u8"[<i>");
    writer.write_inner_text(m_prefix);
    writer.write_inner_html(u8"</i>: ");

    const auto content_status = consume_all(out, d.get_content(), context);

    writer.write_inner_html(u8" \N{EM DASH} <i>");
    writer.write_inner_text(m_suffix);
    writer.write_inner_html(u8"</i>]");
    writer.close_tag(tag);

    return status_concat(attributes_status, content_status);
}

Content_Status
WG21_Head_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context) const
{
    static constexpr std::u8string_view parameters[] { u8"title" };
    Argument_Matcher args { parameters, context.get_transient_memory() };
    args.match(d.get_arguments());

    try_leave_paragraph(out);

    HTML_Content_Policy html_policy { out };
    HTML_Writer writer { html_policy };
    writer
        .open_tag_with_attributes(u8"div") //
        .write_class(u8"wg21-head")
        .end();

    const int title_index = args.get_argument_index(u8"title");
    if (title_index < 0) {
        context.try_warning(
            diagnostic::wg21_head::no_title, d.get_source_span(),
            u8"A wg21_head directive requires a title argument"sv
        );
    }

    const ast::Argument& title_arg = d.get_arguments()[std::size_t(title_index)];

    {
        // FIXME: multiple evaluations of title input
        std::pmr::vector<char8_t> title_plaintext { context.get_transient_memory() };
        const auto status = to_plaintext(title_plaintext, title_arg.get_content(), context);
        if (status != Content_Status::ok) {
            writer.close_tag(u8"div");
            return status;
        }

        const auto title_string = as_u8string_view(title_plaintext);

        const auto scope = context.get_sections().go_to_scoped(section_name::document_head);
        HTML_Writer head_writer { context.get_sections().current_policy() };
        head_writer.open_tag(u8"title");
        head_writer.write_inner_text(title_string);
        head_writer.close_tag(u8"title");
    }

    writer.open_tag(u8"h1");
    const auto title_status = consume_all(html_policy, title_arg.get_content(), context);
    writer.close_tag(u8"h1");
    if (status_is_break(title_status)) {
        return title_status;
    }

    writer.write_inner_html(u8'\n');
    const auto status = consume_all(html_policy, d.get_content(), context);
    writer.close_tag(u8"div");

    return status;
}

} // namespace cowel
