#ifndef COWEL_POLICY_PARAGRAPH_SPLIT_HPP
#define COWEL_POLICY_PARAGRAPH_SPLIT_HPP

#include "cowel/util/char_sequence.hpp"

#include "cowel/policy/content_policy.hpp"
#include "cowel/policy/html.hpp"

#include "cowel/ast.hpp"
#include "cowel/context.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/output_language.hpp"
#include "cowel/util/char_sequence_ops.hpp"

namespace cowel {

struct Paragraph_Split_Policy final : virtual HTML_Content_Policy {
private:
    static constexpr std::u8string_view opening_tag = u8"<p>";
    static constexpr std::u8string_view closing_tag = u8"</p>";

    Paragraphs_State m_state;
    bool m_in_directive = false;
    std::pmr::memory_resource* m_memory;

public:
    [[nodiscard]]
    explicit Paragraph_Split_Policy(
        Text_Sink& parent,
        std::pmr::memory_resource* memory,
        Paragraphs_State initial_state = Paragraphs_State::outside
    )
        : Text_Sink { Output_Language::html }
        , Content_Policy { Output_Language::html }
        , HTML_Content_Policy { parent }
        , m_state { initial_state }
        , m_memory { memory }
    {
    }

    bool write(Char_Sequence8 chars, Output_Language language) override
    {
        if (m_in_directive || language != Output_Language::text) {
            return HTML_Content_Policy::write(chars, language);
        }
        if (chars.empty()) {
            return true;
        }
        const std::u8string_view sv = chars.as_string_view();
        split_into_paragraphs(sv.empty() ? to_string(chars, m_memory) : sv);
        return true;
    }

    [[nodiscard]]
    Content_Status consume(const ast::Text& t, Context&) override
    {
        if (m_in_directive) {
            write(t.get_source(), Output_Language::text);
            return Content_Status::ok;
        }
        split_into_paragraphs(t.get_source());
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
        if (text.empty()) {
            return Content_Status::ok;
        }
        enter_paragraph();
        HTML_Content_Policy::write(text, Output_Language::text);
        return Content_Status::ok;
    }

    [[nodiscard]]
    Content_Status consume(const ast::Directive& directive, Context& context) override
    {
        m_in_directive = true;
        const Content_Status result = apply_behavior(*this, directive, context);
        m_in_directive = false;
        return result;
    }

    [[nodiscard]]
    Content_Status consume(const ast::Generated& generated, Context&) override
    {
        write(generated.as_string(), generated.get_type());
        return Content_Status::ok;
    }

    void enter_paragraph()
    {
        if (m_state == Paragraphs_State::outside) {
            write(opening_tag, Output_Language::html);
            m_state = Paragraphs_State::inside;
        }
    }

    void leave_paragraph()
    {
        if (m_state == Paragraphs_State::inside) {
            write(closing_tag, Output_Language::html);
            m_state = Paragraphs_State::outside;
        }
    }

    void transition(Paragraphs_State state)
    {
        switch (state) {
        case Paragraphs_State::inside: enter_paragraph(); break;
        case Paragraphs_State::outside: leave_paragraph(); break;
        }
    }

private:
    void split_into_paragraphs(std::u8string_view text);
};

} // namespace cowel

#endif
