#include <cstddef>
#include <cstring>
#include <ranges>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

#include "cowel/policy/paragraph_split.hpp"
#include "cowel/policy/plaintext.hpp"

#include "cowel/util/assert.hpp"
#include "cowel/util/from_chars.hpp"
#include "cowel/util/html_writer.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/to_chars.hpp"

#include "cowel/ast.hpp"
#include "cowel/context.hpp"
#include "cowel/directive_arguments.hpp"
#include "cowel/directive_behavior.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/fwd.hpp"

using namespace std::string_view_literals;

namespace cowel {

std::u8string_view expand_escape(std::u8string_view escape)
{
    COWEL_ASSERT(!escape.empty());
    COWEL_DEBUG_ASSERT(is_cowel_escapeable(escape[0]));
    return escape[0] == u8'\r' || escape[0] == u8'\n' ? u8"" : escape;
}

Directive_Behavior* Context::find_directive(std::u8string_view name)
{
    for (const Name_Resolver* const resolver : std::views::reverse(m_name_resolvers)) {
        if (Directive_Behavior* const result = (*resolver)(name, *this)) {
            return result;
        }
    }
    return nullptr;
}

Directive_Behavior* Context::find_directive(const ast::Directive& directive)
{
    return find_directive(directive.get_name());
}

std::span<const ast::Content> trim_blank_text_left(std::span<const ast::Content> content)
{
    while (!content.empty()) {
        if (const auto* const text = std::get_if<ast::Text>(&content.front())) {
            if (is_ascii_blank(text->get_source())) {
                content = content.subspan(1);
                continue;
            }
        }
        if (const auto* const text = std::get_if<ast::Generated>(&content.front())) {
            if (is_ascii_blank(text->as_string())) {
                content = content.subspan(1);
                continue;
            }
        }
        break;
    }
    return content;
}

std::span<const ast::Content> trim_blank_text_right(std::span<const ast::Content> content)
{
    while (!content.empty()) {
        if (const auto* const text = std::get_if<ast::Text>(&content.back())) {
            if (is_ascii_blank(text->get_source())) {
                content = content.subspan(0, content.size() - 1);
                continue;
            }
        }
        if (const auto* const text = std::get_if<ast::Generated>(&content.back())) {
            if (is_ascii_blank(text->as_string())) {
                content = content.subspan(0, content.size() - 1);
                continue;
            }
        }
        break;
    }
    return content;
}

std::span<const ast::Content> trim_blank_text(std::span<const ast::Content> content)
{
    return trim_blank_text_right(trim_blank_text_left(content));
}

namespace {

void try_lookup_error(const ast::Directive& directive, Context& context)
{
    if (!context.emits(Severity::error)) {
        return;
    }

    const std::u8string_view message[] {
        u8"No directive with the name \"",
        directive.get_name(),
        u8"\" exists.",
    };
    context.try_error(
        diagnostic::directive_lookup_unresolved, directive.get_name_span(),
        joined_char_sequence(message)
    );
}

} // namespace

Content_Status
consume_all_trimmed(Content_Policy& out, std::span<const ast::Content> content, Context& context)
{
    content = trim_blank_text(content);

    struct Visitor {
        Content_Policy& out;
        Context& context;
        std::size_t i;
        std::size_t size;

        void write_trimmed(std::u8string_view str) const
        {
            // Note that the following two conditions are not mutually exclusive
            // when content contains just one element.
            if (i == 0) {
                str = trim_ascii_blank_left(str);
            }
            if (i + 1 == size) {
                str = trim_ascii_blank_right(str);
            }
            // The trimming above should have gotten rid of entirely empty strings.
            COWEL_ASSERT(!str.empty());
            out.write(str, Output_Language::text);
        }

        [[nodiscard]]
        Content_Status operator()(const ast::Text& text) const
        {
            write_trimmed(text.get_source());
            return Content_Status::ok;
        }

        [[nodiscard]]
        Content_Status operator()(const ast::Comment& c) const
        {
            return out.consume(c, context);
        }

        [[nodiscard]]
        Content_Status operator()(const ast::Generated& g) const
        {
            if (g.get_type() == Output_Language::text) {
                write_trimmed(g.as_string());
                return Content_Status::ok;
            }
            return out.consume(g, context);
        }

        [[nodiscard]]
        Content_Status operator()(const ast::Escaped& e) const
        {
            return out.consume(e, context);
        }

        [[nodiscard]]
        Content_Status operator()(const ast::Directive& e) const
        {
            return out.consume(e, context);
        }
    };

    return process_greedy(content, [&, i = 0uz](const ast::Content& c) mutable -> Content_Status {
        const auto result = std::visit(Visitor { out, context, i, content.size() }, c);
        ++i;
        return result;
    });
}

Content_Status to_plaintext(
    std::pmr::vector<char8_t>& out,
    std::span<const ast::Content> content,
    Context& context
)
{
    Capturing_Ref_Text_Sink sink { out, Output_Language::text };
    Plaintext_Content_Policy policy { sink };
    return consume_all(policy, content, context);
}

Content_Status apply_behavior(Content_Policy& out, const ast::Directive& d, Context& context)
{
    Directive_Behavior* const behavior = context.find_directive(d);
    if (!behavior) {
        try_lookup_error(d, context);
        if (const Content_Status s = try_generate_error(out, d, context); s != Content_Status::ok) {
            // TODO: error message
            return Content_Status::fatal;
        }
        return Content_Status::error;
    }
    return (*behavior)(out, d, context);
}

void warn_ignored_argument_subset(
    std::span<const ast::Argument> args,
    const Argument_Matcher& matcher,
    Context& context,
    Argument_Subset ignored_subset
)
{
    const std::span<const Argument_Status> statuses = matcher.argument_statuses();
    COWEL_ASSERT(args.size() == statuses.size());

    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];
        const bool is_matched = statuses[i] != Argument_Status::unmatched;
        const bool is_named = arg.has_name();
        const Argument_Subset subset = argument_subset_matched_named(is_matched, is_named);
        if (argument_subset_contains(ignored_subset, subset)) {
            context.try_warning(
                diagnostic::ignored_args, arg.get_source_span(), u8"This argument was ignored."sv
            );
        }
    }
}

