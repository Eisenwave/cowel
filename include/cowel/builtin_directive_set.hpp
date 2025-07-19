#ifndef COWEL_BUILTIN_DIRECTIVE_SET
#define COWEL_BUILTIN_DIRECTIVE_SET

#include <memory>
#include <string_view>

#include "cowel/directive_display.hpp"
#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/html_writer.hpp"

#include "cowel/directive_behavior.hpp"

namespace cowel {

inline constexpr char8_t builtin_directive_prefix = u8'-';
inline constexpr std::u8string_view html_tag_prefix = u8"html-";

/// @brief Indicates which `Content_Policy` should be used by the directive.
enum struct Policy_Usage : bool {
    /// @brief The given policy should be used directly.
    /// This results in applying syntax highlighting and other transformations on content
    /// if the surrounding policy does that.
    inherit,
    /// @brief The content should be processed by an `HTML_Content_Policy`,
    /// which results in the given policy being fed nothing but "HTML blobs".
    /// This is desirable for directives that are not subject to syntax highlighting,
    /// like `\ul`, `\li`, etc.
    html,
};

/// @brief Indicates whether an intro for "special blocks", i.e. notes, examples, etc.
/// should be emitted.
/// That is, a short text snippet at the start of the text content, like "Note:".
enum struct Intro_Policy : bool {
    no,
    yes,
};

struct Deprecated_Behavior : Directive_Behavior {
private:
    const Directive_Behavior& m_behavior;
    const std::u8string_view m_replacement;

public:
    constexpr Deprecated_Behavior(const Directive_Behavior& other, std::u8string_view replacement)
        : m_behavior { other }
        , m_replacement { replacement }
    {
    }

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy& out, const ast::Directive& d, Context& context) const override
    {
        warn(d, context);
        return m_behavior(out, d, context);
    }

    void warn(const ast::Directive& d, Context& context) const
    {
        const std::u8string_view message[] {
            u8"This directive is deprecated; use \\",
            m_replacement,
            u8" instead.",
        };
        context.try_warning(
            diagnostic::deprecated, d.get_name_span(), joined_char_sequence(message)
        );
    }
};

/// @brief Behavior for `\\error` directives.
/// Does not processing.
/// Generates no plaintext.
/// Generates HTML with the source code of the contents wrapped in an `<error->` custom tag.
struct Error_Behavior : Directive_Behavior {
    static constexpr std::u8string_view id = u8"error-";

    constexpr explicit Error_Behavior() = default;

    [[nodiscard]]
    Content_Status operator()(Content_Policy& out, const ast::Directive& d, Context&) const override
    {
        // TODO: inline display
        switch (out.get_language()) {
        case Output_Language::none: {
            return Content_Status::ok;
        }
        case Output_Language::html: {
            HTML_Writer writer { out };
            writer.open_tag(id);
            writer.write_inner_text(d.get_source());
            writer.close_tag(id);
            return Content_Status::ok;
        }
        default: {
            return Content_Status::ok;
        }
        }
    }
};

struct Comment_Behavior : Directive_Behavior {
    constexpr explicit Comment_Behavior() = default;

    [[nodiscard]]
    Content_Status operator()(Content_Policy&, const ast::Directive&, Context&) const override
    {
        return Content_Status::ok;
    }
};

struct [[nodiscard]]
Char_By_Entity_Behavior final : Directive_Behavior {
    constexpr explicit Char_By_Entity_Behavior() = default;

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy& out, const ast::Directive& d, Context&) const override;
};

struct [[nodiscard]] Code_Point_Behavior : Directive_Behavior {

    virtual Result<char32_t, Content_Status>
    get_code_point(const ast::Directive& d, Context& context) const = 0;

    [[nodiscard]]
    Content_Status operator()(Content_Policy& out, const ast::Directive& d, Context&) const final;
};

struct [[nodiscard]]
Char_By_Num_Behavior final : Code_Point_Behavior {
    constexpr explicit Char_By_Num_Behavior() = default;

    [[nodiscard]]
    Result<char32_t, Content_Status>
    get_code_point(const ast::Directive& d, Context& context) const final;
};

