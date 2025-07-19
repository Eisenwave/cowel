#ifndef COWEL_DIRECTIVE_PROCESSING_HPP
#define COWEL_DIRECTIVE_PROCESSING_HPP

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

#include "cowel/directive_display.hpp"
#include "cowel/util/function_ref.hpp"
#include "cowel/util/html_writer.hpp"

#include "cowel/ast.hpp"
#include "cowel/fwd.hpp"

namespace cowel {

/// @brief Returns a string view to containing code units that `escape` corresponds to.
/// For most escape sequences, this returns `escape`.
/// For LF and CRLF escapes, this is an empty string view.
/// @param escape The escaped character(s), not including the initial `\`.
[[nodiscard]]
std::u8string_view expand_escape(std::u8string_view escape);

/// @brief Returns a string view to static storage corresponding to the code units
/// that `escape` corresponds to.
/// For most escape sequences, this is simply the character following the initial `\`.
/// For LF and CRLF escapes, this is an empty string.
[[nodiscard]]
inline std::u8string_view expand_escape(const ast::Escaped& escape)
{
    return expand_escape(escape.get_escaped());
}

[[nodiscard]]
std::span<const ast::Content> trim_blank_text_left(std::span<const ast::Content>);
[[nodiscard]]
std::span<const ast::Content> trim_blank_text_right(std::span<const ast::Content>);

/// @brief Trims leading and trailing completely blank text content.
[[nodiscard]]
std::span<const ast::Content> trim_blank_text(std::span<const ast::Content>);

enum struct To_Plaintext_Mode : Default_Underlying { //
    normal,
    no_side_effects,
    trimmed,
};

enum struct To_Plaintext_Status : Default_Underlying { //
    ok,
    some_ignored,
    error
};

[[nodiscard]]
Content_Status apply_behavior(Content_Policy& out, const ast::Directive& d, Context& context);

template <std::ranges::input_range R, typename Consumer>
    requires std::is_invocable_r_v<Content_Status, Consumer, std::ranges::range_reference_t<R>>
[[nodiscard]]
// NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
Content_Status process_greedy(R&& content, Consumer consumer)
{
    bool error = false;
    for (auto& c : content) {
        const auto status = consumer(c);
        switch (status) {
        case Content_Status::ok: continue;
        case Content_Status::abandon:
            return error ? Content_Status::error_abandon : Content_Status::abandon;
        case Content_Status::error: error = true; continue;
        case Content_Status::error_abandon:
        case Content_Status::fatal: return status;
        }
    }
    return error ? Content_Status::error : Content_Status::ok;
}

template <std::ranges::input_range R, typename Consumer>
    requires std::is_invocable_r_v<Content_Status, Consumer, std::ranges::range_reference_t<R>>
[[nodiscard]]
// NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
Content_Status process_lazy(R&& content, Consumer consumer)
{
    for (auto& c : content) {
        const auto status = consumer(c);
        if (status != Content_Status::ok) {
            return status;
        }
    }
    return Content_Status::ok;
}

[[nodiscard]]
inline Content_Status
consume_all(Content_Policy& out, std::span<const ast::Content> content, Context& context)
{
    return process_greedy(content, [&](const ast::Content& c) {
        return out.consume_content(c, context);
    });
}

[[nodiscard]]
Content_Status
consume_all_trimmed(Content_Policy& out, std::span<const ast::Content> content, Context& context);

[[nodiscard]]
Content_Status to_plaintext(
    std::pmr::vector<char8_t>& out,
    std::span<const ast::Content> content,
    Context& context
);

enum struct To_HTML_Mode : Default_Underlying {
    direct,
    paragraphs,
    trimmed,
    paragraphs_trimmed,
};

[[nodiscard]]
constexpr bool to_html_mode_is_trimmed(To_HTML_Mode mode)
{
    return mode == To_HTML_Mode::trimmed || mode == To_HTML_Mode::paragraphs_trimmed;
}

[[nodiscard]]
constexpr bool to_html_mode_is_paragraphed(To_HTML_Mode mode)
{
    return mode == To_HTML_Mode::paragraphs || mode == To_HTML_Mode::paragraphs_trimmed;
}

enum struct Paragraphs_State : bool {
    outside,
    inside,
};

enum struct Argument_Subset : Default_Underlying {
    none = 0,
    unmatched_positional = 1 << 0,
    matched_positional = 1 << 1,
    positional = unmatched_positional | matched_positional,
    unmatched_named = 1 << 2,
    unmatched = unmatched_positional | unmatched_named,
    matched_named = 1 << 3,
    matched = matched_positional | matched_named,
    named = unmatched_named | matched_named,
    all = unmatched | matched,
};

constexpr Argument_Subset operator|(Argument_Subset x, Argument_Subset y)
{
    return Argument_Subset(std::to_underlying(x) | std::to_underlying(y));
}

constexpr Argument_Subset operator&(Argument_Subset x, Argument_Subset y)
{
    return Argument_Subset(std::to_underlying(x) & std::to_underlying(y));
}

constexpr Argument_Subset argument_subset_matched_named(bool is_matched, bool is_named)
{
    return (is_matched ? Argument_Subset::matched : Argument_Subset::unmatched)
        & (is_named ? Argument_Subset::named : Argument_Subset::positional);
}

constexpr bool argument_subset_contains(Argument_Subset x, Argument_Subset y)
{
    return (x | y) == x;
}

constexpr bool argument_subset_intersects(Argument_Subset x, Argument_Subset y)
{
    return (x & y) != Argument_Subset::none;
}

/// @brief Emits a warning for all ignored arguments,
/// where the caller can specify what subset of arguments were ignored.
void warn_ignored_argument_subset(
    std::span<const ast::Argument> args,
    const Argument_Matcher& matcher,
    Context& context,
    Argument_Subset ignored_subset
);

/// @brief Emits a warning for all ignored arguments,
/// where the caller can specify what subset of arguments were ignored.
/// The given subset has to include either both `matched` and `unmatched`,
/// or be `none`.
void warn_ignored_argument_subset(
    std::span<const ast::Argument> args,
    Context& context,
    Argument_Subset ignored_subset
);

using Argument_Filter = Function_Ref<bool(std::size_t index, const ast::Argument& argument) const>;

[[nodiscard]]
Content_Status named_arguments_to_attributes(
    Attribute_Writer& out,
    const ast::Directive& d,
    Context& context,
    Argument_Filter filter = {},
    Attribute_Style style = Attribute_Style::double_if_needed
);

[[nodiscard]]
Content_Status named_arguments_to_attributes(
    Attribute_Writer& out,
    const ast::Directive& d,
    const Argument_Matcher& matcher,
    Context& context,
    Argument_Subset subset,
    Attribute_Style style = Attribute_Style::double_if_needed
);

[[nodiscard]]
Content_Status named_argument_to_attribute(
    Attribute_Writer& out,
    const ast::Argument& a,
    Context& context,
    Attribute_Style style = Attribute_Style::double_if_needed
);

/// @brief Similar to `Result`,
/// but does not behave like a union,
/// but rather always contains a value.
template <typename T>
struct Greedy_Result {
private:
    T m_value;
    Content_Status m_status = Content_Status::ok;

public:
    [[nodiscard]]
    Greedy_Result(const T& value, Content_Status status = Content_Status::ok)
        : m_value { value }
        , m_status { status }
    {
    }

