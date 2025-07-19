#ifndef COWEL_POLICY_SYNTAX_HIGHLIGHT_HPP
#define COWEL_POLICY_SYNTAX_HIGHLIGHT_HPP

#include <string_view>
#include <vector>

#include "cowel/util/char_sequence.hpp"

#include "cowel/policy/content_policy.hpp"

#include "cowel/ast.hpp"
#include "cowel/context.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/output_language.hpp"
#include "cowel/services.hpp"

namespace cowel {

struct Syntax_Highlight_Policy : virtual Content_Policy {
protected:
    enum struct Span_Type : bool {
        html,
        highlight,
    };

    struct Output_Span {
        Span_Type type;
        std::size_t begin;
        std::size_t length;
    };

    std::pmr::vector<Output_Span> m_spans;
    std::pmr::vector<char8_t> m_html_text;
    std::pmr::vector<char8_t> m_highlighted_text;
    std::u8string_view m_suffix;

public:
    [[nodiscard]]
    explicit Syntax_Highlight_Policy(
        std::pmr::memory_resource* memory,
        std::u8string_view prefix = {},
        std::u8string_view suffix = {}
    )
        : Text_Sink { Output_Language::html }
        , Content_Policy { Output_Language::html }
        , m_spans { memory }
        , m_html_text { memory }
        , m_highlighted_text { memory }
        , m_suffix { suffix }
    {
        m_highlighted_text.insert(m_highlighted_text.end(), prefix.begin(), prefix.end());
    }

    bool write(Char_Sequence8 chars, Output_Language language) override;

    [[nodiscard]]
    Content_Status consume(const ast::Text& text, Context&) override
    {
        write(text.get_source(), Output_Language::text);
        return Content_Status::ok;
    }
    [[nodiscard]]
    Content_Status consume(const ast::Comment&, Context&) override
    {
        return Content_Status::ok;
    }
    [[nodiscard]]
    Content_Status consume(const ast::Escaped& escape, Context&) override
    {
        const std::u8string_view text = expand_escape(escape);
        write(text, Output_Language::text);
        return Content_Status::ok;
    }
    [[nodiscard]]
    Content_Status consume(const ast::Directive& directive, Context& context) override
    {
        return apply_behavior(*this, directive, context);
    }
    [[nodiscard]]
    Content_Status consume(const ast::Generated& generated, Context&) override
    {
        write(generated.as_string(), generated.get_type());
        return Content_Status::ok;
    }

    /// @brief Writes pure HTML content to `out`,
    /// consisting of the received HTML content,
    /// interleaved with syntax highlighting HTML (`<h->...</h->`)
    /// formed from any incoming plaintext.
    /// @param out The sink to write to.
    /// @param context The context.
    /// @param language The syntax highlighting language.
    /// Under the hood, ulight is used, so this needs to be one of the short names
    /// that ulight supports.
    Result<void, Syntax_Highlight_Error>
    write_highlighted(Text_Sink& out, Context& context, std::u8string_view language);
};

} // namespace cowel

#endif
