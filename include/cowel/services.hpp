#ifndef COWEL_SERVICES_HPP
#define COWEL_SERVICES_HPP

#include <memory_resource>
#include <span>
#include <string_view>
#include <vector>

#include "ulight/ulight.hpp"

#include "cowel/util/annotation_span.hpp"
#include "cowel/util/assert.hpp"
#include "cowel/util/result.hpp"
#include "cowel/util/typo.hpp"

#include "cowel/diagnostic.hpp"
#include "cowel/fwd.hpp"

namespace cowel {

namespace detail {
using Suppress_Unused_Include_Annotation_Span = Annotation_Span<void>;
} // namespace detail

using Highlight_Span = ulight::Token;
using ulight::Highlight_Type;
using Highlight_Lang = ulight::Lang;

enum struct Syntax_Highlight_Error : Default_Underlying {
    unsupported_language,
    bad_code,
    other,
};

struct Syntax_Highlighter {

    /// @brief Returns a set of supported languages in no particular order.
    /// These languages can be used in `operator()` as hints.
    [[nodiscard]]
    virtual std::span<const std::u8string_view> get_supported_languages() const
        = 0;

    /// @brief Matches `language` against the set of supported language of the syntax highlighter.
    ///
    // This member function is useful for typo detection.
    [[nodiscard]]
    virtual Distant<std::u8string_view>
    match_supported_language(std::u8string_view language, std::pmr::memory_resource* memory) const
        = 0;

    /// @brief Applies syntax highlighting to the given `code`.
    /// Spans of highlighted source code are appended to `out`.
    /// If a failed result is returned,
    /// nothing is appended to `out`.
    /// @param out Where the spans are appended to.
    /// @param code The source code.
    /// @param language A language hint.
    /// This should be one of the languages returned by `get_supported_languages`.
    /// @param memory Additional memory.
    [[nodiscard]]
    virtual Result<void, Syntax_Highlight_Error> operator()(
        std::pmr::vector<Highlight_Span>& out,
        std::u8string_view code,
        std::u8string_view language,
        std::pmr::memory_resource* memory
    ) = 0;
};

/// @brief A `Syntax_Highlighter` that supports no languages.
struct No_Support_Syntax_Highlighter final : Syntax_Highlighter {

    [[nodiscard]]
    std::span<const std::u8string_view> get_supported_languages() const final
    {
        return {};
    }

    [[nodiscard]]
    Distant<std::u8string_view>
    match_supported_language(std::u8string_view, std::pmr::memory_resource*) const final
    {
        return {};
    }

    [[nodiscard]]
    Result<void, Syntax_Highlight_Error>
    operator()( //
        std::pmr::vector<Highlight_Span>&,
        std::u8string_view,
        std::u8string_view,
        std::pmr::memory_resource*
    ) final
    {
        return Syntax_Highlight_Error::unsupported_language;
    }
};

inline constinit No_Support_Syntax_Highlighter no_support_syntax_highlighter;

struct Document_Info {
    /// @brief ID by which the document is referenced elsewhere. For example, `Knuth01`.
    std::u8string_view id;
    /// @brief Title of the publication.
    std::u8string_view title;
    /// @brief The date of publication.
    std::u8string_view date;
    /// @brief The publisher.
    std::u8string_view publisher;
    /// @brief The primary (short) link to the document.
    std::u8string_view link;
    /// @brief The long link to the document.
    std::u8string_view long_link;
    /// @brief A link to issue tracking for the document.
    /// For example, a GitHub issue URL for WG21 papers.
    std::u8string_view issue_link;
    /// @brief The author(s).
    std::u8string_view author;
};

struct Stored_Document_Info {
    /// @brief The text storage for any dynamic strings in `info`.
    std::pmr::vector<char8_t> text;
    /// @brief Information about the document.
    Document_Info info;
};

struct Bibliography {
    [[nodiscard]]
    virtual const Document_Info* find(std::u8string_view id) const
        = 0;

    [[nodiscard]]
    virtual bool contains(std::u8string_view id) const
    {
        return find(id) != nullptr;
    }

    virtual bool insert(Stored_Document_Info&& info) = 0;

    virtual void clear() = 0;
};

struct File_Entry {
    File_Id id;
    std::u8string_view source;
    std::u8string_view name;
};

enum struct File_Load_Error : Default_Underlying {
    /// @brief Generic I/O error.
    error,
    /// @brief File was not found.
    not_found,
    /// @brief I/O (disk) error when reading the file.
    read_error,
    /// @brief No permissions to read the file.
    permissions,
    /// @brief File contains corrupted UTF-8 data.
    corrupted,
};

/// @brief This class loads files into memory and stores their text data persistently,
/// so that AST nodes can keep non-owning views into such text data.
struct File_Loader {
    /// @brief Loads a file into memory.
    /// If successful, returns a new file entry,
    /// which has non-owning views into the file's loaded source text and name.
    ///
    /// Note that the entry name must not be the same as `path`
    /// because there is no assurance that `path` will remain valid in the long term.
    [[nodiscard]]
    virtual Result<File_Entry, File_Load_Error> load(std::u8string_view path)
        = 0;
};

struct Always_Failing_File_Loader final : File_Loader {
    [[nodiscard]]
    Result<File_Entry, File_Load_Error> load(std::u8string_view) final
    {
        return File_Load_Error::error;
    }
};

inline constinit Always_Failing_File_Loader always_failing_file_loader;

struct Logger {
private:
    Severity m_min_severity;

public:
    [[nodiscard]]
    constexpr explicit Logger(Severity min_severity)
    {
        set_min_severity(min_severity);
    }

    [[nodiscard]]
    constexpr Severity get_min_severity() const
    {
        return m_min_severity;
    }

    constexpr void set_min_severity(Severity severity)
    {
        COWEL_ASSERT(severity <= Severity::none);
        m_min_severity = severity;
    }

    [[nodiscard]]
    constexpr bool can_log(Severity severity) const
    {
        return severity >= m_min_severity;
    }

    constexpr virtual void operator()(Diagnostic diagnostic) = 0;
};

struct Ignorant_Logger final : Logger {
    using Logger::Logger;

    void operator()(Diagnostic) final { }
};

inline constinit Ignorant_Logger ignorant_logger { Severity::none };

} // namespace cowel

#endif