struct [[nodiscard]]
Char_By_Name_Behavior final : Code_Point_Behavior {
    constexpr explicit Char_By_Name_Behavior() = default;

    [[nodiscard]]
    Result<char32_t, Content_Status>
    get_code_point(const ast::Directive& d, Context& context) const final;
};

struct [[nodiscard]]
Char_Get_Num_Behavior final : Directive_Behavior {

    [[nodiscard]]
    constexpr explicit Char_Get_Num_Behavior()
        = default;

    [[nodiscard]]
    Content_Status operator()(Content_Policy& out, const ast::Directive& d, Context&) const final;
};

// clang-format off
inline constexpr std::u8string_view lorem_ipsum = u8"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";
// clang-format on

struct Lorem_Ipsum_Behavior final : Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Lorem_Ipsum_Behavior()
        = default;

    [[nodiscard]]
    Content_Status operator()(Content_Policy& out, const ast::Directive&, Context&) const override
    {
        try_enter_paragraph(out);

        out.write(lorem_ipsum, Output_Language::text);
        return Content_Status::ok;
    }
};

// TODO: use this for \pre directives too.
enum struct Pre_Trimming : bool { no, yes };

/// @brief Responsible for syntax-highlighted directives like `\code` or `\codeblock`.
struct [[nodiscard]] Code_Behavior : Directive_Behavior {
private:
    static constexpr std::u8string_view lang_parameter = u8"lang";
    static constexpr std::u8string_view nested_parameter = u8"nested";
    static constexpr std::u8string_view borders_parameter = u8"borders";
    static constexpr std::u8string_view prefix_parameter = u8"prefix";
    static constexpr std::u8string_view suffix_parameter = u8"suffix";
    // clang-format off
    static constexpr std::u8string_view parameters[] {
        lang_parameter,
        borders_parameter,
        nested_parameter,
        prefix_parameter,
        suffix_parameter,
    };
    // clang-format on

    const std::u8string_view m_tag_name;
    const Directive_Display m_display;
    const Pre_Trimming m_pre_compat_trim;

public:
    [[nodiscard]]
    constexpr explicit Code_Behavior(
        std::u8string_view tag_name,
        Directive_Display display,
        Pre_Trimming pre_compat_trim
    )
        : m_tag_name { tag_name }
        , m_display { display }
        , m_pre_compat_trim { pre_compat_trim }
    {
    }

    [[nodiscard]]
    Content_Status operator()(Content_Policy&, const ast::Directive&, Context&) const override;
};

/// @brief Forces a certain highlight to be applied.
struct [[nodiscard]] Highlight_As_Behavior : Directive_Behavior {
private:
    static constexpr std::u8string_view name_parameter = u8"name";
    static constexpr std::u8string_view parameters[] { name_parameter };

public:
    [[nodiscard]]
    constexpr explicit Highlight_As_Behavior()
        = default;

    [[nodiscard]]
    Content_Status operator()(Content_Policy& out, const ast::Directive&, Context&) const override;
};

struct Literally_Behavior : Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Literally_Behavior()
        = default;

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy& out, const ast::Directive& d, Context& context) const override;
};

struct Unprocessed_Behavior : Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Unprocessed_Behavior()
        = default;

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy& out, const ast::Directive& d, Context& context) const override;
};

struct HTML_Behavior : Directive_Behavior {
private:
    const Directive_Display m_display;

public:
    [[nodiscard]]
    constexpr explicit HTML_Behavior(Directive_Display display)
        : m_display { display }
    {
    }

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy& out, const ast::Directive& d, Context& context) const override;
};

/// @brief Common behavior for generating `<script>` and `<style>` elements
/// via `\script` and `\style`.
///
/// Note that this behavior is distinct from formatting directives like `\b`.
/// Notably, this produces a pure HTML directive with `block` display.
/// Also, character references (e.g. `&lt;`) have no special meaning in such tags,
/// so the output is not escaped in the usual way but taken quite literally,
/// similar to `HTML_Literal_Behavior`.
struct [[nodiscard]]
HTML_Raw_Text_Behavior final : Directive_Behavior {
private:
    const std::u8string_view m_tag_name;

public:
    [[nodiscard]]
    constexpr explicit HTML_Raw_Text_Behavior(std::u8string_view tag_name)
        : m_tag_name { tag_name }
    {
        COWEL_ASSERT(tag_name == u8"style" || tag_name == u8"script");
    }

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy& out, const ast::Directive& d, Context& context) const override;
};

