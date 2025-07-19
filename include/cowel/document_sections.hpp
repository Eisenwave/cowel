#ifndef COWEL_DOCUMENT_WRITER_HPP
#define COWEL_DOCUMENT_WRITER_HPP

#include <cstddef>
#include <map>
#include <memory_resource>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cowel/util/assert.hpp"
#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/chars.hpp"
#include "cowel/util/transparent_comparison.hpp"

#include "cowel/policy/capture.hpp"
#include "cowel/policy/html.hpp"

#include "cowel/fwd.hpp"

namespace cowel {

namespace detail {

using Suppress_Unused_Include_Transparent_Less = Basic_Transparent_String_View_Less<void>;

} // namespace detail

struct Section_Content {
private:
    std::pmr::vector<char8_t> m_data;
    Capturing_Ref_Text_Sink m_sink { m_data, Output_Language::html };
    HTML_Content_Policy m_policy { m_sink };

public:
    [[nodiscard]]
    explicit Section_Content(std::pmr::memory_resource* memory)
        : m_data { memory }
    {
    }

    Section_Content(const Section_Content&) = delete;
    Section_Content& operator=(const Section_Content&) = delete;

    Section_Content(Section_Content&&) = default;
    Section_Content& operator=(Section_Content&&) = delete;

    ~Section_Content() = default;

    [[nodiscard]]
    std::u8string_view text() const
    {
        return as_u8string_view(m_data);
    }

    [[nodiscard]]
    std::pmr::vector<char8_t>& output() &
    {
        return m_data;
    }

    [[nodiscard]]
    Content_Policy& policy()
    {
        return m_policy;
    }
    [[nodiscard]]
    const Content_Policy& policy() const
    {
        return m_policy;
    }
};

struct Document_Sections {
public:
    // The choice of std::map over std::unordered_map is deliberate:
    // We require iterator and reference stability in some cases,
    // and std::unordered_map can invalidate iterators and references on rehashing.
    using map_type
        = std::pmr::map<std::pmr::u8string, Section_Content, Transparent_String_View_Less8>;
    using entry_type = map_type::value_type;

    struct [[nodiscard]] Scoped_Section {
    private:
        friend Document_Sections;

        Document_Sections& self;
        entry_type& old;

        Scoped_Section(Document_Sections& self, entry_type& old)
            : self { self }
            , old { old }
        {
        }

    public:
        Scoped_Section(const Scoped_Section&) = delete;
        Scoped_Section& operator=(const Scoped_Section&) = delete;

        ~Scoped_Section()
        {
            self.m_current = &old;
        }
    };

private:
    map_type m_sections;
    entry_type* m_current = &*m_sections.emplace(std::pmr::u8string {}, get_memory()).first;

public:
    [[nodiscard]]
    explicit Document_Sections(std::pmr::memory_resource* memory)
        : m_sections { memory }
    {
    }

    Document_Sections(const Document_Sections&) = delete;
    Document_Sections& operator=(const Document_Sections&) = delete;
    ~Document_Sections() = default;

    /// @brief Returns the memory resource that this object was constructed with.
    [[nodiscard]]
    std::pmr::memory_resource* get_memory() const
    {
        return m_sections.get_allocator().resource();
    }

    /// @brief Returns a pointer to the section named `section` if one exists;
    /// otherwise returns null.
    ///
    /// No allocations are performed.
    [[nodiscard]]
    entry_type* find(std::u8string_view section)
    {
        const auto existing_iter = m_sections.find(section);
        return existing_iter != m_sections.end() ? &*existing_iter : nullptr;
    }

    /// @brief Returns a pointer to the section named `section` if one exists;
    /// otherwise returns null.
    ///
    /// No allocations are performed.
    [[nodiscard]]
    const entry_type* find(std::u8string_view section) const
    {
        const auto existing_iter = m_sections.find(section);
        return existing_iter != m_sections.end() ? &*existing_iter : nullptr;
    }

