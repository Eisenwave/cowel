#ifndef COWEL_DIAGNOSTIC_HPP
#define COWEL_DIAGNOSTIC_HPP

#include <compare>
#include <string_view>

#include "cowel/util/char_sequence.hpp"
#include "cowel/util/source_position.hpp"

#include "cowel/fwd.hpp"

namespace cowel {
namespace detail {
using Suppress_Unused_Include_Source_Position = Basic_File_Source_Position<void>;
}

enum struct Severity : Default_Underlying {
    /// @brief Alias for `debug`.
    min = 0,
    /// @brief Debugging messages.
    /// Only emitted in debug mode.
    debug = 0,
    /// @brief Minor problems. Only emitted in verbose mode.
    soft_warning = 1,
    /// @brief Major problems with the document.
    warning = 2,
    /// @brief Problems with the document that prevent proper content generation.
    /// Usually results in the generation of `\\error` directives.
    error = 3,
    /// @brief Alias for `error`.
    max = 3,
    /// @brief Greater than all other levels.
    /// No diagnostic with this level is emitted, so using it as a minimum level
    /// silences all diagnostics, even errors.
    none = 4,
};

[[nodiscard]]
constexpr std::strong_ordering operator<=>(Severity x, Severity y) noexcept
{
    return Default_Underlying(x) <=> Default_Underlying(y);
}

[[nodiscard]]
constexpr bool severity_is_emittable(Severity x) noexcept
{
    return x >= Severity::min && x <= Severity::max;
}

struct Diagnostic {
    /// @brief The severity of the diagnostic.
    /// `severity_is_emittable(severity)` shall be `true`.
    Severity severity;
    /// @brief The id of the diagnostic,
    /// which is a non-empty string containing a
    /// dot-separated sequence of identifier for this diagnostic.
    Char_Sequence8 id;
    /// @brief The span of code that is responsible for this diagnostic.
    File_Source_Span location;
    /// @brief The diagnostic message parts.
    Char_Sequence8 message;
};

namespace diagnostic {

// GENERAL DIAGNOSTICS =============================================================================

/// @brief A deprecated feature was used.
inline constexpr std::u8string_view deprecated = u8"deprecated";

/// @brief A duplicate `id` attribute would have been generated.
inline constexpr std::u8string_view duplicate_id = u8"id.duplicate";

/// @brief In document post-processing,
/// a reference to a section was found that is not valid.
inline constexpr std::u8string_view section_ref_not_found = u8"section-ref.not-found";

/// @brief In document post-processing,
/// a reference to a section forms a circular dependency.
inline constexpr std::u8string_view section_ref_circular = u8"section-ref.circular";

/// @brief When loading a syntax highlighting theme,
/// conversion from JSON to to CSS failed.
inline constexpr std::u8string_view theme_conversion = u8"theme.conversion";

/// @brief Directive lookup failed.
inline constexpr std::u8string_view directive_lookup_unresolved = u8"directive-lookup.unresolved";

/// @brief Duplicate arguments to a directive were provided.
inline constexpr std::u8string_view duplicate_args = u8"duplicate.args";

/// @brief Arguments to a directive were ignored.
inline constexpr std::u8string_view ignored_args = u8"ignored.args";

/// @brief The content of a directive was ignored.
inline constexpr std::u8string_view ignored_content = u8"ignored.content";

/// @brief When parsing, a directive block was not terminated via closing brace.
inline constexpr std::u8string_view parse_block_unclosed = u8"parse.block.unclosed";

/// @brief In syntax highlighting,
/// the given language is not supported.
inline constexpr std::u8string_view highlight_language = u8"highlight.language";
/// @brief In syntax highlighting,
/// the code could not be highlighted because it is malformed.
inline constexpr std::u8string_view highlight_malformed = u8"highlight.malformed";
/// @brief In syntax highlighting,
/// something went wrong.
inline constexpr std::u8string_view highlight_error = u8"highlight.error";

/// @brief Unable to perform arithmetic due to a parse error.
inline constexpr std::u8string_view arithmetic_parse = u8"arithmetic.parse";
/// @brief Division by zero in arithmetic.
inline constexpr std::u8string_view arithmetic_div_by_zero = u8"arithmetic.div-by-zero";

// DIRECTIVE-SPECIFIC DIAGNOSTICS ==================================================================

namespace c {

/// @brief In `\\c`, the input is blank.
inline constexpr std::u8string_view blank = u8"c:blank";
/// @brief In `\\c`, the name is invalid, like `\\c{nonsense}`.
inline constexpr std::u8string_view name = u8"c:name";
/// @brief In `\\c`, parsing digits failed, like `\\c{#x1234abc}`.
inline constexpr std::u8string_view digits = u8"c:digits";
/// @brief In `\\c`, a nonscalar value would be encoded.
/// @see is_scalar_value
inline constexpr std::u8string_view nonscalar = u8"c:nonscalar";

} // namespace c

namespace code {

/// @brief In `\code`, the given `nested` parameter is not `yes` or `no`.
inline constexpr std::u8string_view nested_invalid = u8"code:nested.invalid";

} // namespace code

namespace codeblock {

/// @brief In `\codeblock`, the given `borders` parameter is not `yes` or `no`.
inline constexpr std::u8string_view borders_invalid = u8"codeblock:borders.invalid";

} // namespace codeblock

namespace U {

/// @brief In `\\U`, the input is blank.
inline constexpr std::u8string_view blank = u8"U:blank";
/// @brief In `\\U`, parsing digits failed, like `\\U{abc}`.
inline constexpr std::u8string_view digits = u8"U:digits";
/// @brief In `\\U`, a nonscalar value would be encoded.
/// @see is_scalar_value
inline constexpr std::u8string_view nonscalar = u8"U:nonscalar";

} // namespace U

namespace N {

/// @brief In `\\N`, the input is blank.
inline constexpr std::u8string_view blank = u8"N:blank";
/// @brief In `\\N`, the given name does not match any Unicode character name.
inline constexpr std::u8string_view invalid = u8"N:invalid";

} // namespace N

namespace Udigits {

/// @brief In a `\Udigits` directive,
/// the input is blank.
inline constexpr std::u8string_view blank = u8"Udigits:blank";
/// @brief In a `\Udigits` directive,
/// the input malformed text.
inline constexpr std::u8string_view malformed = u8"Udigits:malformed";
/// @brief In a `\Udigits` directive,
/// the input contains code units that were ignored because only the first code point is converted.
inline constexpr std::u8string_view ignored = u8"Udigits:ignored";

/// @brief The `zfill` argument could not be parsed as an integer.
inline constexpr std::u8string_view zfill_not_an_integer = u8"Udigits:zfill.parse";
/// @brief The `zfill` argument could not be parsed as an integer.
inline constexpr std::u8string_view zfill_range = u8"Udigits:zfill.range";
/// @brief The `base` argument could not be parsed as an integer.
inline constexpr std::u8string_view base_not_an_integer = u8"Udigits:base.parse";
/// @brief The `base` argument could not be parsed as an integer.
inline constexpr std::u8string_view base_range = u8"Udigits:base.base";

/// @brief The `lower` argument is neither `yes` nor `no`.
inline constexpr std::u8string_view lower_invalid = u8"Udigits:lower.invalid";

} // namespace Udigits

namespace h {

/// @brief In `\hN` headings, the given `listed` parameter is not `yes` or `no`.
inline constexpr std::u8string_view listed_invalid = u8"h:listed.invalid";
/// @brief In `\hN` headings, the given `show-number` parameter is not `yes` or `no`.
inline constexpr std::u8string_view show_number_invalid = u8"h:show-number.invalid";

} // namespace h

namespace hl {

/// @brief In a `\hl` directive,
/// no name parameter was provided.
inline constexpr std::u8string_view name_missing = u8"hl:name.missing";
/// @brief In a `\hl` directive,
/// the given highlight name is not valid.
inline constexpr std::u8string_view name_invalid = u8"hl:name.invalid";

} // namespace hl

namespace include {

/// @brief In an `\include` directive,
/// no file path was provided.
inline constexpr std::u8string_view path_missing = u8"include:path.empty";

/// @brief In an `\include` directive,
/// the file could not be loaded.
inline constexpr std::u8string_view io = u8"include:io";

} // namespace include

namespace import {

/// @brief In an `\import` directive,
/// no file path was provided.
inline constexpr std::u8string_view path_missing = u8"import:path.empty";

/// @brief In an `\import` directive,
/// the file could not be loaded.
inline constexpr std::u8string_view io = u8"import:io";

} // namespace import

namespace there {

/// @brief In a `\there` directive,
/// no section was provided.
inline constexpr std::u8string_view no_section = u8"there:no-section";

} // namespace there

namespace here {

/// @brief In a `\here` directive,
/// no section was provided.
inline constexpr std::u8string_view no_section = u8"here:no-section";

} // namespace here

namespace macro {

/// @brief A `\put[...]` pseudo-directive was used outside of an argument list,
/// making expansion impossible.
inline constexpr std::u8string_view put_args_outside_args = u8"macro:put.args.outside-args";

/// @brief The content of a `\put` directive is invalid.
inline constexpr std::u8string_view put_invalid = u8"macro:put.invalid";

/// @brief The index of a positional argument was given to a `\put` directive,
/// but not enough positional arguments were provided.
inline constexpr std::u8string_view put_out_of_range = u8"macro:put.out-of-range";

} // namespace macro

namespace math {

/// @brief In a `\math` directive,
/// text was not properly enclosed in `\mi`, `\mn`, etc.
inline constexpr std::u8string_view text = u8"math:text";

} // namespace math

namespace ref {

/// @brief In a `\ref` directive,
/// no `to` argument was provided,
/// meaning that the reference is without any target.
inline constexpr std::u8string_view to_missing = u8"ref:to.missing";

/// @brief In a `\ref` directive,
/// the target is empty.
inline constexpr std::u8string_view to_empty = u8"ref:to.empty";

/// @brief In a `\ref` directive,
/// the target cannot be classified as a URL or anything else,
/// and the target cannot be resolved as a document.
inline constexpr std::u8string_view to_unresolved = u8"ref:to.unresolved";

/// @brief In a `\ref` directive where the target is a C++ draft URL,
/// failed to verbalize the URL.
inline constexpr std::u8string_view draft_verbalization = u8"ref:draft.verbalization";

} // namespace ref

namespace bib {

/// @brief In a `\bib` directive,
/// no `id` was provided.
inline constexpr std::u8string_view id_missing = u8"bib:id.missing";

/// @brief In a `\bib` directive,
/// the specified `id` is empty.
inline constexpr std::u8string_view id_empty = u8"bib:id.empty";

/// @brief In a `\bib` directive,
/// an attempt was made to add a duplicate entry.
inline constexpr std::u8string_view duplicate = u8"bib:duplicate";

} // namespace bib

namespace wg21_head {

/// @brief In a `\wg21-head` directive,
/// no title was specified.
inline constexpr std::u8string_view no_title = u8"wg21-head.no_title";

} // namespace wg21_head

namespace macro {

/// @brief In a `\macro` directive,
/// no pattern was provided.
inline constexpr std::u8string_view no_pattern = u8"macro:pattern.none";

/// @brief In a `\macro` directive,
/// the given pattern is not a directive.
inline constexpr std::u8string_view pattern_no_directive = u8"macro:pattern.no-directive";

/// @brief In a `\macro` directive,
/// the same macro was defined multiple times.
inline constexpr std::u8string_view redefinition = u8"macro:redefinition";

} // namespace macro

} // namespace diagnostic

} // namespace cowel

#endif
