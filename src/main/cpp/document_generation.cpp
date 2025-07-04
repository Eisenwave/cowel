#include <memory_resource>
#include <unordered_set>

#include "cowel/theme_to_css.hpp"
#include "cowel/util/assert.hpp"
#include "cowel/util/html_writer.hpp"

#include "cowel/assets.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_behavior.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/document_content_behavior.hpp"
#include "cowel/document_generation.hpp"
#include "cowel/document_sections.hpp"

namespace cowel {

void generate_document(const Generation_Options& options)
{
    COWEL_ASSERT(options.memory != nullptr);

    std::pmr::unsynchronized_pool_resource transient_memory { options.memory };

    HTML_Writer writer { options.output };

    Context context { options.highlight_theme_source, //
                      options.error_behavior, //
                      options.file_loader,
                      options.logger, //
                      options.highlighter, //
                      options.bibliography, //
                      options.memory, //
                      &transient_memory };
    context.add_resolver(options.builtin_behavior);

    options.root_behavior.generate_html(writer, options.root_content, context);

    context.get_bibliography().clear();
}

namespace {

// See also `reference_section`.
// The point of all this is that references to other sections are encoded using a section name,
// which is encoded using a Supplementary Private Use Area A code point
// storing the section name length, followed by the section name code units.

constexpr char8_t supplementary_pua_a_first_code_unit
    = utf8::encode8_unchecked(supplementary_pua_a_min).code_units[0];
constexpr char8_t supplementary_pua_a_first_code_unit_mask = 0b1111'1000;
constexpr char8_t supplementary_pua_a_first_code_unit_masked
    = supplementary_pua_a_first_code_unit & supplementary_pua_a_first_code_unit_mask;

static_assert(supplementary_pua_a_first_code_unit_masked == 0b1111'0000);

struct Reference_Resolver {
    std::pmr::vector<char8_t>& out;
    std::pmr::unordered_set<const void*>& visited;
    Context& context;

    bool operator()(std::u8string_view text, File_Id file);
};

bool Reference_Resolver::operator()(std::u8string_view text, File_Id file)
{
    bool success = true;

    std::size_t plain_length = 0;
    const auto flush = [&] {
        if (plain_length != 0) {
            out.insert(out.end(), text.data(), text.data() + plain_length);
            text.remove_prefix(plain_length);
            plain_length = 0;
        }
    };
    while (plain_length < text.length()) {
        // Obviously, almost none of the text in the generated document is a section reference,
        // so we perform this early test,
        // which tells us that it couldn't be a PUA A character based on the leading code unit.
        if ((text[plain_length] & supplementary_pua_a_first_code_unit_mask)
            != supplementary_pua_a_first_code_unit_masked) {
            ++plain_length;
            continue;
        }
        const auto [code_point, code_units]
            = utf8::decode_and_length_or_replacement(text.substr(plain_length));
        // This test passes for any code point that is encoded as four UTF-8 code units,
        // so we still need to check if the code point is PUA A.
        if (code_point < supplementary_pua_a_min || code_point > supplementary_pua_a_max) {
            ++plain_length;
            continue;
        }
        COWEL_DEBUG_ASSERT(code_units == 4);
        flush();
        const auto reference_length = std::size_t(code_point - supplementary_pua_a_min);
        COWEL_ASSERT(4 + reference_length <= text.length());
        const std::u8string_view section_name = text.substr(4, reference_length);

        bool section_success = true;
        const Document_Sections::entry_type* const entry
            = context.get_sections().find(section_name);
        if (!entry) {
            const std::u8string_view message[] {
                u8"Invalid reference to section \"",
                section_name,
                u8"\".",
            };
            context.try_error(diagnostic::section_ref_not_found, { {}, file }, message);
            section_success = false;
        }
        else if (const auto [_, insert_success] = visited.insert(entry); !insert_success) {
            const std::u8string_view message[] {
                u8"Circular dependency in reference to section \"",
                section_name,
                u8"\".",
            };
            context.try_error(diagnostic::section_ref_circular, { {}, file }, message);
            section_success = false;
        }
        text.remove_prefix(4 + reference_length);
        if (section_success) {
            const std::u8string_view referenced_text { entry->second.data(), entry->second.size() };
            (*this)(referenced_text, file);
        }
        else {
            success = section_success;
        }
        visited.erase(entry);
    }
    flush();
    return success;
}

[[maybe_unused]]
void warn_deprecated_directive_names(std::span<const ast::Content> content, Context& context)
{
    for (const ast::Content& c : content) {
        if (const auto* const d = std::get_if<ast::Directive>(&c)) {
            if (d->get_name().contains(u8'-')) {
                context.try_warning(
                    diagnostic::deprecated, d->get_name_span(),
                    u8"The use of '-' in directive names is deprecated."
                );
            }
            for (const ast::Argument& arg : d->get_arguments()) {
                warn_deprecated_directive_names(arg.get_content(), context);
            }
            warn_deprecated_directive_names(d->get_content(), context);
        }
    }
}

constexpr bool enable_warn_deprecated_directive_names = false;

} // namespace

void Head_Body_Content_Behavior::generate_html(
    HTML_Writer& out,
    std::span<const ast::Content> content,
    Context& context
) const
{
    // TODO: Enable once it's no longer necessary to have hyphens in directive names.
    //       This will require changing directives like \html-X or \wg21-link.
    if constexpr (enable_warn_deprecated_directive_names) {
        warn_deprecated_directive_names(content, context);
    }

    Document_Sections& sections = context.get_sections();

    const Document_Sections::entry_type* html_section = nullptr;
    auto& html_text = [&] -> std::pmr::vector<char8_t>& {
        const auto scope = sections.go_to_scoped(section_name::document_html);

        HTML_Writer current_out = sections.current_html();
        const auto open_and_close = [&](std::u8string_view tag, auto f) {
            current_out.open_tag(tag);
            current_out.write_inner_html(u8'\n');
            f();
            current_out.close_tag(tag);
            current_out.write_inner_html(u8'\n');
        };

        current_out.write_preamble();
        open_and_close(u8"html", [&] {
            open_and_close(u8"head", [&] {
                reference_section(current_out, section_name::document_head);
            });
            open_and_close(u8"body", [&] {
                reference_section(current_out, section_name::document_body);
            });
        });

        html_section = &sections.current();
        return sections.current_text();
    }();
    const std::u8string_view html_string { html_text.data(), html_text.size() };

    {
        const auto scope = sections.go_to_scoped(section_name::document_head);
        HTML_Writer current_out = sections.current_html();
        generate_head(current_out, content, context);
    }
    {
        const auto scope = sections.go_to_scoped(section_name::document_body);
        HTML_Writer current_out = sections.current_html();
        generate_body(current_out, content, context);
    }

    std::pmr::unordered_set<const void*> visited(context.get_transient_memory());
    visited.insert(html_section);

    const File_Id file = content.empty() ? File_Id {} : ast::get_source_span(content.front()).file;
    Reference_Resolver { out.get_output(), visited, context }(html_string, file);
}

constexpr std::u8string_view indent = u8"  ";
constexpr std::u8string_view newline_indent = u8"\n  ";

void Document_Content_Behavior::generate_html(
    HTML_Writer& out,
    std::span<const ast::Content> content,
    Context& context
) const
{
    context.add_resolver(m_macro_resolver);

    Head_Body_Content_Behavior::generate_html(out, content, context);
}

void Document_Content_Behavior::generate_head(
    HTML_Writer& out,
    std::span<const ast::Content>,
    Context& context
) const
{
    constexpr std::u8string_view google_fonts_url
        = u8"https://fonts.googleapis.com/css2"
          u8"?family=Fira+Code:wght@300..700"
          u8"&family=Noto+Sans:ital,wght@0,100..900;1,100..900"
          u8"&family=Noto+Serif:ital,wght@0,100..900;1,100..900"
          u8"&display=swap";

    out.write_inner_html(indent);

    out.open_tag_with_attributes(u8"meta") //
        .write_charset(u8"UTF-8")
        .end_empty();
    out.open_tag_with_attributes(u8"meta") //
        .write_name(u8"viewport")
        .write_content(u8"width=device-width, initial-scale=1")
        .end_empty();

    out.open_tag_with_attributes(u8"link") //
        .write_rel(u8"preconnent")
        .write_href(u8"https://fonts.googleapis.com")
        .end_empty();
    out.write_inner_html(newline_indent);
    out.open_tag_with_attributes(u8"link") //
        .write_rel(u8"preconnent")
        .write_href(u8"https://fonts.gstatic.com")
        .write_crossorigin()
        .end_empty();
    out.write_inner_html(newline_indent);
    out.open_tag_with_attributes(u8"link") //
        .write_rel(u8"stylesheet")
        .write_href(google_fonts_url)
        .end_empty();

    const auto include_css_or_js = [&](std::u8string_view tag, std::u8string_view source) {
        out.write_inner_html(newline_indent);
        out.open_tag(tag);
        out.write_inner_html(u8'\n');
        out.write_inner_html(source);
        out.write_inner_html(indent);
        out.close_tag(tag);
    };
    include_css_or_js(u8"style", assets::main_css);
    {
        out.write_inner_text(newline_indent);
        out.open_tag(u8"style");
        out.write_inner_html(u8'\n');
        const std::u8string_view theme_json = context.get_highlight_theme_source();
        if (!theme_to_css(out.get_output(), theme_json, context.get_transient_memory())) {
            context.try_error(
                diagnostic::theme_conversion, { {}, File_Id {} },
                u8"Failed to convert the syntax highlight theme to CSS, "
                u8"possibly because the JSON was malformed."
            );
        }
        out.write_inner_html(indent);
        out.close_tag(u8"style");
    }

    include_css_or_js(u8"script", assets::light_dark_js);
    out.write_inner_html(u8'\n');
}

void Document_Content_Behavior::generate_body(
    HTML_Writer& out,
    std::span<const ast::Content> content,
    Context& context
) const
{
    out.write_inner_html(assets::settings_widget_html);
    out.open_tag(u8"main");
    out.write_inner_html(u8'\n');
    to_html(out, content, context, To_HTML_Mode::paragraphs);
    out.close_tag(u8"main");
}

} // namespace cowel
