#ifndef COWEL_POLICY_UNPROCESSED_HPP
#define COWEL_POLICY_UNPROCESSED_HPP

#include "cowel/util/assert.hpp"

#include "cowel/policy/plaintext.hpp"

#include "cowel/fwd.hpp"
#include "cowel/output_language.hpp"

namespace cowel {

struct Unprocessed_Content_Policy : virtual Plaintext_Content_Policy {
private:
public:
    [[nodiscard]]
    explicit Unprocessed_Content_Policy(Text_Sink& parent)
        : Text_Sink { Output_Language::text }
        , Content_Policy { Output_Language::text }
        , Plaintext_Content_Policy { parent }

    {
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
    Content_Status consume(const ast::Directive& directive, Context&) override
    {
        write(directive.get_source(), Output_Language::text);
        return Content_Status::ok;
    }
    [[nodiscard]]
    Content_Status consume(const ast::Generated&, Context&) override
    {
        COWEL_ASSERT_UNREACHABLE(
            u8"Generated content within Unprocessed_Content_Policy should be impossible."
        );
        return Content_Status::fatal;
    }
};

} // namespace cowel

#endif
