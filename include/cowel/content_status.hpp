#ifndef COWEL_CONTENT_STATUS_HPP
#define COWEL_CONTENT_STATUS_HPP

#include <concepts>
#include <string_view>

#include "cowel/fwd.hpp"
#include "cowel/util/assert.hpp"

namespace cowel {

enum struct Content_Status : Default_Underlying {
    /// @brief Content could be produced successfully,
    /// and generation should continue.
    ok,
    /// @brief Content generation was aborted (due to a break/return-like construct).
    /// However, this is not an error.
    abandon,
    /// @brief An error occurred,
    /// but that error is recoverable.
    error,
    /// @brief An error occurred,
    /// but processing continued until `abandon` was returned.
    /// This is effectively a combination of `error` and `abandon`.
    error_abandon,
    /// @brief An unrecoverable error occurred,
    /// and generation of the document as a whole has to be abandoned.
    fatal,
};

[[nodiscard]]
constexpr std::u8string_view status_name(Content_Status status)
{
    using enum Content_Status;
    switch (status) {
        COWEL_ENUM_STRING_CASE8(ok);
        COWEL_ENUM_STRING_CASE8(abandon);
        COWEL_ENUM_STRING_CASE8(error);
        COWEL_ENUM_STRING_CASE8(error_abandon);
        COWEL_ENUM_STRING_CASE8(fatal);
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid status.");
}

/// @brief Returns `true` iff `status` is a non-error status.
[[nodiscard]]
constexpr bool status_is_ok(Content_Status status) noexcept
{
    return status < Content_Status::error;
}

/// @brief Returns `true` iff `status` is an error status.
[[nodiscard]]
constexpr bool status_is_error(Content_Status status) noexcept
{
    return status >= Content_Status::error;
}

/// @brief Returns `true` iff `status` indicates that control flow should continue,
/// regardless whether the status is successful or an error.
[[nodiscard]]
constexpr bool status_is_continue(Content_Status status) noexcept
{
    return status == Content_Status::ok || status == Content_Status::error;
}

/// @brief Retruns `true` iff `status` indicates that control flow should break,
/// regardless whether the status is successful or an error.
[[nodiscard]]
constexpr bool status_is_break(Content_Status status) noexcept
{
    return status == Content_Status::abandon //
        || status == Content_Status::error_abandon //
        || status == Content_Status::fatal;
}

[[nodiscard]]
constexpr Content_Status status_concat(Content_Status first, Content_Status second) noexcept
{
    return status_is_break(first)           ? first
        : first == Content_Status::ok       ? second
        : second == Content_Status::ok      ? Content_Status::error
        : second == Content_Status::abandon ? Content_Status::error_abandon
                                            : second;
}

template <std::same_as<Content_Status>... S>
[[nodiscard]]
constexpr Content_Status status_concat(S... s) noexcept
{
    Content_Status result = Content_Status::ok;
    ((result = status_concat(result, s)), ...);
    return result;
}

} // namespace cowel

#endif
