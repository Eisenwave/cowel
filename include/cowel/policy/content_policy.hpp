#ifndef COWEL_CONTENT_POLICY_HPP
#define COWEL_CONTENT_POLICY_HPP

#include "cowel/util/char_sequence.hpp"

#include "cowel/ast.hpp"
#include "cowel/content_status.hpp"
#include "cowel/fwd.hpp"
#include "cowel/output_language.hpp"

namespace cowel {

struct Text_Sink {
private:
    Output_Language m_language;

public:
    [[nodiscard]]
    explicit Text_Sink(Output_Language language)
        : m_language { language }
    {
    }

    /// @brief Returns the "native" language of the content policy.
    /// That is, the language in which it expects its input content to be.
    /// Most directives can ignore this information,
    /// but some directives like `\cow_char_by_entity` have different output based on the language.
    [[nodiscard]]
    Output_Language get_language() const noexcept
    {
        return m_language;
    }

    /// @brief Attempts to write `str` in the specified `language`.
    /// @returns `true` iff the language was accepted.
    /// `get_language()` is always accepted.
    virtual bool write(Char_Sequence8 str, Output_Language language) = 0;
};

/// @brief A content policy can receive different kinds of content as well as text,
/// and controls how these are processed.
///
/// A content policy has a single target language which it expects its given content to be in.
/// If given content in different languages, it can choose to ignore it,
/// transform it into its expected format, etc.
///
/// Furthermore, when a content policy receives different kinds of AST content,
/// it decides how those should be processed.
/// It can even choose to turn comments into text, ignore directives entirely, etc.
struct Content_Policy : virtual Text_Sink {

    [[nodiscard]]
    explicit Content_Policy(Output_Language language)
        : Text_Sink { language }
    {
    }

    [[nodiscard]]
    virtual Content_Status consume(const ast::Text& text, Context& context)
        = 0;
    [[nodiscard]]
    virtual Content_Status consume(const ast::Comment& comment, Context& context)
        = 0;
    [[nodiscard]]
    virtual Content_Status consume(const ast::Escaped& escape, Context& context)
        = 0;
    [[nodiscard]]
    virtual Content_Status consume(const ast::Directive& directive, Context& context)
        = 0;
    [[nodiscard]]
    virtual Content_Status consume(const ast::Generated& generated, Context&)
        = 0;

    [[nodiscard]]
    Content_Status consume_content(const ast::Content& content, Context& context)
    {
        return std::visit([&](const auto& c) { return consume(c, context); }, content);
    }
};

} // namespace cowel

#endif
