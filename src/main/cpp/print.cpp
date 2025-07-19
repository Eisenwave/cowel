#include "cowel/fwd.hpp"

#ifndef COWEL_EMSCRIPTEN

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <optional>
#include <span>
#include <string_view>

#include "cowel/util/annotated_string.hpp"
#include "cowel/util/ansi.hpp"
#include "cowel/util/assert.hpp"
#include "cowel/util/io.hpp"
#include "cowel/util/source_position.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/to_chars.hpp"
#include "cowel/util/tty.hpp"

#include "cowel/ast.hpp"
#include "cowel/diagnostic_highlight.hpp"
#include "cowel/print.hpp"

namespace cowel {

namespace {

[[nodiscard]]
std::u8string_view diagnostic_highlight_ansi_sequence(Diagnostic_Highlight type)
{
    switch (type) {
        using enum Diagnostic_Highlight;

    case text:
    case code_citation:
    case punctuation:
    case op: return ansi::reset;

    case code_position:
    case internal:
    case diff_common: return ansi::h_black;

    case error_text:
    case error:
    case diff_del: return ansi::h_red;

    case warning:
    case line_number: return ansi::h_yellow;

    case note: return ansi::h_white;

    case success:
    case position_indicator:
    case diff_ins: return ansi::h_green;

    case internal_error_notice: return ansi::h_yellow;

    case operand: return ansi::h_magenta;

    case tag: return ansi::h_blue;

    case attribute: return ansi::h_magenta;

    case escape: return ansi::h_yellow;
    }
    COWEL_ASSERT_UNREACHABLE(u8"Unknown code span type.");
}

enum struct Error_Line_Type : Default_Underlying { note, error };

struct Error_Line {
    std::optional<File_Source_Position> pos {};
    std::u8string_view message;
    bool omit_affected_line = false;
};

[[nodiscard]]
std::u8string_view to_prose(IO_Error_Code e)
{
    using enum IO_Error_Code;
    switch (e) {
    case cannot_open: //
        return u8"Failed to open file.";
    case read_error: //
        return u8"I/O error occurred when reading from file.";
    case write_error: //
        return u8"I/O error occurred when writing to file.";
    case corrupted: //
        return u8"Data in the file is corrupted (not properly encoded).";
    }
    COWEL_ASSERT_UNREACHABLE(u8"invalid error code");
}

void do_print_affected_line(
    Diagnostic_String& out,
    std::u8string_view source,
    std::size_t begin,
    std::size_t length,
    std::size_t line,
    std::size_t column
)
{
    COWEL_ASSERT(length > 0);

    // TODO: add proper multi-line support

    const std::u8string_view cited_code = find_line(source, begin);

    const auto line_chars = to_characters<char8_t>(line + 1);
    constexpr std::size_t pad_max = 6;
    const std::size_t pad_length
        = pad_max - std::min(line_chars.length(), std::size_t { pad_max - 1 });
    out.append(pad_length, u8' ');
    out.append_integer(line + 1, Diagnostic_Highlight::line_number);
    out.append(u8' ');
    out.append(u8'|', Diagnostic_Highlight::punctuation);
    out.append(u8' ');
    out.append(cited_code, Diagnostic_Highlight::code_citation);
    out.append(u8'\n');

    const std::size_t align_length = std::max(pad_max, line_chars.length() + 1);
    out.append(align_length, u8' ');
    out.append(u8' ');
    out.append(u8'|', Diagnostic_Highlight::punctuation);
    out.append(u8' ');
    out.append(column, u8' ');
    {
        const std::size_t indicator_length = std::min(length, cited_code.length() - column);
        auto position = out.build(Diagnostic_Highlight::position_indicator);
        position.append(u8'^');
        if (indicator_length) {
            position.append(indicator_length - 1, u8'~');
        }
    }
    out.append(u8'\n');
}

} // namespace

void print_file_position(
    Diagnostic_String& out,
    std::u8string_view file,
    const Source_Position& pos,
    bool colon_suffix
)
{
    auto builder = out.build(Diagnostic_Highlight::code_position);
    builder.append(file)
        .append(':')
        .append_integer(pos.line + 1)
        .append(':')
        .append_integer(pos.column + 1);
    if (colon_suffix) {
        builder.append(':');
    }
}

void print_affected_line(
    Diagnostic_String& out,
    std::u8string_view source,
    const Source_Position& pos
)
{
    do_print_affected_line(out, source, pos.begin, 1, pos.line, pos.column);
}

void print_affected_line(Diagnostic_String& out, std::u8string_view source, const Source_Span& pos)
{
    COWEL_ASSERT(!pos.empty());
    do_print_affected_line(out, source, pos.begin, pos.length, pos.line, pos.column);
}

std::u8string_view find_line(std::u8string_view source, std::size_t index)
{
    COWEL_ASSERT(index <= source.size());

    if (index == source.size() || source[index] == '\n') {
        // Special case for EOF positions, which may be past the end of a line,
        // and even past the end of the whole source, but only by a single character.
        // For such positions, we yield the currently ended line.
        --index;
    }

    std::size_t begin = source.rfind('\n', index);
    begin = begin != std::u8string_view::npos ? begin + 1 : 0;

    const std::size_t end = std::min(source.find('\n', index + 1), source.size());

    return source.substr(begin, end - begin);
}

void print_location_of_file(Diagnostic_String& out, std::u8string_view file)
{
    out.build(Diagnostic_Highlight::code_position).append(file).append(u8':');
}

void print_assertion_error(Diagnostic_String& out, const Assertion_Error& error)
{
    out.append(u8"Assertion failed! ", Diagnostic_Highlight::error_text);

    const std::u8string_view message = error.type == Assertion_Error_Type::expression
        ? u8"The following expression evaluated to 'false', but was expected to be 'true':"
        : u8"Code which must be unreachable has been reached.";
    out.append(message, Diagnostic_Highlight::text);
    out.append(u8"\n\n");

    const Source_Position pos { .line = error.location.line(),
                                .column = error.location.column(),
                                .begin = {} };
    print_file_position(out, as_u8string_view(error.location.file_name()), pos);
    out.append(u8' ');
    out.append(error.message, Diagnostic_Highlight::error_text);
    out.append(u8"\n\n");
    print_internal_error_notice(out);
}

void print_io_error(Diagnostic_String& out, std::u8string_view file, IO_Error_Code error)
{
    print_location_of_file(out, file);
    out.append(u8' ');
    out.append(to_prose(error), Diagnostic_Highlight::text);
    out.append(u8'\n');
}

namespace {

void print_cut_off(Diagnostic_String& out, std::u8string_view v, std::size_t limit)
{
    std::size_t visual_length = 0;

    for (std::size_t i = 0; i < v.length();) {
        if (visual_length >= limit) {
            out.append(u8"...", Diagnostic_Highlight::punctuation);
            break;
        }

        if (v[i] == '\r') {
            out.append(u8"\\r", Diagnostic_Highlight::escape);
            visual_length += 2;
            ++i;
        }
        else if (v[i] == '\t') {
            out.append(u8"\\t", Diagnostic_Highlight::escape);
            visual_length += 2;
            ++i;
        }
        else if (v[i] == '\n') {
            out.append(u8"\\n", Diagnostic_Highlight::escape);
            visual_length += 2;
            ++i;
        }
        else {
            const auto remainder = v.substr(i, limit - visual_length);
            const auto part = remainder.substr(0, remainder.find_first_of(u8"\r\t\n"));
            out.append(part, Diagnostic_Highlight::code_citation);
            visual_length += part.size();
            i += part.size();
        }
    }
}

} // namespace

struct [[nodiscard]]
AST_Printer final : ast::Const_Visitor {
private:
    using char_type = char8_t;
    using string_view_type = std::u8string_view;
    using annotated_string_type = Diagnostic_String;

    annotated_string_type& out;
    string_view_type source;
    const AST_Formatting_Options options;
    int indent_level = 0;

    struct [[nodiscard]] Scoped_Indent {
        int& level;

        explicit Scoped_Indent(int& level)
            : level { ++level }
        {
        }

        Scoped_Indent(const Scoped_Indent&) = delete;
        Scoped_Indent& operator=(const Scoped_Indent&) = delete;

        ~Scoped_Indent()
        {
            --level;
        }
    };

public:
    AST_Printer(
        annotated_string_type& out,
        string_view_type source,
        const AST_Formatting_Options& options
    )
        : out { out }
        , source { source }
        , options { options }
    {
        COWEL_ASSERT(options.max_node_text_length >= 3);
    }

    void visit(const ast::Text& node) final
    {
        print_indent();

        out.append(u8"Text", Diagnostic_Highlight::tag);
        out.append(u8'(', Diagnostic_Highlight::punctuation);
        print_cut_off(out, node.get_source(), std::size_t(options.max_node_text_length));
        out.append(u8')', Diagnostic_Highlight::punctuation);
        out.append(u8'\n');
    }

    void visit(const ast::Comment& node) final
    {
        print_indent();

        out.append(u8"Comment", Diagnostic_Highlight::tag);
        out.append(u8'(', Diagnostic_Highlight::punctuation);
        print_cut_off(out, node.get_source(), std::size_t(options.max_node_text_length));
        out.append(u8')', Diagnostic_Highlight::punctuation);
        out.append(u8'\n');
    }

    void visit(const ast::Escaped& node) final
    {
        print_indent();

        out.append(u8"Escaped", Diagnostic_Highlight::tag);
        out.append(u8'(', Diagnostic_Highlight::punctuation);
        print_cut_off(out, node.get_source(), std::size_t(options.max_node_text_length));
        out.append(u8')', Diagnostic_Highlight::punctuation);
        out.append(u8'\n');
    }

    void visit(const ast::Directive& directive) final
    {
        print_indent();

        out.build(Diagnostic_Highlight::tag).append(u8'\\').append(directive.get_name());

        if (!directive.get_arguments().empty()) {
            out.append(u8'[', Diagnostic_Highlight::punctuation);
            out.append(u8'\n');
            {
                const Scoped_Indent i = indented();
                visit_arguments(directive);
            }
            print_indent();
            out.append(u8']', Diagnostic_Highlight::punctuation);
        }
        else {
            out.append(u8"[]", Diagnostic_Highlight::punctuation);
        }

        if (!directive.get_content().empty()) {
            out.append(u8'{', Diagnostic_Highlight::punctuation);
            out.append(u8'\n');
            {
                const Scoped_Indent i = indented();
                visit_content_sequence(directive.get_content());
            }
            print_indent();
            out.append(u8'}', Diagnostic_Highlight::punctuation);
        }
        else {
            out.append(u8"{}", Diagnostic_Highlight::punctuation);
        }

        out.append(u8'\n');
    }

    void visit(const ast::Generated& generated) final
    {
        print_indent();

        out.append(u8"Generated", Diagnostic_Highlight::tag);

        out.append(u8'{', Diagnostic_Highlight::punctuation);
        out.append(generated.as_string(), Diagnostic_Highlight::text);
        out.append(u8'}', Diagnostic_Highlight::punctuation);

        out.append(u8'\n');
    }

    void visit(const ast::Argument& arg) final
    {
        print_indent();

        if (arg.has_name()) {
            out.append(u8"Named_Argument", Diagnostic_Highlight::tag);
            out.append(u8'(', Diagnostic_Highlight::punctuation);
            out.append(arg.get_name(), Diagnostic_Highlight::attribute);
            out.append(u8')', Diagnostic_Highlight::punctuation);
        }
        else {
            out.append(u8"Positional_Argument", Diagnostic_Highlight::tag);
        }

        if (!arg.get_content().empty()) {
            out.append(u8'\n');
            const Scoped_Indent i = indented();
            visit_content_sequence(arg.get_content());
        }
        else {
            out.append(u8" (empty value)", Diagnostic_Highlight::internal);
            out.append(u8'\n');
        }
    }

private:
    Scoped_Indent indented()
    {
        return Scoped_Indent { indent_level };
    }

    void print_indent()
    {
        COWEL_ASSERT(indent_level >= 0);
        COWEL_ASSERT(options.indent_width >= 0);

        const auto indent = std::size_t(options.indent_width * indent_level);
        out.append(indent, u8' ');
    }
};

void print_ast(
    Diagnostic_String& out,
    std::u8string_view source,
    std::span<const ast::Content> root_content,
    AST_Formatting_Options options
)
{
    AST_Printer { out, source, options }.visit_content_sequence(root_content);
}

void print_internal_error_notice(Diagnostic_String& out)
{
    constexpr std::u8string_view notice
        = u8"This is an internal error. Please report this bug at:\n"
          u8"https://github.com/Eisenwave/bit-manipulation/issues\n";
    out.append(notice, Diagnostic_Highlight::internal_error_notice);
}

#ifndef COWEL_EMSCRIPTEN

std::ostream& operator<<(std::ostream& out, std::u8string_view str)
{
    return out << as_string_view(str);
}

std::ostream& print_code_string(std::ostream& out, const Diagnostic_String& string, bool colors)
{
    const std::u8string_view text = string.get_text();
    if (!colors) {
        return out << text;
    }

    Annotation_Span<Diagnostic_Highlight> previous {};
    for (const Annotation_Span<Diagnostic_Highlight> span : string) {
        const std::size_t previous_end = previous.begin + previous.length;
        COWEL_ASSERT(span.begin >= previous_end);
        if (previous_end != span.begin) {
            out << text.substr(previous_end, span.begin - previous_end);
        }
        out << diagnostic_highlight_ansi_sequence(span.value)
            << text.substr(span.begin, span.length) << ansi::reset;
        previous = span;
    }
    const std::size_t last_span_end = previous.begin + previous.length;
    if (last_span_end != text.size()) {
        out << text.substr(last_span_end);
    }

    return out;
}

void print_code_string_stdout(const Diagnostic_String& string)
{
    print_code_string(std::cout, string, is_stdout_tty);
}

void print_code_string_stderr(const Diagnostic_String& string)
{
    print_code_string(std::cerr, string, is_stderr_tty);
}

void flush_stdout()
{
    std::cout.flush();
}

void flush_stderr()
{
    std::cerr.flush();
}

#endif

} // namespace cowel

#endif
