#ifndef COWEL_POLICY_PLAINTEXT_HPP
#define COWEL_POLICY_PLAINTEXT_HPP

#include <string_view>

#include "cowel/policy/content_policy.hpp"

#include "cowel/directive_processing.hpp"
#include "cowel/fwd.hpp"

namespace cowel {

struct Plaintext_Content_Policy : virtual Content_Policy {
protected:
    Text_Sink& m_parent;

public:
    [[nodiscard]]
    explicit Plaintext_Content_Policy(Text_Sink& parent)
        : Text_Sink { Output_Language::text }
        , Content_Policy { Output_Language::text }
        , m_parent { parent }
    {
    }

    bool write(Char_Sequence8 chars, Output_Language language) override
    {
        COWEL_ASSERT(language != Output_Language::none);
        return language == Output_Language::text && m_parent.write(chars, language);
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
