#ifndef COWEL_POLICY_CAPTURE_HPP
#define COWEL_POLICY_CAPTURE_HPP

#include <vector>

#include "cowel/util/char_sequence.hpp"

#include "cowel/policy/content_policy.hpp"

#include "cowel/output_language.hpp"

namespace cowel {

/// @brief Content policy which redirects the output of another content policy
/// into a `std::pmr::vector`.
struct Capturing_Ref_Text_Sink : virtual Text_Sink {
private:
    std::pmr::vector<char8_t>& m_out;

public:
    [[nodiscard]]
    explicit Capturing_Ref_Text_Sink(std::pmr::vector<char8_t>& out, Output_Language language)
        : Text_Sink { language }
        , m_out { out }
    {
    }

    [[nodiscard]]
    std::pmr::vector<char8_t>& operator*() &
    {
        return m_out;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
    bool write(Char_Sequence8 chars, Output_Language) override
    {
        const std::size_t initial_size = m_out.size();
        m_out.resize(initial_size + chars.size());
        chars.extract(std::span { m_out }.subspan(initial_size));
        return true;
    }
};

/// @brief Content policy which redirects the output of another content policy
/// into a `std::pmr::vector`.
struct Vector_Text_Sink : virtual Text_Sink {
private:
    std::pmr::vector<char8_t> m_out;

public:
    [[nodiscard]]
    explicit Vector_Text_Sink(Output_Language language, std::pmr::memory_resource* memory)
        : Text_Sink { language }
        , m_out { memory }
    {
    }

    [[nodiscard]]
    std::pmr::vector<char8_t>& operator*() &
    {
        return m_out;
    }

    [[nodiscard]]
    const std::pmr::vector<char8_t>& operator*() const&
    {
        return m_out;
    }

    [[nodiscard]]
    std::pmr::vector<char8_t>&& operator*() &&
    {
        return std::move(m_out);
    }

    [[nodiscard]]
    const std::pmr::vector<char8_t>&& operator*() const&&
    {
        return std::move(m_out);
    }

    // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
    bool write(Char_Sequence8 chars, Output_Language) override
    {
        const std::size_t initial_size = m_out.size();
        m_out.resize(initial_size + chars.size());
        chars.extract(std::span { m_out }.subspan(initial_size));
        COWEL_DEBUG_ASSERT(chars.empty());
        return true;
    }
};

} // namespace cowel

#endif
