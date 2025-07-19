#ifndef COWEL_OUTPUT_LANGUAGE_HPP
#define COWEL_OUTPUT_LANGUAGE_HPP

#include "cowel/fwd.hpp"

namespace cowel {

enum struct Output_Language : Default_Underlying {
    /// @brief No output.
    /// However, processing of side effects still happens.
    none,
    /// @brief Plaintext output.
    /// This is used e.g. in the input of `\cow_char_by_name`,
    /// in arguments that turn into HTML attributes, and other places
    /// where markup cannot be provided.
    text,
    /// @brief HTML output.
    html,
};

}

#endif