    [[nodiscard]]
    Greedy_Result(T&& value, Content_Status status = Content_Status::ok)
        : m_value { std::move(value) }
        , m_status { status }
    {
    }

    [[nodiscard]]
    constexpr explicit operator bool() const
    {
        return m_status == Content_Status::ok;
    }

    [[nodiscard]]
    constexpr T& operator*()
    {
        COWEL_ASSERT(m_status == Content_Status::ok);
        return m_value;
    }
    [[nodiscard]]
    constexpr const T& operator*() const
    {
        COWEL_ASSERT(m_status == Content_Status::ok);
        return m_value;
    }

    [[nodiscard]]
    constexpr T* operator->()
    {
        COWEL_ASSERT(m_status == Content_Status::ok);
        return std::addressof(m_value);
    }
    [[nodiscard]]
    constexpr const T* operator->() const
    {
        COWEL_ASSERT(m_status == Content_Status::ok);
        return std::addressof(m_value);
    }

    [[nodiscard]]
    constexpr Content_Status status() const
    {
        return m_status;
    }
};

/// @brief Converts a the content of the argument matching the given parameter.
/// @param out The vector into which generated plaintext should be written.
/// @param d The directive.
/// @param args The arguments. Matching must have already taken place.
/// @param parameter The name of the parameter to which the argument belongs.
/// @param context The context.
[[nodiscard]]
Result<bool, Content_Status> argument_to_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Directive& d,
    const Argument_Matcher& args,
    std::u8string_view parameter,
    Context& context
);

[[nodiscard]]
Greedy_Result<bool> get_yes_no_argument(
    std::u8string_view name,
    std::u8string_view diagnostic_id,
    const ast::Directive& d,
    const Argument_Matcher& args,
    Context& context,
    bool fallback
);

[[nodiscard]]
Greedy_Result<std::size_t> get_integer_argument(
    std::u8string_view name,
    std::u8string_view parse_error_diagnostic,
    std::u8string_view range_error_diagnostic,
    const Argument_Matcher& args,
    const ast::Directive& d,
    Context& context,
    std::size_t fallback,
    std::size_t min = 0,
    std::size_t max = std::size_t(-1)
);

struct String_Argument {
    std::pmr::vector<char8_t> data;
    std::u8string_view string;
};

[[nodiscard]]
Greedy_Result<String_Argument> get_string_argument(
    std::u8string_view name,
    const ast::Directive& d,
    const Argument_Matcher& args,
    Context& context,
    std::u8string_view fallback = u8""
);

/// @brief Uses the error behavior provided by `context` to process `d`.
/// Returns `on_success` if that generation succeeded.
[[nodiscard]]
Content_Status try_generate_error(
    Content_Policy& out,
    const ast::Directive& d,
    Context& context,
    Content_Status on_success = Content_Status::error
);

/// @brief If `out` is a `Paragraph_Split_Content_Policy`
/// calls `out.enter_paragraph()`.
void try_enter_paragraph(Content_Policy& out);

/// @brief If `out` is a `Paragraph_Split_Content_Policy`
/// calls `out.leave_paragraph()`.
void try_leave_paragraph(Content_Policy& out);

/// @brief If `display` is `in_line`, calls `try_enter_paragraph(out)`.
/// If `display` is `block`, calls `try_leave_paragraph(out)`.
/// Otherwise, has no effect.
void ensure_paragraph_matches_display(Content_Policy& out, Directive_Display display);

} // namespace cowel

#endif
