#include <algorithm>
#include <string_view>
#include <vector>

#include "cowel/util/char_sequence.hpp"
#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/html_writer.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/url_encode.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/directive_arguments.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/services.hpp"

using namespace std::string_view_literals;

namespace cowel {
namespace {

template <typename Predicate>
void url_encode_to_writer(
    HTML_Writer& out,
    std::u8string_view url,
    Context& context,
    Predicate pred
)
{
    std::pmr::vector<char8_t> buffer { context.get_transient_memory() };
    url_encode_ascii_if(std::back_inserter(buffer), url, pred);
    out.write_inner_html(as_u8string_view(buffer));
}

constexpr std::u8string_view bib_item_id_prefix = u8"bib-item-";

constexpr auto is_url_always_encoded_lambda = [](char8_t c) { return is_url_always_encoded(c); };

void write_bibliography_entry(HTML_Writer& out, const Document_Info& info, Context& context)
{
    const auto open_link_tag = [&](std::u8string_view url, bool link_class = false) {
        out.write_inner_html(u8"<a href=\"");
        url_encode_to_writer(out, url, context, is_url_always_encoded_lambda);
        out.write_inner_html(u8'"');
        if (link_class) {
            out.write_inner_html(u8" class=bib-link");
        }
        out.write_inner_html(u8'>');
    };

    const std::u8string_view id_parts[] { bib_item_id_prefix, info.id };

    out.open_tag_with_attributes(u8"div") //
        .write_attribute(u8"id", id_parts)
        .write_class(u8"bib-item")
        .end();
    out.write_inner_html(u8'\n');

    if (!info.link.empty()) {
        open_link_tag(info.link);
    }

    COWEL_ASSERT(!info.id.empty());
    out.write_inner_html(u8'[');
    out.write_inner_text(info.id);
    out.write_inner_html(u8']');

    if (!info.link.empty()) {
        out.write_inner_html(u8"</a>");
    }

    if (!info.author.empty()) {
        out.write_inner_html(u8'\n');
        out.open_tag_with_attributes(u8"span") //
            .write_class(u8"bib-author")
            .end();
        out.write_inner_text(info.author);
        out.write_inner_html(u8'.');
        out.close_tag(u8"span");
    }
    if (!info.title.empty()) {
        out.write_inner_html(u8'\n');
        out.open_tag_with_attributes(u8"span") //
            .write_class(u8"bib-title")
            .end();
        out.write_inner_text(info.title);
        out.close_tag(u8"span");
    }
    if (!info.date.empty()) {
        out.write_inner_html(u8"\n");
        out.open_tag_with_attributes(u8"span") //
            .write_class(u8"bib-date")
            .end();
        out.write_inner_text(info.date);
        out.close_tag(u8"span");
    }

    const auto write_link = [&](std::u8string_view url) {
        out.write_inner_html(u8'\n');
        open_link_tag(url, true);
        out.write_inner_text(url);
        out.write_inner_html(u8"</a>");
    };

    if (!info.long_link.empty()) {
        write_link(info.long_link);
    }
    else if (!info.link.empty()) {
        write_link(info.link);
    }

    out.close_tag(u8"div");
}

} // namespace

Content_Status
Bibliography_Add_Behavior::operator()(Content_Policy&, const ast::Directive& d, Context& context)
    const
{
    static constexpr struct Entry {
        std::u8string_view parameter;
        std::u8string_view Document_Info::* member;
    } table[] {
        { u8"id", &Document_Info::id },
        { u8"title", &Document_Info::title },
        { u8"date", &Document_Info::date },
        { u8"publisher", &Document_Info::publisher },
        { u8"link", &Document_Info::link },
        { u8"long-link", &Document_Info::long_link },
        { u8"issue-link", &Document_Info::issue_link },
        { u8"author", &Document_Info::author },
    };

    // clang-format off
        static constexpr auto parameters = []()  {
            std::array<std::u8string_view, std::size(table)> result;
            std::ranges::transform(table, result.begin(), &Entry::parameter);
            return result;
        }();
    // clang-format on
    Argument_Matcher args { parameters, context.get_transient_memory() };
    args.match(d.get_arguments());

    Stored_Document_Info result { .text
                                  = std::pmr::vector<char8_t> { context.get_transient_memory() },
                                  .info {} };

    if (args.get_argument_index(u8"id") < 0) {
        context.try_error(
            diagnostic::bib::id_missing, d.get_source_span(),
            u8"An id argument is required to add a bibliography entry."sv
        );
        return Content_Status::error;
    }

    // The following process of converting directive arguments
    // to members in the Document_Info object needs to be done in two passes
    // because vector reallocation would invalidate string views.
    //  1. Concatenate the text data and remember the string lengths.
    //  2. Form string views and assign to members in the Document_Info.

    std::pmr::vector<std::size_t> string_lengths { context.get_transient_memory() };
    string_lengths.reserve(parameters.size());

    for (const auto& entry : table) {
        const int index = args.get_argument_index(entry.parameter);
        if (index < 0) {
            string_lengths.push_back(0);
            continue;
        }
        const ast::Argument& arg = d.get_arguments()[std::size_t(index)];
        const std::size_t initial_size = result.text.size();
        const auto status = to_plaintext(result.text, arg.get_content(), context);
        if (status != Content_Status::ok) {
            return status;
        }
        COWEL_ASSERT(result.text.size() >= initial_size);

        if (entry.parameter == u8"id" && result.text.size() == initial_size) {
            context.try_error(
                diagnostic::bib::id_empty, d.get_source_span(),
                u8"An id argument for a bibliography entry cannot be empty."sv
            );
            return Content_Status::error;
        }

        string_lengths.push_back(result.text.size() - initial_size);
    }
    {
        const auto result_string = as_u8string_view(result.text);
        std::size_t offset = 0;
        for (std::size_t i = 0; i < string_lengths.size(); ++i) {
            const auto length = string_lengths[i];
            const auto part_string = result_string.substr(offset, length);
            offset += length;
            result.info.*table[i].member = part_string;
        }
    }
    if (context.get_bibliography().contains(result.info.id)) {
        const std::u8string_view message[] {
            u8"A bibliography entry with id \"",
            result.info.id,
            u8"\" already exists.",
        };
        context.try_error(
            diagnostic::bib::duplicate, d.get_source_span(), joined_char_sequence(message)
        );
        return Content_Status::error;
    }

    // To facilitate later referencing,
    // we output the opening and closing HTML tags for this bibliography entry into sections.
    // If the bibliography entry has a link,
    // those tags will be "<a href=..." and "</a>",
    // otherwise the sections remain empty.
    {
        std::pmr::u8string section_name { u8"std.bib.", context.get_transient_memory() };
        section_name += result.info.id;
        const auto scope = context.get_sections().go_to_scoped(section_name);
        Content_Policy& section_out = context.get_sections().current_policy();
        HTML_Writer section_writer { section_out };

        if (!result.info.link.empty()) {
            // If the document info has a link,
            // then we want references to bibliography entries (e.g. "[N5008]")
            // to use that link.
            section_writer.write_inner_html(u8"<a href=\"");
            url_encode_to_writer(
                section_writer, result.info.link, context, is_url_always_encoded_lambda
            );
            section_writer.write_inner_html(u8"\">");
        }
        else {
            // Otherwise, the reference should redirect down
            // to the respective entry within the bibliography.
            // In any case, it's important to guarantee that an <a> element will be emitted.
            const std::u8string_view id_parts[] { u8"#", bib_item_id_prefix, result.info.id };
            section_writer
                .open_tag_with_attributes(u8"a") //
                .write_attribute(u8"id", id_parts)
                .end();
            section_writer.set_depth(0);
        }
        return Content_Status::ok;
    }

    {
        const auto scope = context.get_sections().go_to_scoped(section_name::bibliography);
        HTML_Writer bib_writer { context.get_sections().current_policy() };
        write_bibliography_entry(bib_writer, result.info, context);
    }

    context.get_bibliography().insert(std::move(result));
    return Content_Status::ok;
}

} // namespace cowel
