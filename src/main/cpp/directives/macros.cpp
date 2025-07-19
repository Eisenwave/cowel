#include <string_view>
#include <vector>

#include "cowel/util/char_sequence.hpp"
#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/from_chars.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/to_chars.hpp"

#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/context.hpp"
#include "cowel/directive_arguments.hpp"
#include "cowel/directive_processing.hpp"

using namespace std::string_view_literals;
namespace cowel {

Content_Status
Macro_Define_Behavior::operator()(Content_Policy&, const ast::Directive& d, Context& context) const
{
    static const std::u8string_view parameters[] { u8"pattern" };
    Argument_Matcher args { parameters, context.get_transient_memory() };
    args.match(d.get_arguments());

    const int pattern_index = args.get_argument_index(u8"pattern");
    if (pattern_index < 0) {
        context.try_error(
            diagnostic::macro::no_pattern, d.get_source_span(),
            u8"A directive pattern must be provided when defining a macro."sv
        );
        return Content_Status::error;
    }
    const ast::Argument& pattern_arg = d.get_arguments()[std::size_t(pattern_index)];
    if (pattern_arg.get_content().size() != 1
        || !std::holds_alternative<ast::Directive>(pattern_arg.get_content()[0])) {
        context.try_error(
            diagnostic::macro::pattern_no_directive, pattern_arg.get_source_span(),
            u8"The pattern in a macro definition has to be a single directive, nothing else."sv
        );
        return Content_Status::error;
    }

    const auto& pattern_directive = std::get<ast::Directive>(pattern_arg.get_content()[0]);
    // The pattern arguments and content currently have no special meaning.
    // They are merely used as documentation by the user, but are never processed.
    // We are only interested in the pattern name at the point of definition.
    const std::u8string_view pattern_name = pattern_directive.get_name();
    std::pmr::u8string owned_name { pattern_name, context.get_transient_memory() };

    const bool success = context.emplace_macro(std::move(owned_name), auto(d));
    if (!success) {
        const std::u8string_view message[] {
            u8"Redefinition of macro \"",
            pattern_name,
            u8"\".",
        };
        context.try_soft_warning(
            diagnostic::macro::redefinition, d.get_source_span(), joined_char_sequence(message)
        );
    }
    return Content_Status::ok;
}

namespace {

enum struct Put_Response : bool {
    normal,
    abort,
};

[[nodiscard]]
Content_Status substitute_in_macro(
    std::pmr::vector<ast::Content>& content,
    const std::span<const ast::Argument> provided_arguments,
    const std::span<const ast::Content> provided_content,
    Context& context,
    const Function_Ref<Put_Response(const File_Source_Span&)> on_variadic_put
)
{
    static constexpr std::u8string_view put_parameters[] { u8"else" };

    for (auto it = content.begin(); it != content.end();) {
        auto* const d = std::get_if<ast::Directive>(&*it);
        if (!d) {
            // Anything other than directives (text, etc.) are unaffected by macro substitution.
            ++it;
            continue;
        }

        // Before anything else, we have to replace the contents and the arguments of directives.
        // This comes even before the use evaluation of \put and \arg
        // in order to facilitate nesting, like \arg[\arg[0]].
        std::pmr::vector<ast::Argument>& d_arguments = d->get_arguments();
        for (auto arg_it = d_arguments.begin(); arg_it != d_arguments.end();) {
            std::pmr::vector<ast::Content>& arg_content = arg_it->get_content();
            // Regular case where we just have some content in directive arguments that we
            // run substitution on, recursively.
            if (arg_content.size() != 1
                || !std::holds_alternative<ast::Directive>(arg_content.front())) {
                const auto status = substitute_in_macro(
                    arg_content, provided_arguments, provided_content, context, on_variadic_put
                );
                if (status_is_break(status)) {
                    return status;
                }
                ++arg_it;
                continue;
            }
            // Special case where we have a single directive argument.
            // Within that context, \put{...} is treated specially and can be used as
            // a variadic expansion of the provided arguments.
            bool variadically_expanded = false;
            auto variadic_put_expand = [&](const File_Source_Span&) {
                // This just gets rid of the \put{...} argument,
                // to be replaced with expanded arguments.
                arg_it = d_arguments.erase(arg_it);
                arg_it = d_arguments.insert(
                    arg_it, provided_arguments.begin(), provided_arguments.end()
                );
                arg_it += std::ptrdiff_t(provided_arguments.size());
                variadically_expanded = true;
                return Put_Response::abort;
            };
            const auto status = substitute_in_macro(
                arg_content, provided_arguments, provided_content, context, variadic_put_expand
            );
            if (status_is_break(status)) {
                return status;
            }
            if (!variadically_expanded) {
                ++arg_it;
            }
        }

        const auto status0 = substitute_in_macro(
            d->get_content(), provided_arguments, provided_content, context, on_variadic_put
        );
        if (status_is_break(status0)) {
            return status0;
        }

        if (d->get_name() != u8"put") {
            ++it;
            continue;
        }
        Argument_Matcher put_args { put_parameters, context.get_transient_memory() };
        put_args.match(d->get_arguments());
        warn_ignored_argument_subset(
            d->get_arguments(), put_args, context, Argument_Subset::unmatched
        );

        std::pmr::vector<char8_t> selection { context.get_transient_memory() };
        const auto status1 = to_plaintext(selection, d->get_content(), context);
        if (status_is_break(status1)) {
            return status1;
        }

        const auto selection_string = trim_ascii_blank(as_u8string_view(selection));

        const auto substitute_arg = [&](const ast::Argument& arg) {
            const std::span<const ast::Content> arg_content = arg.get_content();
            it = content.insert(it, arg_content.begin(), arg_content.end());
            it += std::ptrdiff_t(arg_content.size());
        };

        // Simple case like \put where we expand the given contents.
        if (selection.empty()) {
            it = content.erase(it);
            it = content.insert(it, provided_content.begin(), provided_content.end());
            // We must skip over substituted content,
            // otherwise we risk expanding a \put directive that was passed to the macro,
            // rather than being in the macro definition,
            // and \put is only supposed to have special meaning within the macro definition.
            it += std::ptrdiff_t(provided_content.size());
            continue;
        }

        // Variadic \put{...} case.
        // Handling depends on the context.
        if (selection_string == u8"...") {
            // important: erasing kills d, so we need to copy its location beforehand
            const File_Source_Span location = d->get_source_span();
            it = content.erase(it);
            if (on_variadic_put(location) == Put_Response::abort) {
                return Content_Status::ok;
            }
            continue;
        }

        // Index case like \put{0} for selecting a given argument,
        // possibly with a fallback like \put[else=abc]{0}.
        const std::optional<std::size_t> arg_index = from_chars<std::size_t>(selection_string);
        if (!arg_index) {
            context.try_error(
                diagnostic::macro::put_invalid, d->get_source_span(),
                u8"The argument to this \\put pseudo-directive is invalid."sv
            );
            it = content.erase(it);
            continue;
        }
        if (*arg_index >= provided_arguments.size()) {
            const int else_index = put_args.get_argument_index(u8"else");
            if (else_index < 0) {
                const Characters8 limit_chars = to_characters8(provided_arguments.size());
                const std::u8string_view message[] {
                    u8"This \\put directive is invalid because the positional argument at ",
                    u8"index [",
                    selection_string,
                    u8"] was requested, but only ",
                    limit_chars.as_string(),
                    u8" were provided. ",
                    u8"To make this valid, provide an \"else\" argument, like ",
                    u8"\\put[else=xyz]{0}."
                };
                context.try_error(
                    diagnostic::macro::put_out_of_range, d->get_source_span(),
                    joined_char_sequence(message)
                );
                it = content.erase(it);
                continue;
            }
            // It is very important that we do it in this order because erase(it)
            // also kills d.
            ast::Argument else_arg = std::move(d->get_arguments()[std::size_t(else_index)]);
            it = content.erase(it);
            substitute_arg(else_arg);
            continue;
        }
        it = content.erase(it);
        substitute_arg(provided_arguments[*arg_index]);
    }
    return Content_Status::ok;
}

[[nodiscard]]
Content_Status instantiate_macro(
    std::pmr::vector<ast::Content>& out,
    const ast::Directive& definition,
    std::span<const ast::Argument> put_arguments,
    std::span<const ast::Content> put_content,
    Context& context
)
{
    const std::span<const ast::Content> content = definition.get_content();
    out.insert(out.end(), content.begin(), content.end());

    auto on_variadic_put = [&](const File_Source_Span& location) {
        context.try_error(
            diagnostic::macro::put_args_outside_args, location,
            u8"A \\put[...] pseudo-directive can only be used as the sole positional argument "
            u8"in a directive."sv
        );
        return Put_Response::normal;
    };
    return substitute_in_macro(out, put_arguments, put_content, context, on_variadic_put);
}

} // namespace

Content_Status Macro_Instantiate_Behavior::operator()(
    Content_Policy& out,
    const ast::Directive& d,
    Context& context
) const
{
    const ast::Directive* const definition = context.find_macro(d.get_name());
    // We always find a macro
    // because the name lookup for this directive utilizes `find_macro`,
    // so we're effectively calling it twice with the same input.
    COWEL_ASSERT(definition);

    std::pmr::vector<ast::Content> instance { context.get_transient_memory() };
    const auto status
        = instantiate_macro(instance, *definition, d.get_arguments(), d.get_content(), context);
    if (status_is_break(status)) {
        return status;
    }
    return consume_all(out, instance, context);
}

} // namespace cowel