void warn_ignored_argument_subset(
    std::span<const ast::Argument> args,
    Context& context,
    Argument_Subset ignored_subset
)
{
    COWEL_ASSERT(
        argument_subset_contains(ignored_subset, Argument_Subset::matched)
        == argument_subset_contains(ignored_subset, Argument_Subset::unmatched)
    );

    for (const auto& arg : args) {
        const auto subset = arg.has_name() ? Argument_Subset::named : Argument_Subset::positional;
        if (argument_subset_contains(ignored_subset, subset)) {
            context.try_warning(
                diagnostic::ignored_args, arg.get_source_span(), u8"This argument was ignored."sv
            );
        }
    }
}

Content_Status named_arguments_to_attributes(
    Attribute_Writer& out,
    const ast::Directive& d,
    Context& context,
    Argument_Filter filter,
    Attribute_Style style
)
{
    const std::span<const ast::Argument> args = d.get_arguments();
    std::size_t i = 0;
    return process_greedy(args, [&](const ast::Argument& a) -> Content_Status {
        const bool passed_filter = a.has_name() && (!filter || filter(i, a));
        ++i;
        return passed_filter ? named_argument_to_attribute(out, a, context, style)
                             : Content_Status::ok;
    });
}

Content_Status named_arguments_to_attributes(
    Attribute_Writer& out,
    const ast::Directive& d,
    const Argument_Matcher& matcher,
    Context& context,
    Argument_Subset subset,
    Attribute_Style style
)
{
    COWEL_ASSERT(!argument_subset_intersects(subset, Argument_Subset::positional));

    const auto filter = [&](std::size_t index, const ast::Argument& a) -> bool {
        const Argument_Status status = matcher.argument_statuses()[index];
        if (status == Argument_Status::duplicate_named) {
            const std::u8string_view message[] = {
                u8"This argument is a duplicate of a previous named argument also named \"",
                a.get_name(),
                u8"\", and will be ignored.",
            };
            context.try_warning(
                diagnostic::duplicate_args, a.get_source_span(), joined_char_sequence(message)
            );
            return false;
        }
        const auto arg_subset = status == Argument_Status::unmatched
            ? Argument_Subset::unmatched_named
            : Argument_Subset::matched_named;
        return argument_subset_contains(subset, arg_subset);
    };
    return named_arguments_to_attributes(out, d, context, filter, style);
}

Content_Status named_argument_to_attribute(
    Attribute_Writer& out,
    const ast::Argument& a,
    Context& context,
    Attribute_Style style
)
{
    COWEL_ASSERT(a.has_name());
    std::pmr::vector<char8_t> value { context.get_transient_memory() };
    // TODO: error handling
    value.clear();
    const auto status = to_plaintext(value, a.get_content(), context);
    const std::u8string_view value_string { value.data(), value.size() };
    const std::u8string_view name = a.get_name();
    out.write_attribute(name, value_string, style);
    return status;
}

Result<bool, Content_Status> argument_to_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Directive& d,
    const Argument_Matcher& args,
    std::u8string_view parameter,
    Context& context
)
{
    const int i = args.get_argument_index(parameter);
    if (i < 0) {
        return false;
    }
    const ast::Argument& arg = d.get_arguments()[std::size_t(i)];
    // TODO: warn when pure HTML argument was used as variable name
    const auto status = to_plaintext(out, arg.get_content(), context);
    if (status != Content_Status::ok) {
        return status;
    }
    return true;
}

