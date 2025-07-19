#ifndef COWEL_DIRECTIVE_DISPLAY_HPP
#define COWEL_DIRECTIVE_DISPLAY_HPP

#include "cowel/fwd.hpp"

namespace cowel {

/// @brief Specifies how a directive should be displayed.
/// This is used to configure in certain directives
/// how they interact with paragraph splitting.
enum struct Directive_Display : Default_Underlying {
    /// @brief Nothing is displayed.
    none,
    /// @brief The directive is a block, such as `\\h1` or `\\codeblock`.
    /// Such directives are not integrated into other paragraphs or surround text.
    block,
    /// @brief The directive is inline, such as `\\b` or `\\code`.
    /// This means that it will be displayed within paragraphs and as part of other text.
    in_line,
};

} // namespace cowel

#endif
