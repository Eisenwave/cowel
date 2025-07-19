#ifndef COWEL_DIAGNOSTICS_HPP
#define COWEL_DIAGNOSTICS_HPP

#include "cowel/fwd.hpp"

#ifdef COWEL_EMSCRIPTEN
#error "This header should not be included when using emscripten."
#endif

#include <cstddef>
#include <iosfwd>
#include <span>
#include <string_view>

#include "cowel/util/assert.hpp"
#include "cowel/util/io.hpp"
#include "cowel/util/source_position.hpp"

namespace cowel {

/// @brief Returns the line that contains the given index.
/// @param source the source string
/// @param index the index within the source string, in range `[0, source.size())`
/// @return A line which contains the given `index`.
[[nodiscard]]
std::u8string_view find_line(std::u8string_view source, std::size_t index);

/// @brief Prints the location of the file nicely formatted.
/// @param out the string to write to
/// @param file the file
void print_location_of_file(Diagnostic_String& out, std::u8string_view file);

/// @brief Prints a position within a file, consisting of the file name and line/column.
/// @param out the string to write to
/// @param file the file
/// @param pos the position within the file
/// @param colon_suffix if `true`, appends a `:` to the string as part of the same token
void print_file_position(
    Diagnostic_String& out,
    std::u8string_view file,
    const Source_Position& pos,
    bool colon_suffix = true
);

/// @brief Prints the contents of the affected line within `source` as well as position indicators
/// which show the span which is affected by some diagnostic.
/// @param out the string to write to
/// @param source the program source
/// @param pos the position within the source
void print_affected_line(
    Diagnostic_String& out,
    std::u8string_view source,
    const Source_Position& pos
);

void print_affected_line(Diagnostic_String& out, std::u8string_view source, const Source_Span& pos);

void print_assertion_error(Diagnostic_String& out, const Assertion_Error& error);

void print_io_error(Diagnostic_String& out, std::u8string_view file, IO_Error_Code error);

struct AST_Formatting_Options {
    int indent_width;
    int max_node_text_length;
};

void print_ast(
    Diagnostic_String& out,
    std::u8string_view source,
    std::span<const ast::Content> root_content,
    AST_Formatting_Options
);

void print_internal_error_notice(Diagnostic_String& out);

#ifndef COWEL_EMSCRIPTEN
std::ostream& operator<<(std::ostream& out, std::u8string_view str);
#endif

#ifndef COWEL_EMSCRIPTEN
std::ostream& print_code_string(std::ostream& out, const Diagnostic_String& string, bool colors);

void print_code_string_stdout(const Diagnostic_String&);
void print_code_string_stderr(const Diagnostic_String&);
void flush_stdout();
void flush_stderr();

#endif

} // namespace cowel

#endif
