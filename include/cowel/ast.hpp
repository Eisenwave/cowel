#ifndef COWEL_AST_HPP
#define COWEL_AST_HPP

#include <concepts>
#include <cstddef>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "cowel/util/assert.hpp"
#include "cowel/util/meta.hpp"
#include "cowel/util/source_position.hpp"

#include "cowel/fwd.hpp"
#include "cowel/output_language.hpp"

namespace cowel::ast {
namespace detail {

using Suppress_Unused_Include_Source_Position_2 = Basic_File_Source_Span<void>;

}

struct Argument final {
private:
    File_Source_Span m_source_span;
    std::u8string_view m_source;
    std::pmr::vector<Content> m_content;
    File_Source_Span m_name_span;
    std::u8string_view m_name;

public:
    /// @brief Constructor for named arguments.
    [[nodiscard]]
    Argument(
        const File_Source_Span& source_span,
        std::u8string_view source,
        const File_Source_Span& name_span,
        std::u8string_view name,
        std::pmr::vector<ast::Content>&& children
    );

    /// @brief Constructor for positional (unnamed) arguments.
    [[nodiscard]]
    Argument(
        const File_Source_Span& source_span,
        std::u8string_view source,
        std::pmr::vector<ast::Content>&& children
    );

    Argument(Argument&&) noexcept;
    Argument(const Argument&);

    Argument& operator=(Argument&&) noexcept;
    Argument& operator=(const Argument&);

    ~Argument();

    [[nodiscard]]
    File_Source_Span get_source_span() const
    {
        return m_source_span;
    }

    [[nodiscard]]
    std::u8string_view get_source() const
    {
        return m_source;
    }

    [[nodiscard]]
    bool has_name() const
    {
        return !m_name_span.empty();
    }
    [[nodiscard]]
    File_Source_Span get_name_span() const
    {
        return m_name_span;
    }
    [[nodiscard]]
    std::u8string_view get_name() const
    {
        return m_name;
    }

    [[nodiscard]]
    std::pmr::vector<Content>& get_content() &;
    [[nodiscard]]
    std::span<const Content> get_content() const&;
    [[nodiscard]]
    std::pmr::vector<Content>&& get_content() &&;
};

struct Directive final {
private:
    File_Source_Span m_source_span;
    std::u8string_view m_source;
    std::u8string_view m_name;

    std::pmr::vector<Argument> m_arguments;
    std::pmr::vector<Content> m_content;

public:
    [[nodiscard]]
    Directive(
        const File_Source_Span& source_span,
        std::u8string_view source,
        std::u8string_view name,
        std::pmr::vector<Argument>&& args,
        std::pmr::vector<Content>&& block
    );

    Directive(Directive&&) noexcept;
    Directive(const Directive&);

    Directive& operator=(Directive&&) noexcept;
    Directive& operator=(const Directive&);

    ~Directive();

    [[nodiscard]]
    File_Source_Span get_source_span() const
    {
        return m_source_span;
    }

    [[nodiscard]]
    std::u8string_view get_source() const
    {
        return m_source;
    }

    [[nodiscard]]
    File_Source_Span get_name_span() const
    {
        return m_source_span.with_length(m_name.length());
    }

    [[nodiscard]]
    std::u8string_view get_name() const
    {
        return m_name;
    }

    [[nodiscard]]
    std::pmr::vector<Argument>& get_arguments();
    [[nodiscard]]
    std::span<const Argument> get_arguments() const;
    [[nodiscard]]
    std::pmr::vector<Content>& get_content();
    [[nodiscard]]
    std::span<Content const> get_content() const;
};

struct Text final {
private:
    File_Source_Span m_source_span;
    std::u8string_view m_source;

public:
    [[nodiscard]]
    Text(const File_Source_Span& source_span, std::u8string_view source);

    [[nodiscard]]
    File_Source_Span get_source_span() const
    {
        return m_source_span;
    }

