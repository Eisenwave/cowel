#ifndef COWEL_POLICY_HTML_LITERAL
#define COWEL_POLICY_HTML_LITERAL

#include "cowel/policy/content_policy.hpp"
#include "cowel/policy/html.hpp"

#include "cowel/fwd.hpp"
#include "cowel/output_language.hpp"

namespace cowel {

struct HTML_Literal_Content_Policy : virtual HTML_Content_Policy {

    [[nodiscard]]
    explicit HTML_Literal_Content_Policy(Text_Sink& parent)
        : Text_Sink { Output_Language::text }
        , Content_Policy { Output_Language::text }
        , HTML_Content_Policy { parent }
    {
    }

    bool write(Char_Sequence8 chars, Output_Language language) override
    {
        COWEL_ASSERT(language != Output_Language::none);
        return language == Output_Language::text && m_parent.write(chars, Output_Language::html);
    }

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
};

} // namespace cowel

#endif