Greedy_Result<bool> get_yes_no_argument(
    std::u8string_view name,
    std::u8string_view diagnostic_id,
    const ast::Directive& d,
    const Argument_Matcher& args,
    Context& context,
    bool fallback
)
{
    const int index = args.get_argument_index(name);
    if (index < 0) {
        return fallback;
    }
    const ast::Argument& arg = d.get_arguments()[std::size_t(index)];
    std::pmr::vector<char8_t> data { context.get_transient_memory() };
    const auto text_status = to_plaintext(data, arg.get_content(), context);
    if (text_status != Content_Status::ok) {
        return { fallback, text_status };
    }

    const auto string = as_u8string_view(data);
    if (string == u8"yes") {
        return true;
    }
    if (string == u8"no") {
        return false;
    }
    const std::u8string_view message[] {
        u8"Argument has to be \"yes\" or \"no\", but \"",
        string,
        u8"\" was given.",
    };
    context.try_warning(diagnostic_id, arg.get_source_span(), joined_char_sequence(message));
    return fallback;
}

Greedy_Result<std::size_t> get_integer_argument(
    std::u8string_view name,
    std::u8string_view parse_error_diagnostic,
    std::u8string_view range_error_diagnostic,
    const Argument_Matcher& args,
    const ast::Directive& d,
    Context& context,
    std::size_t fallback,
    std::size_t min,
    std::size_t max
)
{
    COWEL_ASSERT(fallback >= min && fallback <= max);

    const int index = args.get_argument_index(name);
    if (index < 0) {
        return fallback;
    }
    const ast::Argument& arg = d.get_arguments()[std::size_t(index)];
    std::pmr::vector<char8_t> arg_text { context.get_transient_memory() };
    const auto text_status = to_plaintext(arg_text, arg.get_content(), context);
    if (text_status != Content_Status::ok) {
        return { fallback, text_status };
    }
    const auto arg_string = as_u8string_view(arg_text);

    const std::optional<std::size_t> value = from_chars<std::size_t>(arg_string);
    if (!value) {
        const std::u8string_view message[] {
            u8"The specified ",
            name,
            u8" \"",
            arg_string,
            u8"\" is ignored because it could not be parsed as a (positive) integer.",
        };
        context.try_warning(
            parse_error_diagnostic, arg.get_source_span(), joined_char_sequence(message)
        );
        return fallback;
    }
    if (value < min || value > max) {
        const Characters8 min_chars = to_characters8(min);
        const Characters8 max_chars = to_characters8(max);
        const std::u8string_view message[] {
            u8"The specified ",
            name,
            u8" \"",
            arg_string,
            u8"\" is ignored because it is outside of the valid range [",
            min_chars.as_string(),
            u8", ",
            max_chars.as_string(),
            u8"].",
        };
        context.try_warning(
            range_error_diagnostic, arg.get_source_span(), joined_char_sequence(message)
        );
        return fallback;
    }

    return *value;
}

Greedy_Result<String_Argument> get_string_argument(
    std::u8string_view name,
    const ast::Directive& d,
    const Argument_Matcher& args,
    Context& context,
    std::u8string_view fallback
)
{
    String_Argument result { .data = std::pmr::vector<char8_t>(context.get_transient_memory()),
                             .string = {} };
    const int index = args.get_argument_index(name);
    if (index < 0) {
        result.string = fallback;
        return result;
    }
    const auto status
        = to_plaintext(result.data, d.get_arguments()[std::size_t(index)].get_content(), context);
    if (status != Content_Status::ok) {
        result.string = fallback;
        return { result, status };
    }
    result.string = as_u8string_view(result.data);
    return result;
}

Content_Status try_generate_error(
    Content_Policy& out,
    const ast::Directive& d,
    Context& context,
    Content_Status on_success
)
{
    if (const Directive_Behavior* const behavior = context.get_error_behavior()) {
        const Content_Status result = (*behavior)(out, d, context);
        return result == Content_Status::ok ? on_success : result;
    }
    return on_success;
}

void try_enter_paragraph(Content_Policy& out)
{
    if (auto* const derived = dynamic_cast<Paragraph_Split_Policy*>(&out)) {
        derived->enter_paragraph();
    }
}

void try_leave_paragraph(Content_Policy& out)
{
    if (auto* const derived = dynamic_cast<Paragraph_Split_Policy*>(&out)) {
        derived->leave_paragraph();
    }
}

void ensure_paragraph_matches_display(Content_Policy& out, Directive_Display display)
{
    switch (display) {
    case Directive_Display::in_line: {
        try_enter_paragraph(out);
        break;
    }
    case Directive_Display::block: {
        try_leave_paragraph(out);
        break;
    }
    case Directive_Display::none: break;
    }
}

} // namespace cowel