    [[nodiscard]]
    std::u8string_view get_source() const
    {
        return m_source;
    }
};

struct Comment final {
private:
    File_Source_Span m_source_span;
    std::u8string_view m_source;
    std::size_t m_suffix_length;

public:
    [[nodiscard]]
    Comment(const File_Source_Span& source_span, std::u8string_view source);

    [[nodiscard]]
    File_Source_Span get_source_span() const
    {
        return m_source_span;
    }

    /// @brief Returns a string containing all the source characters comprising the comment,
    /// including the `\:` prefix and the LF/CRLF suffix, if any.
    [[nodiscard]]
    std::u8string_view get_source() const
    {
        return m_source;
    }

    [[nodiscard]]
    std::size_t get_suffix_length() const
    {
        return m_suffix_length;
    }

    /// @brief Returns the suffix of the comment.
    /// That is, an empty string (if the comment ends with EOF),
    /// or a string containing the terminating LF/CRLF.
    [[nodiscard]]
    std::u8string_view get_suffix() const
    {
        return m_source.substr(m_source.length() - m_suffix_length);
    }

    /// @brief Returns the text content of the comment, excluding the prefix and suffix.
    [[nodiscard]]
    std::u8string_view get_text() const
    {
        constexpr std::size_t prefix_length = 2; // \:
        return m_source.substr(prefix_length, m_source.length() - prefix_length - m_suffix_length);
    }
};

/// @brief An escape sequence, such as `\\{`, `\\}`, or `\\\\`.
struct Escaped final {
private:
    File_Source_Span m_source_span;
    std::u8string_view m_source;

public:
    [[nodiscard]]
    Escaped(const File_Source_Span& source_span, std::u8string_view source);

    [[nodiscard]]
    File_Source_Span get_source_span() const
    {
        return m_source_span;
    }

    /// @brief Returns a two-character substring of the `source`,
    /// where the first character is the escaping backslash,
    /// and the second character is the escaped character.
    [[nodiscard]]
    std::u8string_view get_source() const
    {
        return m_source;
    }

    /// @brief Returns the source span covering the escaped characters.
    [[nodiscard]]
    File_Source_Span get_escaped_span() const
    {
        return m_source_span.to_right(1);
    }

    /// @brief Returns the escaped characters.
    [[nodiscard]]
    std::u8string_view get_escaped() const
    {
        COWEL_DEBUG_ASSERT(m_source.size() >= 2);
        return m_source.substr(1);
    }
};

struct Generated final {
private:
    std::pmr::vector<char8_t> m_data;
    Output_Language m_type;

public:
    [[nodiscard]]
    explicit Generated(std::pmr::vector<char8_t>&& data, Output_Language type)
        : m_data { std::move(data) }
        , m_type { type }
    {
    }

    [[nodiscard]]
    constexpr Output_Language get_type() const
    {
        return m_type;
    }

    [[nodiscard]]
    constexpr std::span<char8_t> as_span()
    {
        return m_data;
    }

    [[nodiscard]]
    constexpr std::span<const char8_t> as_span() const
    {
        return m_data;
    }

    [[nodiscard]]
    constexpr std::u8string_view as_string() const
    {
        return { m_data.data(), m_data.size() };
    }

    [[nodiscard]]
    constexpr std::size_t size() const
    {
        return m_data.size();
    }