    /// @brief Creates a new section named `section` if one doesn't exist yet.
    /// Returns a reference to the new or existing one.
    ///
    /// Allocates a new key string if the section doesn't exist yet.
    entry_type& make(std::u8string_view section)
    {
        if (entry_type* const existing = find(section)) {
            return *existing;
        }
        const auto [iter, success]
            = m_sections.emplace(std::pmr::u8string { section, get_memory() }, get_memory());
        COWEL_ASSERT(success);
        return *iter;
    }

    /// @brief Like the overload taking `std::u8string_view`,
    /// but if the section does not yet exist,
    /// doesn't allocate a new string for the key name.
    entry_type& make(std::pmr::u8string&& section)
    {
        if (entry_type* const existing = find(section)) {
            return *existing;
        }
        const auto [iter, success] = m_sections.emplace(std::move(section), get_memory());
        COWEL_ASSERT(success);
        return *iter;
    }

    /// @brief Sets the current section to the given `section` if one already exists,
    /// and returns a pointer to the section entry;
    /// otherwise returns null.
    ///
    /// No allocations are performed.
    [[nodiscard]]
    entry_type* try_go_to(std::u8string_view section)
    {
        const auto existing_iter = m_sections.find(section);
        entry_type* const result = existing_iter != m_sections.end() ? &*existing_iter : nullptr;
        m_current = result;
        return result;
    }

    /// @brief Sets the current section to an existing one or a newly created one named `section`,
    /// and returns a reference to that section.
    ///
    /// Allocates a new key string if a section doesn't exist yet.
    entry_type& go_to(std::u8string_view section)
    {
        entry_type& result = make(section);
        m_current = &result;
        return result;
    }

    /// @brief Like the overload taking `std::u8string_view`,
    /// but if the section does not yet exist,
    /// doesn't allocate a new string for the key name.
    entry_type& go_to(std::pmr::u8string&& section)
    {
        entry_type& result = make(std::move(section));
        m_current = &result;
        return result;
    }

    /// @brief Calls `go_to(section)` and returns a `Scoped_Section` which,
    /// upon destruction, sets the current section to the section prior to `go_to`.
    ///
    /// This is useful for temporarily writing content to a different section.
    Scoped_Section go_to_scoped(std::u8string_view section)
    {
        entry_type* const old = m_current;
        go_to(section);
        return Scoped_Section { *this, *old };
    }

    /// @brief Calls `go_to(section)` and returns a `Scoped_Section` which,
    /// upon destruction, sets the current section to the section prior to `go_to`.
    ///
    /// This is useful for temporarily writing content to a different section.
    Scoped_Section go_to_scoped(std::pmr::u8string&& section)
    {
        entry_type* const old = m_current;
        go_to(std::move(section));
        return Scoped_Section { *this, *old };
    }

    /// @brief Returns a reference to the current section.
    [[nodiscard]]
    entry_type& current() noexcept
    {
        return *m_current;
    }

    /// @brief Returns a reference to the current section.
    [[nodiscard]]
    const entry_type& current() const noexcept
    {
        return *m_current;
    }

    [[nodiscard]]
    std::u8string_view current_name() const noexcept
    {
        return current().first;
    }

    [[nodiscard]]
    std::pmr::vector<char8_t>& current_output() noexcept
    {
        return current().second.output();
    }

    /// @brief Returns the output characters of the current section.
    [[nodiscard]]
    Content_Policy& current_policy() noexcept
    {
        return current().second.policy();
    }
};

/// @brief Appends a "section reference" to `out`.
/// This works by mapping the length onto a code point within the
/// Supplementary Private Use Area-A block,
/// and encoding that as UTF-8.
/// The given name is then appended as is.
/// @returns `name.size() <= 65635`.
/// A name beyond that size cannot be encoded as a section reference.
inline bool reference_section(Content_Policy& out, Char_Sequence8 name)
{
    constexpr std::size_t max_length = supplementary_pua_a_max - supplementary_pua_a_min;
    if (name.size() > max_length) {
        return false;
    }

    const char32_t first_point = supplementary_pua_a_min + char32_t(name.size());
    out.write(make_char_sequence(first_point), Output_Language::html);
    out.write(name, Output_Language::html);
    return true;
}

} // namespace cowel

#endif
