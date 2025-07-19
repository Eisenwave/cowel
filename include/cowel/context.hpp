#ifndef COWEL_CONTEXT_HPP
#define COWEL_CONTEXT_HPP

#include <memory_resource>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cowel/util/assert.hpp"
#include "cowel/util/transparent_comparison.hpp"
#include "cowel/util/typo.hpp"

#include "cowel/ast.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/document_sections.hpp"
#include "cowel/fwd.hpp"
#include "cowel/services.hpp"

namespace cowel {

namespace detail {

// We only use the aliases from fwd.hpp, but we need the definition of
// `Basic_Transparent_String_View_Equals` to actually make this usable.
using Suppress_Unused_Include_Transparent_Equals = Basic_Transparent_String_View_Equals<void>;

} // namespace detail

struct Name_Resolver {
    [[nodiscard]]
    virtual Distant<std::u8string_view>
    fuzzy_lookup_name(std::u8string_view name, Context& context) const = 0;

    [[nodiscard]]
    virtual Directive_Behavior* operator()(std::u8string_view name, Context& context) const
        = 0;
};

struct Referred {
    std::u8string_view mask_html;
};

/// @brief Stores contextual information during document processing.
struct Context {
public:
    using char_type = char8_t;
    using string_type = std::pmr::u8string;
    using string_view_type = std::u8string_view;

    using Variable_Map = std::pmr::unordered_map<
        string_type,
        string_type,
        Transparent_String_View_Hash8,
        Transparent_String_View_Equals8>;
    using Macro_Map = std::pmr::unordered_map<
        std::pmr::u8string,
        ast::Directive,
        Transparent_String_View_Hash8,
        Transparent_String_View_Equals8>;
    using ID_Map = std::pmr::unordered_map<
        std::pmr::u8string,
        Referred,
        Transparent_String_View_Hash8,
        Transparent_String_View_Equals8>;

private:
    /// @brief Additional memory used during processing.
    std::pmr::memory_resource* m_memory;
    std::pmr::memory_resource* m_transient_memory;
    /// @brief JSON source code of the syntax highlighting theme.
    string_view_type m_highlight_theme_source;
    /// @brief A list of (non-null) name resolvers.
    /// Whenever directives have to be looked up,
    /// these are processed from
    /// last to first (i.e. from most recently added) to determine which
    /// `Directive_Behavior` should handle a given directive.
    std::pmr::vector<const Name_Resolver*> m_name_resolvers { m_transient_memory };
    /// @brief Map of ids (as in, `id` attributes in HTML elements)
    /// to information about the reference.
    ID_Map m_id_references { m_transient_memory };
    Macro_Map m_macros { m_transient_memory };
    Directive_Behavior* m_error_behavior;

    File_Loader& m_file_loader;
    Logger& m_logger;
    Syntax_Highlighter& m_syntax_highlighter;
    Bibliography& m_bibliography;

    Document_Sections m_sections { m_memory };
    Variable_Map m_variables { m_memory };

public:
    /// @brief Constructs a new context.
    /// @param source The source code.
    /// @param emit_diagnostic Called when a diagnostic is emitted.
    /// @param min_diagnostic_level The minimum level of diagnostics that are emitted.
    /// @param error_behavior The behavior to be used when directive processing encounters an error.
    /// May be null.
    /// @param persistent_memory Additional memory which persists beyond the destruction
    /// of the context.
    /// This is used e.g. for the creation of diagnostic messages,
    /// for directive behavior that requires storing information between passes, etc.
    /// @param transient_memory Additional memory which does not persist beyond the
    /// destruction of the context.
    explicit Context(
        string_view_type highlight_theme_source,
        Directive_Behavior* error_behavior,
        File_Loader& file_loader,
        Logger& logger,
        Syntax_Highlighter& highlighter,
        Bibliography& bibliography,
        std::pmr::memory_resource* persistent_memory,
        std::pmr::memory_resource* transient_memory
    )
        : m_memory { persistent_memory }
        , m_transient_memory { transient_memory }
        , m_highlight_theme_source { highlight_theme_source }
        , m_error_behavior { error_behavior }
        , m_file_loader { file_loader }
        , m_logger { logger }
        , m_syntax_highlighter { highlighter }
        , m_bibliography { bibliography }
    {
    }

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    ~Context() = default;

    [[nodiscard]]
    File_Loader& get_file_loader()
    {
        return m_file_loader;
    }
    [[nodiscard]]
    const File_Loader& get_file_loader() const
    {
        return m_file_loader;
    }

    [[nodiscard]]
    Logger& get_logger()
    {
        return m_logger;
    }
    [[nodiscard]]
    const Logger& get_logger() const
    {
        return m_logger;
    }

    [[nodiscard]]
    Syntax_Highlighter& get_highlighter()
    {
        return m_syntax_highlighter;
    }
    [[nodiscard]]
    const Syntax_Highlighter& get_highlighter() const
    {
        return m_syntax_highlighter;
    }

    [[nodiscard]]
    Bibliography& get_bibliography()
    {
        return m_bibliography;
    }
    [[nodiscard]]
    const Bibliography& get_bibliography() const
    {
        return m_bibliography;
    }

    [[nodiscard]]
    Variable_Map& get_variables()
    {
        return m_variables;
    }

    [[nodiscard]]
    const Variable_Map& get_variables() const
    {
        return m_variables;
    }

    [[nodiscard]]
    string_type* get_variable(string_view_type key)
    {
        const auto it = m_variables.find(key);
        return it == m_variables.end() ? nullptr : &it->second;
    }
    [[nodiscard]]
    const string_type* get_variable(string_view_type key) const
    {
        const auto it = m_variables.find(key);
        return it == m_variables.end() ? nullptr : &it->second;
    }