struct Variable_Behavior : Directive_Behavior {
    static constexpr std::u8string_view var_parameter = u8"var";
    static constexpr std::u8string_view parameters[] { var_parameter };

    [[nodiscard]]
    constexpr explicit Variable_Behavior()
        = default;

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy& out, const ast::Directive& d, Context& context) const override;

protected:
    [[nodiscard]]
    virtual Content_Status generate_var(
        Content_Policy& out,
        const ast::Directive& d,
        std::u8string_view var,
        Context& context
    ) const
        = 0;
};

enum struct Expression_Type : Default_Underlying {
    add,
    subtract,
    multiply,
    divide,
};

[[nodiscard]]
constexpr int expression_type_neutral_element(Expression_Type e)
{
    return e == Expression_Type::add || e == Expression_Type::subtract ? 0 : 1;
}

struct Expression_Behavior final : Directive_Behavior {
private:
    const Expression_Type m_type;

public:
    [[nodiscard]]
    constexpr explicit Expression_Behavior(Expression_Type type)
        : m_type { type }
    {
    }

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy& out, const ast::Directive&, Context& context) const final;
};

struct Get_Variable_Behavior final : Variable_Behavior {
    [[nodiscard]]
    constexpr explicit Get_Variable_Behavior()
        = default;

    [[nodiscard]]
    Content_Status generate_var(
        Content_Policy& out,
        const ast::Directive&,
        std::u8string_view var,
        Context& context
    ) const final;
};

enum struct Variable_Operation : Default_Underlying {
    // TODO: add more operations
    set
};

[[nodiscard]]
Content_Status set_variable_to_op_result(
    Variable_Operation op,
    const ast::Directive& d,
    std::u8string_view var,
    Context& context
);

struct Modify_Variable_Behavior final : Variable_Behavior {
private:
    const Variable_Operation m_op;

public:
    [[nodiscard]]
    constexpr explicit Modify_Variable_Behavior(Variable_Operation op)
        : m_op { op }
    {
    }

    [[nodiscard]]
    Content_Status
    generate_var(Content_Policy&, const ast::Directive& d, std::u8string_view var, Context& context)
        const final
    {
        return set_variable_to_op_result(m_op, d, var, context);
    }
};

struct HTML_Wrapper_Behavior final : Directive_Behavior {
private:
    const Directive_Display m_display;
    const bool m_is_paragraphed;

public:
    [[nodiscard]]
    constexpr explicit HTML_Wrapper_Behavior(Directive_Display display, To_HTML_Mode to_html_mode)
        : m_display { display }
        , m_is_paragraphed { to_html_mode == To_HTML_Mode::paragraphs }
    {
        COWEL_ASSERT(
            to_html_mode == To_HTML_Mode::paragraphs || to_html_mode == To_HTML_Mode::direct
        );
    }

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy& out, const ast::Directive& d, Context& context) const override;
};

struct Plaintext_Wrapper_Behavior : Directive_Behavior {
protected:
    const Directive_Display m_display;

public:
    [[nodiscard]]
    constexpr explicit Plaintext_Wrapper_Behavior(Directive_Display display)
        : m_display { display }
    {
    }

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy& out, const ast::Directive& d, Context& context) const override;
};

struct Trim_Behavior : Directive_Behavior {
protected:
    const Directive_Display m_display;

public:
    [[nodiscard]]
    constexpr explicit Trim_Behavior(Directive_Display display)
        : m_display { display }
    {
    }

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy& out, const ast::Directive& d, Context& context) const override;
};

struct Passthrough_Behavior : Directive_Behavior {
protected:
    const Policy_Usage m_policy;
    const Directive_Display m_display;

public:
    [[nodiscard]]
    constexpr explicit Passthrough_Behavior(Policy_Usage policy, Directive_Display display)
        : m_policy { policy }
        , m_display { display }
    {
    }

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy& out, const ast::Directive& d, Context& context) const override;

    [[nodiscard]]
    virtual std::u8string_view get_name(const ast::Directive& d) const
        = 0;
};

