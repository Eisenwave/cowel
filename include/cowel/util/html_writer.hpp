#ifndef COWEL_HTML_WRITER_HPP
#define COWEL_HTML_WRITER_HPP

#include <algorithm>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

#include "cowel/util/assert.hpp"
#include "cowel/util/chars.hpp"
#include "cowel/util/html.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/url_encode.hpp"

#include "cowel/policy/content_policy.hpp"

#include "cowel/fwd.hpp"

namespace cowel {

enum struct Attribute_Encoding : Default_Underlying {
    text,
    url,
};

enum struct Attribute_Quoting : bool {
    none,
    quoted,
};

/// @brief Appends text to a vector without any processing.
inline void append(std::pmr::vector<char8_t>& out, std::u8string_view text)
{
    out.insert(out.end(), text.data(), text.data() + text.size());
}

enum struct Attribute_Style : Default_Underlying {
    /// @brief Always use double quotes, like `id="name" class="a b" hidden=""`.
    always_double,
    /// @brief Always use single quotes, like `id='name' class='a b' hidden=''`.
    always_single,
    /// @brief Use single quotes when needed, like `id=name class="a b" hidden`.
    double_if_needed,
    /// @brief Use single quotes when needed, like `id=name class='a b' hidden`.
    single_if_needed,
};

[[nodiscard]]
constexpr bool attribute_style_demands_quotes(Attribute_Style style)
{
    return style == Attribute_Style::always_double || style == Attribute_Style::always_single;
}

[[nodiscard]]
constexpr char8_t attribute_style_quote_char(Attribute_Style style)
{
    switch (style) {
    case Attribute_Style::always_double:
    case Attribute_Style::double_if_needed: return '"';
    case Attribute_Style::always_single:
    case Attribute_Style::single_if_needed: return '\'';
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid attribute style.");
}

template <string_or_char_consumer Out>
struct Basic_Attribute_Writer;

/// @brief A class which provides member functions for writing HTML content to a stream
/// correctly.
/// Both entire HTML documents can be written, as well as HTML snippets.
/// This writer only performs checks that are possibly without additional memory.
/// These include:
/// - verifying that given tag names and values are appropriate
/// - ensuring that the number of opened tags matches the number of closed tags
/// - ensuring that the preamble is written exactly once
///
/// To correctly use this class, the opening tags must match the closing tags.
/// I.e. for every `begin_tag(tag, type)` or `begin_tag_with_attributes(tag, type)`,
/// there must be a matching `end_tag(tag, type)`.
template <string_or_char_consumer Out>
struct Basic_HTML_Writer {
public:
    friend Basic_Attribute_Writer<Out>;
    using Self = Basic_HTML_Writer;

private:
    Out m_out;

    std::size_t m_depth = 0;
    bool m_in_attributes = false;

public:
    /// @brief Constructor.
    /// Writes nothing to the stream.
    /// `out.fail()` shall be true.
    /// @param out the output stream
    [[nodiscard]]
    explicit Basic_HTML_Writer(const Out& out)
        : m_out { out }
    {
    }
    [[nodiscard]]
    explicit Basic_HTML_Writer(Out&& out)
        : m_out { std::move(out) }
    {
    }

    Basic_HTML_Writer(const Basic_HTML_Writer&) = delete;
    Basic_HTML_Writer& operator=(const Basic_HTML_Writer&) = delete;

    ~Basic_HTML_Writer() = default;

    Out& get_output()
    {
        return m_out;
    }

    /// @brief Returns the output vector that this writers has been constructed with,
    /// and to which all output is written.
    [[nodiscard]]
    const Out& get_output() const
    {
        return m_out;
    }

    /// @brief Validates whether the HTML document is complete.
    /// Namely, `write_preamble()` must have taken place and any opened tags must have
    /// been closed.
    /// This member function has false negatives, however, it has no false positives.
    /// @return `true` if the writer is in a state where the HTML document could be considered
    /// complete, `false` otherwise.
    [[nodiscard]]
    bool is_done() const
    {
        return m_depth == 0;
    }

    /// @brief Sets the element depth tracked by the writer.
    /// This is useful when the writer is used to output unbalanced HTML tags.
    Self& set_depth(std::size_t depth)
    {
        m_depth = depth;
        return *this;
    }

    /// @brief Writes the `<!DOCTYPE ...` preamble for the HTML file.
    /// For whole documents should be called exactly once, prior to any other `write` functions.
    /// However, it is not required to call this.
    Self& write_preamble()
    {
        COWEL_ASSERT(!m_in_attributes);
        m_out(u8"<!DOCTYPE html>\n");
        return *this;
    }

    /// @brief Writes a self-closing tag such as `<br/>` or `<hr/>`.
    Self& write_self_closing_tag(std::u8string_view id)
    {
        COWEL_ASSERT(!m_in_attributes);
        COWEL_ASSERT(is_html_tag_name(id));

        m_out(u8'<');
        m_out(id);
        m_out(u8"/>");

        return *this;
    }

    /// @brief Writes an HTML comment with the given contents.
    Self& write_comment(std::u8string_view comment)
    {
        m_out(u8"<!--");
        append_html_escaped_of(m_out, comment, u8"<>");
        m_out(u8"-->");
        return *this;
    }

    /// @brief Writes an opening tag such as `<div>`.
    Self& open_tag(std::u8string_view id)
    {
        COWEL_ASSERT(!m_in_attributes);
        COWEL_ASSERT(is_html_tag_name(id));

        m_out(u8'<');
        m_out(id);
        m_out(u8'>');
        ++m_depth;

        return *this;
    }

    /// @brief Writes an opening tag immediately followed by a closing tag, like `<div></div>`.
    Self& open_and_close_tag(std::u8string_view id)
    {
        COWEL_ASSERT(!m_in_attributes);
        COWEL_ASSERT(is_html_tag_name(id));

        m_out(u8'<');
        m_out(id);
        m_out(u8"></");
        m_out(id);
        m_out(u8'>');

        return *this;
    }

    /// @brief Writes an incomplete opening tag such as `<div`.
    /// Returns an `Attribute_Writer` which must be used to write attributes (if any)
    /// and complete the opening tag.
    /// @param properties the tag properties
    /// @return `*this`
    [[nodiscard]]
    Basic_Attribute_Writer<Out> open_tag_with_attributes(std::u8string_view id)
    {
        COWEL_ASSERT(!m_in_attributes);
        COWEL_ASSERT(is_html_tag_name(id));

        m_out(u8'<');
        m_out(id);

        return Basic_Attribute_Writer<Out> { *this };
    }

    /// @brief Writes a closing tag, such as `</div>`.
    /// The most recent call to `open_tag` or `open_tag_with_attributes` shall have been made with
    /// the same arguments.
    Self& close_tag(std::u8string_view id)
    {
        COWEL_ASSERT(!m_in_attributes);
        COWEL_ASSERT(is_html_tag_name(id));
        COWEL_ASSERT(m_depth != 0);

        --m_depth;

        m_out(u8"</");
        m_out(id);
        m_out(u8'>');

        return *this;
    }

    /// @brief Writes text between tags.
    /// Text characters such as `<` or `>` which interfere with HTML are converted to entities.
    void write_inner_text(std::u8string_view text)
    {
        COWEL_ASSERT(!m_in_attributes);
        append_html_escaped_of(m_out, text, u8"&<>");
    }
    void write_inner_text(std::u32string_view text)
    {
        COWEL_ASSERT(!m_in_attributes);
        for (const char32_t c : text) {
            write_inner_text(c);
        }
    }
    void write_inner_text(char8_t c)
    {
        COWEL_DEBUG_ASSERT(is_ascii(c));
        write_inner_text({ &c, 1 });
    }
    void write_inner_text(char32_t c)
    {
        COWEL_DEBUG_ASSERT(!m_in_attributes);
        COWEL_DEBUG_ASSERT(is_scalar_value(c));
        m_out(
            is_html_min_raw_passthrough_character(c) ? utf8::encode8_unchecked(c).as_string()
                                                     : html_entity_of(c)
        );
    }

    /// @brief Writes HTML content between tags.
    /// Unlike `write_inner_text`, does not escape any entities.
    ///
    /// WARNING: Improper use of this function can easily result in incorrect HTML output.
    void write_inner_html(std::u8string_view text)
    {
        COWEL_ASSERT(!m_in_attributes);
        m_out(text);
    }
    void write_inner_html(std::u32string_view text)
    {
        COWEL_ASSERT(!m_in_attributes);
        for (const char32_t c : text) {
            write_inner_html(c);
        }
    }
    void write_inner_html(char8_t c)
    {
        COWEL_DEBUG_ASSERT(is_ascii(c));
        m_out(c);
    }
    void write_inner_html(char32_t c)
    {
        COWEL_DEBUG_ASSERT(!m_in_attributes);
        COWEL_DEBUG_ASSERT(is_scalar_value(c));
        m_out(utf8::encode8_unchecked(c).as_string());
    }

private:
    Attribute_Quoting write_attribute(
        std::u8string_view key,
        std::u8string_view value,
        Attribute_Style style,
        Attribute_Encoding encoding
    )
    {
        return write_attribute(key, { &value, 1 }, style, encoding);
    }
    Attribute_Quoting write_attribute(
        std::u8string_view key,
        std::span<const std::u8string_view> value_parts,
        Attribute_Style style,
        Attribute_Encoding encoding
    )
    {
        if (std::ranges::all_of(value_parts, [](std::u8string_view s) { return s.empty(); })) {
            return write_empty_attribute(key, style);
        }

        COWEL_ASSERT(m_in_attributes);
        COWEL_ASSERT(is_html_attribute_name(key));

        m_out(u8' ');
        m_out(key);

        const char8_t quote_char = attribute_style_quote_char(style);
        m_out(u8'=');

        const bool omit_quotes = !attribute_style_demands_quotes(style)
            && std::ranges::all_of(value_parts, [](std::u8string_view s) {
                   return is_html_unquoted_attribute_value(s);
               });

        if (omit_quotes) {
            for (const std::u8string_view part : value_parts) {
                switch (encoding) {
                case Attribute_Encoding::text: {
                    m_out(part);
                    break;
                }
                case Attribute_Encoding::url: {
                    url_encode_ascii_if(m_out, part, [](char8_t c) {
                        return is_url_always_encoded(c);
                    });
                    break;
                }
                }
            }
        }
        else {
            m_out(quote_char);
            for (const std::u8string_view part : value_parts) {
                switch (encoding) {
                case Attribute_Encoding::text: {
                    append_html_escaped_of(m_out, part, u8"\"'");
                    break;
                }
                case Attribute_Encoding::url: {
                    url_encode_ascii_if(m_out, part, [](char8_t c) {
                        static_assert(is_url_always_encoded(u8'"'));
                        static_assert(!is_url_always_encoded(u8'\''));
                        return c == u8'\'' || is_url_always_encoded(c);
                    });
                    break;
                }
                }
            }
            m_out(quote_char);
        }

        return omit_quotes ? Attribute_Quoting::none : Attribute_Quoting::quoted;
    }

    Attribute_Quoting write_empty_attribute(std::u8string_view key, Attribute_Style style)
    {
        COWEL_ASSERT(m_in_attributes);
        COWEL_ASSERT(is_html_attribute_name(key));

        m_out(u8' ');
        m_out(key);

        switch (style) {
        case Attribute_Style::always_double: {
            m_out(u8"=\"\"");
            return Attribute_Quoting::quoted;
        }
        case Attribute_Style::always_single: {
            m_out(u8"=''");
            return Attribute_Quoting::quoted;
        }
        default: {
            return Attribute_Quoting::none;
        }
        }
    }
    Self& end_attributes()
    {
        COWEL_ASSERT(m_in_attributes);

        m_out(u8'>');
        m_in_attributes = false;
        ++m_depth;

        return *this;
    }
    Self& end_empty_tag_attributes()
    {
        COWEL_ASSERT(m_in_attributes);

        m_out(u8"/>");
        m_in_attributes = false;

        return *this;
    }
};

/// @brief RAII helper class which lets us write attributes more conveniently.
/// This class is not intended to be used directly, but with the help of `HTML_Writer`.
template <string_or_char_consumer Out>
struct Basic_Attribute_Writer {
private:
    Basic_HTML_Writer<Out>& m_writer;
    /// @brief If this is `true`,
    /// it would not be safe to append a `/` character to the written data because it may be
    /// included in the value of an attribute.
    /// For example, this can happen when writing `<br id=xyz`.
    /// Now appending `/>` to close the attribute would result in the `/` being appended to `xzy`.
    bool m_unsafe_slash = false;

public:
    explicit Basic_Attribute_Writer(Basic_HTML_Writer<Out>& writer)
        : m_writer(writer)
    {
        m_writer.m_in_attributes = true;
    }

    Basic_Attribute_Writer(const Basic_Attribute_Writer&) = delete;
    Basic_Attribute_Writer& operator=(const Basic_Attribute_Writer&) = delete;

    /// @brief Writes an attribute to the stream, such as `class=centered`.
    /// If `value` is empty, writes `key` on its own.
    /// If `value` requires quotes to comply with the HTML standard, quotes are added.
    /// For example, if `value` is `x y`, `key="x y"` is written.
    /// @param key the attribute key; `is_identifier(key)` shall be `true`.
    /// @param value the attribute value, or an empty string
    /// @return `*this`
    Basic_Attribute_Writer& write_attribute(
        std::u8string_view key,
        std::u8string_view value,
        Attribute_Style style = Attribute_Style::double_if_needed
    )
    {
        const Attribute_Quoting quoting
            = m_writer.write_attribute(key, value, style, Attribute_Encoding::text);
        m_unsafe_slash = quoting == Attribute_Quoting::none;
        return *this;
    }

    /// @brief Like the overload taking a single `string_view` as a value,
    /// but allows for the attribute to consist of multiple parts,
    /// which are joined together.
    Basic_Attribute_Writer& write_attribute(
        std::u8string_view key,
        std::span<const std::u8string_view> value_parts,
        Attribute_Style style = Attribute_Style::double_if_needed
    )
    {
        const Attribute_Quoting quoting
            = m_writer.write_attribute(key, value_parts, style, Attribute_Encoding::text);
        m_unsafe_slash = quoting == Attribute_Quoting::none;
        return *this;
    }

    /// @brief Like `write_attribute`,
    /// but applies minimal URL encoding to the value.
    Basic_Attribute_Writer& write_url_attribute(
        std::u8string_view key,
        std::u8string_view value,
        Attribute_Style style = Attribute_Style::double_if_needed
    )
    {
        const Attribute_Quoting quoting
            = m_writer.write_attribute(key, value, style, Attribute_Encoding::url);
        m_unsafe_slash = quoting == Attribute_Quoting::none;
        return *this;
    }

    Basic_Attribute_Writer& write_empty_attribute(
        std::u8string_view key,
        Attribute_Style style = Attribute_Style::double_if_needed
    )
    {
        m_writer.write_empty_attribute(key, style);
        m_unsafe_slash = false;
        return *this;
    }

    Basic_Attribute_Writer& write_charset(
        std::u8string_view value,
        Attribute_Style style = Attribute_Style::double_if_needed
    )
    {
        return write_attribute(u8"charset", value, style);
    }

    Basic_Attribute_Writer&
    write_class(std::u8string_view value, Attribute_Style style = Attribute_Style::double_if_needed)
    {
        return write_attribute(u8"class", value, style);
    }

    Basic_Attribute_Writer& write_content(
        std::u8string_view value,
        Attribute_Style style = Attribute_Style::double_if_needed
    )
    {
        return write_attribute(u8"content", value, style);
    }

    Basic_Attribute_Writer& write_crossorigin()
    {
        return write_empty_attribute(u8"crossorigin");
    }

    Basic_Attribute_Writer&
    write_href(std::u8string_view value, Attribute_Style style = Attribute_Style::double_if_needed)
    {
        return write_url_attribute(u8"href", value, style);
    }

    Basic_Attribute_Writer&
    write_id(std::u8string_view value, Attribute_Style style = Attribute_Style::double_if_needed)
    {
        return write_attribute(u8"id", value, style);
    }

    Basic_Attribute_Writer&
    write_name(std::u8string_view value, Attribute_Style style = Attribute_Style::double_if_needed)
    {
        return write_attribute(u8"name", value, style);
    }

    Basic_Attribute_Writer&
    write_rel(std::u8string_view value, Attribute_Style style = Attribute_Style::double_if_needed)
    {
        return write_attribute(u8"rel", value, style);
    }

    Basic_Attribute_Writer&
    write_src(std::u8string_view value, Attribute_Style style = Attribute_Style::double_if_needed)
    {
        return write_attribute(u8"src", value, style);
    }

    Basic_Attribute_Writer& write_tabindex(
        std::u8string_view value,
        Attribute_Style style = Attribute_Style::double_if_needed
    )
    {
        return write_attribute(u8"tabindex", value, style);
    }

    /// @brief Writes `>` and finishes writing attributes.
    /// This function or `end_empty()` shall be called exactly once prior to destruction of this
    /// writer.
    Basic_Attribute_Writer& end()
    {
        m_writer.end_attributes();
        return *this;
    }

    /// @brief Writes `/>` and finishes writing attributes.
    /// This function or `end()` shall be called exactly once prior to destruction of this
    /// writer.
    Basic_Attribute_Writer& end_empty()
    {
        if (m_unsafe_slash) {
            m_writer.m_out(u8' ');
        }
        m_writer.end_empty_tag_attributes();
        return *this;
    }

    /// @brief Destructor.
    /// A call to `end()` or `end_empty()` shall have been made prior to destruction.
    ~Basic_Attribute_Writer() noexcept(false)
    {
        // This indicates that end() or end_empty() weren't called.
        COWEL_ASSERT(!m_writer.m_in_attributes);
    }
};

struct To_Text_Sink_Consumer {
    Text_Sink& out;

    [[nodiscard]]
    To_Text_Sink_Consumer(Text_Sink& out)
        : out { out }
    {
    }

    void operator()(std::u8string_view str) const
    {
        out.write(str, Output_Language::html);
    }
    void operator()(char8_t c) const
    {
        out.write(c, Output_Language::html);
    }
};

using HTML_Writer = Basic_HTML_Writer<To_Text_Sink_Consumer>;
using Attribute_Writer = Basic_Attribute_Writer<To_Text_Sink_Consumer>;

} // namespace cowel

#endif