    [[nodiscard]]
    Document_Sections& get_sections()
    {
        return m_sections;
    }

    [[nodiscard]]
    const Document_Sections& get_sections() const
    {
        return m_sections;
    }

    /// @brief Returns a memory resource that the `Context` has been constructed with.
    /// This may possibly persist beyond the destruction of the context.
    ///
    /// Note that the context is destroyed after the preprocessing pass,
    /// so this may be useful to retain some information between passes (e.g. caches).
    [[nodiscard]]
    std::pmr::memory_resource* get_persistent_memory() const
    {
        return m_memory;
    }

    /// @brief Returns a memory resource that is destroyed with this object.
    /// Note that contexts are destroyed after each pass, so this should only be used for
    /// temporary memory.
    [[nodiscard]]
    std::pmr::memory_resource* get_transient_memory() const
    {
        return m_transient_memory;
    }

    [[nodiscard]]
    Directive_Behavior* get_error_behavior() const
    {
        return m_error_behavior;
    }

    [[nodiscard]]
    string_view_type get_highlight_theme_source() const
    {
        return m_highlight_theme_source;
    }

    /// @brief Returns the inclusive minimum level of diagnostics that are currently emitted.
    /// This may be `none`, in which case no diagnostic are emitted.
    [[nodiscard]]
    Severity get_min_diagnostic_level() const
    {
        return m_logger.get_min_severity();
    }

    /// @brief Equivalent to `get_min_diagnostic_level() >= severity`.
    [[nodiscard]]
    bool emits(Severity severity) const
    {
        return m_logger.can_log(severity);
    }

    void emit(Diagnostic diagnostic)
    {
        COWEL_ASSERT(emits(diagnostic.severity));
        m_logger(diagnostic);
    }

    void emit(
        Severity severity,
        string_view_type id,
        const File_Source_Span& location,
        Char_Sequence8 message
    )
    {
        emit({ severity, id, location, message });
    }

    void emit_debug(string_view_type id, const File_Source_Span& location, Char_Sequence8 message)
    {
        emit(Severity::debug, id, location, message);
    }

    void
    emit_soft_warning(string_view_type id, const File_Source_Span& location, Char_Sequence8 message)
    {
        emit(Severity::soft_warning, id, location, message);
    }

    void emit_warning(string_view_type id, const File_Source_Span& location, Char_Sequence8 message)
    {
        emit(Severity::warning, id, location, message);
    }

    void emit_error(string_view_type id, const File_Source_Span& location, Char_Sequence8 message)
    {
        emit(Severity::error, id, location, message);
    }

    void try_emit(Diagnostic diagnostic)
    {
        if (emits(diagnostic.severity)) {
            emit(diagnostic);
        }
    }

    void try_emit(
        Severity severity,
        string_view_type id,
        const File_Source_Span& location,
        Char_Sequence8 message
    )
    {
        if (emits(severity)) {
            emit({ severity, id, location, message });
        }
    }

    void try_debug(string_view_type id, const File_Source_Span& location, Char_Sequence8 message)
    {
        try_emit(Severity::debug, id, location, message);
    }

    void
    try_soft_warning(string_view_type id, const File_Source_Span& location, Char_Sequence8 message)
    {
        try_emit(Severity::soft_warning, id, location, message);
    }

    void try_warning(string_view_type id, const File_Source_Span& location, Char_Sequence8 message)
    {
        try_emit(Severity::warning, id, location, message);
    }

    void try_error(string_view_type id, const File_Source_Span& location, Char_Sequence8 message)
    {
        try_emit(Severity::error, id, location, message);
    }

    void add_resolver(const Name_Resolver& resolver)
    {
        m_name_resolvers.push_back(&resolver);
    }

    /// @brief Finds a directive behavior using `name_resolvers` in reverse order.
    /// @param name the name of the directive
    /// @return the behavior for the given name, or `nullptr` if none could be found
    [[nodiscard]]
    Directive_Behavior* find_directive(string_view_type name);

    /// @brief Equivalent to `find_directive(directive.get_name(source))`.
    [[nodiscard]]
    Directive_Behavior* find_directive(const ast::Directive& directive);

    [[nodiscard]]
    const Referred* find_id(std::u8string_view id) const
    {
        const auto it = m_id_references.find(id);
        return it == m_id_references.end() ? nullptr : &it->second;
    }

    [[nodiscard]]
    bool emplace_id(std::pmr::u8string&& id, const Referred& referred)
    {
        const auto [it, success] = m_id_references.try_emplace(std::move(id), referred);
        return success;
    }

    [[nodiscard]]
    const ast::Directive* find_macro(std::u8string_view id) const
    {
        const auto it = m_macros.find(id);
        return it == m_macros.end() ? nullptr : &it->second;
    }

    [[nodiscard]]
    bool emplace_macro(std::pmr::u8string&& id, ast::Directive&& definition_directive)
    {
        const auto [it, success]
            = m_macros.try_emplace(std::move(id), std::move(definition_directive));
        return success;
    }
};

struct Macro_Name_Resolver final : Name_Resolver {
    Directive_Behavior& m_macro_behavior;

    Macro_Name_Resolver(Directive_Behavior& macro_behavior)
        : m_macro_behavior { macro_behavior }
    {
    }

    Distant<std::u8string_view> fuzzy_lookup_name(std::u8string_view, Context&) const final
    {
        COWEL_ASSERT_UNREACHABLE(u8"Unimplemented.");
    }

    [[nodiscard]]
    Directive_Behavior* operator()(std::u8string_view name, Context& context) const final
    {
        return context.find_macro(name) ? &m_macro_behavior : nullptr;
    }
};

} // namespace cowel

#endif