struct In_Tag_Behavior : Directive_Behavior {
protected:
    const std::u8string_view m_tag_name;
    const std::u8string_view m_class_name;
    const Policy_Usage m_policy;
    const Directive_Display m_display;

public:
    [[nodiscard]]
    constexpr explicit In_Tag_Behavior(
        std::u8string_view tag_name,
        std::u8string_view class_name,
        Policy_Usage policy,
        Directive_Display display
    )
        : m_tag_name { tag_name }
        , m_class_name { class_name }
        , m_policy { policy }
        , m_display { display }
    {
    }

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy& out, const ast::Directive& d, Context& context) const override;
};

/// @brief Behavior for self-closing tags, like `<br/>` and `<hr/>`.
struct Self_Closing_Behavior final : Directive_Behavior {
private:
    const std::u8string_view m_tag_name;
    const Directive_Display m_display;

public:
    [[nodiscard]]
    constexpr explicit Self_Closing_Behavior(std::u8string_view tag_name, Directive_Display display)
        : m_tag_name { tag_name }
        , m_display { display }
    {
    }

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy& out, const ast::Directive& d, Context& context) const final;
};

/// @brief Behavior for any formatting tags that are mapped onto HTML with the same name.
/// This includes `\\i{...}`, `\\strong`, and many more.
///
/// Preprocesses and processes all arguments.
/// Generates the contents inside in plaintext.
///
/// Generates HTML where arguments are converted to HTML attributes,
/// in a tag that has the same name as the directive.
/// For example, `\\i[id = 123]{...}` generates `<i id=123>...</i>`.
struct Directive_Name_Passthrough_Behavior : Passthrough_Behavior {
private:
    const std::u8string_view m_name_prefix;

public:
    [[nodiscard]]
    constexpr explicit Directive_Name_Passthrough_Behavior(
        Policy_Usage policy,
        Directive_Display display,
        std::u8string_view name_prefix
    )
        : Passthrough_Behavior { policy, display }
        , m_name_prefix { name_prefix }
    {
    }

    [[nodiscard]]
    std::u8string_view get_name(const ast::Directive& d) const override;
};

struct Fixed_Name_Passthrough_Behavior : Passthrough_Behavior {
private:
    const std::u8string_view m_name;

public:
    [[nodiscard]]
    constexpr explicit Fixed_Name_Passthrough_Behavior(
        std::u8string_view name,
        Policy_Usage policy,
        Directive_Display display
    )
        : Passthrough_Behavior { policy, display }
        , m_name { name }
    {
    }

    [[nodiscard]]
    std::u8string_view get_name(const ast::Directive&) const override
    {
        return m_name;
    }
};

struct Special_Block_Behavior final : Directive_Behavior {
private:
    const std::u8string_view m_name;
    const Intro_Policy m_intro;

public:
    [[nodiscard]]
    constexpr explicit Special_Block_Behavior(std::u8string_view name, Intro_Policy intro)
        : m_name { name }
        , m_intro { intro }
    {
    }

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy& out, const ast::Directive& d, Context& context) const final;
};

struct WG21_Block_Behavior final : Directive_Behavior {
private:
    const std::u8string_view m_prefix;
    const std::u8string_view m_suffix;

public:
    [[nodiscard]]
    constexpr explicit WG21_Block_Behavior(std::u8string_view prefix, std::u8string_view suffix)
        : m_prefix { prefix }
        , m_suffix { suffix }
    {
    }

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy& out, const ast::Directive& d, Context& context) const final;
};

struct WG21_Head_Behavior final : Directive_Behavior {
    [[nodiscard]]
    constexpr explicit WG21_Head_Behavior()
        = default;

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy& out, const ast::Directive& d, Context& context) const final;
};

struct URL_Behavior final : Directive_Behavior {
private:
    const std::u8string_view m_url_prefix;

public:
    [[nodiscard]]
    constexpr explicit URL_Behavior(std::u8string_view url_prefix = u8"")
        : m_url_prefix { url_prefix }
    {
    }

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy& out, const ast::Directive& d, Context& context) const final;
};

struct Ref_Behavior final : Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Ref_Behavior()
        = default;

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy&, const ast::Directive& d, Context& context) const final;
};

struct Bibliography_Add_Behavior final : Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Bibliography_Add_Behavior()
        = default;

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy& out, const ast::Directive& d, Context& context) const final;
};