    [[nodiscard]]
    constexpr bool emtpy() const
    {
        return m_data.empty();
    }
};

using Content_Variant = std::variant<Directive, Text, Comment, Escaped, Generated>;

struct Content : Content_Variant {
    using Content_Variant::variant;
};

static_assert(std::is_move_constructible_v<Content>);
static_assert(std::is_move_constructible_v<Content>);
static_assert(std::is_copy_assignable_v<Content>);
static_assert(std::is_move_assignable_v<Content>);

template <typename T>
concept node = []<typename... Ts>(std::variant<Ts...>*) {
    return one_of<T, Ts...>;
}(static_cast<Content_Variant*>(nullptr));

template <typename T>
concept user_written = node<T> && !std::same_as<T, Generated>;

// Suppress false positive: https://github.com/llvm/llvm-project/issues/130745
// NOLINTBEGIN(readability-redundant-inline-specifier)
inline Argument::Argument(Argument&&) noexcept = default;
inline Argument::Argument(const Argument&) = default;
inline Argument& Argument::operator=(Argument&&) noexcept = default;
inline Argument& Argument::operator=(const Argument&) = default;
inline Argument::~Argument() = default;

inline Directive::Directive(Directive&&) noexcept = default;
inline Directive::Directive(const Directive&) = default;
inline Directive& Directive::operator=(Directive&&) noexcept = default;
inline Directive& Directive::operator=(const Directive&) = default;
inline Directive::~Directive() = default;
// NOLINTEND(readability-redundant-inline-specifier)

inline std::pmr::vector<Content>& Argument::get_content() &
{
    return m_content;
}
inline std::span<const Content> Argument::get_content() const&
{
    return m_content;
}
inline std::pmr::vector<Content>&& Argument::get_content() &&
{
    return std::move(m_content);
}

inline std::pmr::vector<Argument>& Directive::get_arguments()
{
    return m_arguments;
}
inline std::span<const Argument> Directive::get_arguments() const
{
    return m_arguments;
}

inline std::pmr::vector<Content>& Directive::get_content()
{
    return m_content;
}
inline std::span<const Content> Directive::get_content() const
{
    return m_content;
}

[[nodiscard]]
inline File_Source_Span get_source_span(const Content& node)
{
    return visit(
        [&]<typename T>(const T& v) -> File_Source_Span {
            if constexpr (one_of<T, Text, Escaped, Directive>) {
                return v.get_source_span();
            }
            else {
                return { {}, File_Id { 0 } };
            }
        },
        node
    );
}

[[nodiscard]]
inline std::u8string_view get_source(const Content& node)
{
    return visit(
        []<typename T>(const T& v) -> std::u8string_view {
            if constexpr (user_written<T>) {
                return v.get_source();
            }
            else {
                return {};
            }
        },
        node
    );
}

template <bool constant>
struct Visitor_Impl {
    using Argument_Type = const_if_t<Argument, constant>;
    using Text_Type = const_if_t<Text, constant>;
    using Comment_Type = const_if_t<Comment, constant>;
    using Escaped_Type = const_if_t<Escaped, constant>;
    using Directive_Type = const_if_t<Directive, constant>;
    using Generated_Type = const_if_t<Generated, constant>;
    using Content_Type = const_if_t<Content, constant>;

    void visit_arguments(Directive_Type& directive)
    {
        for (Argument_Type& arg : directive.get_arguments()) {
            this->visit(arg);
        }
    }

    void visit_children(Directive_Type& directive)
    {
        visit_arguments();
        visit_content_sequence(directive.get_content());
    }

    void visit_children(Argument_Type& argument)
    {
        visit_content_sequence(argument.get_content());
    }

    void visit_content(Content_Type& content)
    {
        std::visit([&](auto& c) { this->visit(c); }, content);
    }

    void visit_content_sequence(std::span<Content_Type> content)
    {
        for (Content_Type& c : content) {
            this->visit_content(c);
        }
    }

    virtual void visit(Argument_Type& argument) = 0;
    virtual void visit(Directive_Type& directive) = 0;
    virtual void visit(Generated_Type& generated) = 0;
    virtual void visit(Text_Type& text) = 0;
    virtual void visit(Comment_Type& text) = 0;
    virtual void visit(Escaped_Type& text) = 0;
};

using Visitor = Visitor_Impl<false>;
using Const_Visitor = Visitor_Impl<true>;

} // namespace cowel::ast

#endif
