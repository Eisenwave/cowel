#ifndef COWEL_DIRECTIVE_BEHAVIOR_HPP
#define COWEL_DIRECTIVE_BEHAVIOR_HPP

#include "cowel/ast.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/fwd.hpp"

namespace cowel {

/// @brief Implements behavior that one or multiple directives should have.
struct Directive_Behavior {
    constexpr Directive_Behavior() = default;

    [[nodiscard]]
    virtual Content_Status operator()(Content_Policy& out, const ast::Directive&, Context&) const
        = 0;
};

} // namespace cowel

#endif