struct List_Behavior final : Directive_Behavior {
private:
    const std::u8string_view m_tag_name;
    const Directive_Behavior& m_item_behavior;

public:
    [[nodiscard]]
    constexpr explicit List_Behavior(
        std::u8string_view tag_name,
        const Directive_Behavior& item_behavior
    )
        : m_tag_name { tag_name }
        , m_item_behavior { item_behavior }
    {
    }

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy& out, const ast::Directive& d, Context& context) const override;
};

struct Heading_Behavior final : Directive_Behavior {
private:
    const int m_level;

public:
    [[nodiscard]]
    constexpr explicit Heading_Behavior(int level)
        : m_level { level }
    {
        COWEL_ASSERT(m_level >= 1 && level <= 6);
    }

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy& out, const ast::Directive& d, Context& context) const override;
};

struct There_Behavior final : Directive_Behavior {
    [[nodiscard]]
    constexpr explicit There_Behavior()
        = default;

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy& out, const ast::Directive& d, Context& context) const override;
};

struct Here_Behavior final : Directive_Behavior {
private:
    const Directive_Display m_display;

public:
    [[nodiscard]]
    constexpr explicit Here_Behavior(Directive_Display display)
        : m_display { display }
    {
    }

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy& out, const ast::Directive& d, Context& context) const override;
};

struct Make_Section_Behavior final : Directive_Behavior {
private:
    const Directive_Display m_display;
    const std::u8string_view m_class_name;
    const std::u8string_view m_section_name;

public:
    [[nodiscard]]
    constexpr explicit Make_Section_Behavior(
        Directive_Display display,
        std::u8string_view class_name,
        std::u8string_view section_name
    )
        : m_display { display }
        , m_class_name { class_name }
        , m_section_name { section_name }
    {
    }

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy& out, const ast::Directive& d, Context& context) const override;
};

struct Math_Behavior final : Directive_Behavior {
private:
    const Directive_Display m_display;

public:
    [[nodiscard]]
    constexpr explicit Math_Behavior(Directive_Display display)
        : m_display { display }
    {
    }

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy& out, const ast::Directive& d, Context& context) const override;
};

struct Include_Behavior final : Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Include_Behavior()
        = default;

    [[nodiscard]]
    Content_Status operator()(Content_Policy& out, const ast::Directive&, Context&) const override;
};

struct Import_Behavior final : Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Import_Behavior()
        = default;

    [[nodiscard]]
    Content_Status operator()(Content_Policy& out, const ast::Directive&, Context&) const override;
};

struct Macro_Define_Behavior final : Directive_Behavior {

    [[nodiscard]]
    Content_Status
    operator()(Content_Policy& out, const ast::Directive& d, Context& context) const override;
};

struct Macro_Instantiate_Behavior final : Directive_Behavior {

    [[nodiscard]]
    Content_Status operator()(Content_Policy& out, const ast::Directive&, Context&) const override;
};

struct [[nodiscard]]
Builtin_Directive_Set final : Name_Resolver {
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

public:
    Builtin_Directive_Set();

    Builtin_Directive_Set(const Builtin_Directive_Set&) = delete;
    Builtin_Directive_Set& operator=(const Builtin_Directive_Set&) = delete;

    ~Builtin_Directive_Set();

    [[nodiscard]]
    Directive_Behavior& get_error_behavior() noexcept;

    [[nodiscard]]
    Directive_Behavior& get_macro_behavior() noexcept;

    [[nodiscard]]
    Distant<std::u8string_view>
    fuzzy_lookup_name(std::u8string_view name, Context& context) const final;

    [[nodiscard]]
    Directive_Behavior* operator()(std::u8string_view name, Context& context) const final;
};

namespace class_name {

inline constexpr std::u8string_view bibliography = u8"bib";
inline constexpr std::u8string_view table_of_contents = u8"toc";

} // namespace class_name

namespace section_name {

inline constexpr std::u8string_view bibliography = u8"std.bib";
inline constexpr std::u8string_view id_preview = u8"std.id-preview";
inline constexpr std::u8string_view document_html = u8"std.html";
inline constexpr std::u8string_view document_head = u8"std.head";
inline constexpr std::u8string_view document_body = u8"std.body";
inline constexpr std::u8string_view table_of_contents = u8"std.toc";

} // namespace section_name

} // namespace cowel

#endif
