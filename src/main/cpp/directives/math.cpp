#include <algorithm>
#include <bitset>
#include <string_view>

#include "cowel/builtin_directive_set.hpp"
#include "cowel/directive_processing.hpp"

using namespace std::string_view_literals;

namespace cowel {
namespace {

// clang-format off
#define COWEL_MATHML_ELEMENT_DATA(F)                                                               \
    F(annotation, 1)                                                                               \
    F(annotation-xml, 0)                                                                         \
    F(maction, 0)                                                                                  \
    F(menclose, 0)                                                                                 \
    F(merror, 0)                                                                                   \
    F(mfenced, 0)                                                                                  \
    F(mfrac, 0)                                                                                    \
    F(mi, 1)                                                                                       \
    F(mmultiscripts, 0)                                                                            \
    F(mn, 1)                                                                                       \
    F(mo, 1)                                                                                       \
    F(mover, 0)                                                                                    \
    F(mpadded, 0)                                                                                  \
    F(mphantom, 0)                                                                                 \
    F(mprescripts, 0)                                                                              \
    F(mroot, 0)                                                                                    \
    F(mrow, 0)                                                                                     \
    F(ms, 1)                                                                                       \
    F(mspace, 0)                                                                                   \
    F(msqrt, 0)                                                                                    \
    F(mstyle, 0)                                                                                   \
    F(msub, 0)                                                                                     \
    F(msubsup, 0)                                                                                  \
    F(msup, 0)                                                                                     \
    F(mtable, 0)                                                                                   \
    F(mtd, 0)                                                                                      \
    F(mtext, 1)                                                                                    \
    F(mtr, 0)                                                                                      \
    F(munder, 0)                                                                                   \
    F(munderover, 0)                                                                               \
    F(semantics, 0)
// clang-format on

#define COWEL_MATHML_ELEMENT_NAME(name, permits_text) u8## #name,
#define COWEL_MATHML_ELEMENT_PERMITS_TEXT(name, permits_text) #permits_text[0],

constexpr std::u8string_view mathml_names[] = { //
    COWEL_MATHML_ELEMENT_DATA(COWEL_MATHML_ELEMENT_NAME)
};

static_assert(std::ranges::is_sorted(mathml_names));

constexpr auto mathml_permits_text_bits = [] { //
    char init[] { 0, COWEL_MATHML_ELEMENT_DATA(COWEL_MATHML_ELEMENT_PERMITS_TEXT) };
    std::ranges::reverse(init);
    // Use of std::string is workaround for libc++ bug:
    // https://github.com/llvm/llvm-project/issues/143684
    return std::bitset<std::size(init)>(init);
}();

constexpr std::ptrdiff_t mathml_element_index(std::u8string_view name)
{
    const auto* const pos = std::ranges::lower_bound(mathml_names, name);
    if (pos == std::end(mathml_names) || *pos != name) {
        return -1;
    }
    return pos - mathml_names;
}

static_assert(mathml_permits_text_bits[mathml_element_index(u8"mi")]);
static_assert(!mathml_permits_text_bits[mathml_element_index(u8"munderover")]);

[[nodiscard]]
Content_Status to_math_html(
    Content_Policy& out,
    std::span<const ast::Content> contents,
    Context& context,
    bool permit_text = false
)
{
    return process_greedy(contents, [&](const ast::Content& c) -> Content_Status {
        const auto* const d = std::get_if<ast::Directive>(&c);
        if (!d) {
            if (!permit_text) {
                const auto* const t = std::get_if<ast::Text>(&c);
                const bool is_blank_text = std::ranges::all_of(t->get_source(), [](char8_t c) {
                    return is_ascii_blank(c);
                });
                if (!is_blank_text) {
                    context.try_warning(
                        diagnostic::math::text, ast::get_source_span(c),
                        u8"Text cannot appear in this context. "
                        u8"MathML requires text to be enclosed in <mi>, <mn>, etc., "
                        u8"which correspond to \\mi, \\mn, and other pseudo-directives."sv
                    );
                }
            }
            return out.consume_content(c, context);
        }
        const std::u8string_view name = d->get_name();
        const std::ptrdiff_t index = mathml_element_index(name);
        if (index < 0) {
            return out.consume(*d, context);
        }
        warn_ignored_argument_subset(d->get_arguments(), context, Argument_Subset::positional);

        HTML_Writer writer { out };
        Attribute_Writer attributes = writer.open_tag_with_attributes(name);
        const auto attributes_status = named_arguments_to_attributes(attributes, *d, context);
        attributes.end();
        if (status_is_break(attributes_status)) {
            writer.close_tag(name);
            return attributes_status;
        }

        const bool child_permits_text = mathml_permits_text_bits[std::size_t(index)];
        const auto nested_status = to_math_html(out, d->get_content(), context, child_permits_text);
        writer.close_tag(name);
        return status_concat(attributes_status, nested_status);
    });
}

} // namespace

Content_Status
Math_Behavior::operator()(Content_Policy& out, const ast::Directive& d, Context& context) const
{
    constexpr std::u8string_view tag_name = u8"math";
    const std::u8string_view display_string
        = m_display == Directive_Display::in_line ? u8"inline" : u8"block";

    ensure_paragraph_matches_display(out, m_display);

    HTML_Content_Policy policy { out };
    HTML_Writer writer { policy };
    Attribute_Writer attributes = writer.open_tag_with_attributes(tag_name);
    attributes.write_attribute(u8"display", display_string);
    const auto attributes_status = named_arguments_to_attributes(attributes, d, context);
    attributes.end();
    warn_ignored_argument_subset(d.get_arguments(), context, Argument_Subset::positional);
    if (status_is_break(attributes_status)) {
        writer.close_tag(tag_name);
        return attributes_status;
    }

    const auto nested_status = to_math_html(policy, d.get_content(), context);
    writer.close_tag(tag_name);
    return status_concat(attributes_status, nested_status);
}

} // namespace cowel
