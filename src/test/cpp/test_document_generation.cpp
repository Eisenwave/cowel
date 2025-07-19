#include <filesystem>
#include <initializer_list>
#include <memory_resource>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "cowel/diagnostic_highlight.hpp"
#include "cowel/policy/paragraph_split.hpp"
#include "cowel/util/annotated_string.hpp"
#include "cowel/util/assert.hpp"
#include "cowel/util/meta.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/directive_behavior.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/document_generation.hpp"
#include "cowel/fwd.hpp"
#include "cowel/parse.hpp"

#include "collecting_logger.hpp"
#include "diff.hpp"
#include "io.hpp"
#include "test_highlighter.hpp"

namespace cowel {
namespace {

using Suppress_Unused_Include_Annotated_String = Basic_Annotated_String<void, void>;

enum struct Test_Behavior : Default_Underlying {
    trivial,
    paragraphs,
    empty_head,
};

[[nodiscard]]
Content_Status
write_empty_head_document(Text_Sink& out, std::span<const ast::Content> content, Context& context)
{
    constexpr auto write_head = [](Content_Policy&, std::span<const ast::Content>, Context&) {
        return Content_Status::ok;
    };
    constexpr auto write_body
        = [](Content_Policy& policy, std::span<const ast::Content> content, Context& context) {
              return consume_all(policy, content, context);
          };

    return write_head_body_document(out, content, context, const_v<write_head>, const_v<write_body>);
}

struct Doc_Gen_Test : testing::Test {
    std::pmr::monotonic_buffer_resource memory;
    std::pmr::vector<char8_t> out { &memory };

    Builtin_Directive_Set builtin_directives {};

    std::filesystem::path file_path;
    std::pmr::vector<char8_t> source { &memory };
    std::pmr::vector<char8_t> theme_source { &memory };
    std::u8string_view source_string {};
    std::u8string_view theme_source_string {};
    std::pmr::vector<ast::Content> content { &memory };

    Collecting_Logger logger { &memory };

    Doc_Gen_Test()
    {
        const bool theme_loaded = load_theme();
        COWEL_ASSERT(theme_loaded);
    }

    template <Test_Behavior behavior>
    [[nodiscard]]
    Function_Ref<Content_Status(Context&)> get_behavior_impl()
    {
        constexpr auto action = [](Doc_Gen_Test* self, Context& context) -> Content_Status {
            Capturing_Ref_Text_Sink sink { self->out, Output_Language::html };
            switch (behavior) {
            case Test_Behavior::trivial: {
                HTML_Content_Policy policy { sink };
                return consume_all(policy, self->content, context);
            }
            case Test_Behavior::paragraphs: {
                Paragraph_Split_Policy policy { sink, context.get_transient_memory() };
                return consume_all(policy, self->content, context);
            }
            case Test_Behavior::empty_head: {
                return write_empty_head_document(sink, self->content, context);
            }
            }
        };
        return Function_Ref<Content_Status(Context&)> { const_v<action>, this };
    }

    [[nodiscard]]
    Function_Ref<Content_Status(Context&)> get_behavior(Test_Behavior behavior)
    {
        switch (behavior) {
        case Test_Behavior::trivial: return get_behavior_impl<Test_Behavior::trivial>();
        case Test_Behavior::paragraphs: return get_behavior_impl<Test_Behavior::paragraphs>();
        case Test_Behavior::empty_head: return get_behavior_impl<Test_Behavior::empty_head>();
        }
        COWEL_ASSERT_UNREACHABLE(u8"Invalid behavior.");
    }

    [[nodiscard]]
    bool load_document(const std::filesystem::path& path)
    {
        file_path = "test" / path;
        if (!load_utf8_file_or_error(source, file_path.generic_u8string(), &memory)) {
            return false;
        }
        source_string = { source.data(), source.size() };
        content = parse_and_build(source_string, File_Id {}, &memory, make_parse_error_consumer());
        return true;
    }

    [[nodiscard]]
    bool load_theme()
    {
        constexpr std::u8string_view theme_path = u8"ulight/themes/wg21.json";

        if (!load_utf8_file_or_error(theme_source, theme_path, &memory)) {
            return false;
        }
        theme_source_string = { source.data(), source.size() };
        return true;
    }

    void load_source(std::u8string_view source)
    {
        source_string = source;
        content = parse_and_build(source, File_Id {}, &memory, make_parse_error_consumer());
    }

    [[nodiscard]]
    Content_Status generate(Test_Behavior behavior)
    {
        Directive_Behavior& error_behavior = builtin_directives.get_error_behavior();
        const Generation_Options options { .builtin_behavior = builtin_directives,
                                           .error_behavior = &error_behavior,
                                           .highlight_theme_source = theme_source_string,
                                           .logger = logger,
                                           .highlighter = test_highlighter,
                                           .memory = &memory };
        const Function_Ref<Content_Status(Context&)> f = get_behavior(behavior);
        return run_generation(f, options);
    }

    [[nodiscard]]
    std::u8string_view output_text() const
    {
        return as_u8string_view(out);
    }

    [[nodiscard]]
    Parse_Error_Consumer make_parse_error_consumer()
    {
        constexpr auto invoker = [](Doc_Gen_Test* self, std::u8string_view id,
                                    const File_Source_Span& location, std::u8string_view message) {
            Diagnostic d { Severity::error, id, location, message };
            self->logger(d);
        };
        return Parse_Error_Consumer { const_v<invoker>, this };
    }

    void clear()
    {
        out.clear();
        source.clear();
        content.clear();
        logger.diagnostics.clear();
    }
};

TEST_F(Doc_Gen_Test, empty)
{
    constexpr std::u8string_view expected;
    ASSERT_TRUE(load_document("empty.cow"));
    const Content_Status status = generate(Test_Behavior::trivial);
    EXPECT_EQ(status, Content_Status::ok);
    EXPECT_EQ(expected, output_text());
}

TEST_F(Doc_Gen_Test, text)
{
    constexpr std::u8string_view expected = u8"Hello, world!\n";
    ASSERT_TRUE(load_document("text.cow"));
    const Content_Status status = generate(Test_Behavior::trivial);
    EXPECT_EQ(status, Content_Status::ok);
    EXPECT_EQ(expected, output_text());
}

TEST_F(Doc_Gen_Test, paragraphs)
{
    constexpr std::u8string_view expected = u8R"(<p>This is
a paragraph.
</p>
<p>This is another paragraph.
</p>)";
    ASSERT_TRUE(load_document("paragraphs.cow"));
    const Content_Status status = generate(Test_Behavior::paragraphs);
    EXPECT_EQ(status, Content_Status::ok);
    EXPECT_EQ(expected, output_text());
}

struct Path {
    std::u8string_view value;
};

struct Source {
    std::u8string_view contents;
};

struct Basic_Test {
    std::variant<Path, Source> document;
    std::variant<Path, Source> expected_html;
    Content_Status expected_status = Content_Status::ok;
    std::initializer_list<std::u8string_view> expected_diagnostics = {};
    Test_Behavior behavior = Test_Behavior::trivial;
};

// clang-format off
constexpr Basic_Test basic_tests[] {
    { Source {u8"\\c{#x41}\\c{#x42}\\c{#x43}\n"},
      Source{ u8"&#x41;&#x42;&#x43;\n" } },

    { Source{ u8"\\c{#x00B6}\n" },
      Source { u8"&#x00B6;\n" } },

    { Source{ u8"\\c{}\n" },
      Source { u8"<error->\\c{}</error->\n" },
      Content_Status::error,
      { diagnostic::c::blank } },

    { Source{ u8"\\c{ }\n" },
      Source { u8"<error->\\c{ }</error->\n" },
      Content_Status::error,
      { diagnostic::c::blank } },

    { Source{ u8"\\c{#zzz}\n" },
      Source { u8"<error->\\c{#zzz}</error->\n" },
      Content_Status::error,
      { diagnostic::c::digits } },

    { Source{ u8"\\c{#xD800}\n" },
      Source { u8"<error->\\c{#xD800}</error->\n" },
      Content_Status::error,
      { diagnostic::c::nonscalar } },
    
    { Path { u8"U/ascii.cow" },
      Source { u8"ABC\n" } },

    { Source { u8"\\U{00B6}\n" },
      Source { u8"¶\n" } },

    { Source { u8"\\U{}\n" },
      Source { u8"<error->\\U{}</error->\n" },
      Content_Status::error,
      { diagnostic::U::blank } },

    { Source { u8"\\U{ }\n" },
      Source { u8"<error->\\U{ }</error->\n" },
      Content_Status::error,
      { diagnostic::U::blank } },

    { Source { u8"\\U{zzz}\n" },
      Source { u8"<error->\\U{zzz}</error->\n" },
      Content_Status::error,
      { diagnostic::U::digits } },

    { Source { u8"\\U{D800}\n" },
      Source { u8"<error->\\U{D800}</error->\n" },
      Content_Status::error,
      { diagnostic::U::nonscalar } },

    { Source { u8"\\h1{Heading}\n" },
      Source { u8"<h1 id=heading><a class=para href=#heading></a>Heading</h1>\n" } },

    { Source { u8"\\h1{\\code[x]{abcx}}\n" },
      Source { u8"<h1 id=abcx><a class=para href=#abcx></a><code>abc<h- data-h=kw>x</h-></code></h1>\n" } },

    { Source { u8"\\h2[listed=no]{ }\n" },
      Source { u8"<h2> </h2>\n" } },

    { Source { u8"\\h3[id=user id,listed=no]{Heading}\n" },
      Source { u8"<h3 id=\"user id\"><a class=para href=\"#user%20id\"></a>Heading</h3>\n" } },

    { Source { u8"\\h4[id=user-id,listed=no]{Heading}\n" },
      Source { u8"<h4 id=user-id><a class=para href=#user-id></a>Heading</h4>\n" } },

    { Source { u8"\\html{<b>Bold</b>}\n" },
      Source { u8"<b>Bold</b>\n" } },

    { Source { u8"\\style{b { color: red; }}\n" },
      Source { u8"<style>b { color: red; }</style>\n" } },
    
    { Source { u8"\\script{let x = 3 < 5; let y = true && false;}\n" },
      Source { u8"<script>let x = 3 < 5; let y = true && false;</script>\n" } },

    { Source { u8"\\code{}\n" },
      Source { u8"<code></code>\n" },
      Content_Status::ok,
      { diagnostic::highlight_language } },

    { Source { u8"\\code[x]{}\n" },
      Source { u8"<code></code>\n" } },

    { Source { u8"\\code[x]{ }\n" },
      Source { u8"<code> </code>\n" } },

    { Source { u8"\\code[x]{xxx}\n" },
      Source { u8"<code><h- data-h=kw>xxx</h-></code>\n" } },

    { Source { u8"\\code[x]{xxx123}\n" },
      Source { u8"<code><h- data-h=kw>xxx</h->123</code>\n" } },

    { Source { u8"\\code[x]{ 123 }\n" },
      Source { u8"<code> 123 </code>\n" } },

    { Source { u8"\\code[x]{ \\b{123} }\n" },
      Source { u8"<code> <b>123</b> </code>\n" } },

    { Source { u8"\\code[x]{ \\b{xxx} }\n" },
      Source { u8"<code> <b><h- data-h=kw>xxx</h-></b> </code>\n" } },

    { Source { u8"\\code[x]{ \\b{x}xx }\n" },
      Source { u8"<code> <b><h- data-h=kw>x</h-></b><h- data-h=kw>xx</h-> </code>\n" } },

    { Path { u8"codeblock/trim.cow" },
      Path { u8"codeblock/trim.html" } },

    { Source { u8"\\hl[keyword]{awoo}\n" },
      Source { u8"<h- data-h=kw>awoo</h->\n" } },

    { Source { u8"\\code[c]{int \\hl[number]{x}}\n" },
      Source { u8"<code><h- data-h=kw_type>int</h-> <h- data-h=num>x</h-></code>\n" } },

    { Source { u8"\\math{\\mi[id=Z]{x}}\n" },
      Source { u8"<math display=inline><mi id=Z>x</mi></math>\n" } },

    { Path { u8"macro/macros.cow" },
      Path { u8"macro/macros.html" } },

    { Source { u8"\\awoo\n" },
      Source { u8"<error->\\awoo</error->\n" },
      Content_Status::error,
      { diagnostic::directive_lookup_unresolved } },

    { Source { u8"\\code[x]{\\awoo}\n" },
      Source { u8"<code><error->\\awoo</error-></code>\n" },
      Content_Status::error,
      { diagnostic::directive_lookup_unresolved } },

    { Source { u8"" },
      Path { u8"document/empty.html" },
      Content_Status::error,
      {},
      Test_Behavior::empty_head },
    
    { Path { u8"comments.cow" },
      Path { u8"comments.cow.html" } }
};
// clang-format on

[[nodiscard]]
bool load_basic_test_input(Doc_Gen_Test& fixture, const Basic_Test& test)
{
    if (const auto* const path = std::get_if<Path>(&test.document)) {
        return fixture.load_document(path->value);
    }
    const auto& source = std::get<Source>(test.document);
    fixture.load_source(source.contents);
    return true;
}

[[nodiscard]]
std::u8string_view load_basic_test_expectations(
    std::pmr::vector<char8_t>& storage,
    const Basic_Test& test,
    std::pmr::memory_resource* memory
)
{
    if (const auto* const path = std::get_if<Path>(&test.expected_html)) {
        storage.clear();
        std::pmr::u8string full_path { u8"test/", memory };
        full_path += path->value;
        if (!load_utf8_file_or_error(storage, full_path, memory)) {
            return u8"";
        }
        return std::u8string_view { storage.data(), storage.size() };
    }
    return std::get<Source>(test.expected_html).contents;
}

void append_specials_escaped(
    Diagnostic_String& out,
    std::u8string_view str,
    Diagnostic_Highlight default_highlight
)
{
    for (char8_t c : str) {
        switch (c) {
        case u8'\n': out.append(u8"\\n", Diagnostic_Highlight::escape); break;
        case u8'\t': out.append(u8"\\t", Diagnostic_Highlight::escape); break;
        default: out.append(c, default_highlight);
        }
    }
}

void append_test_details(Diagnostic_String& out, const Basic_Test& test)
{
    if (const auto* const source = std::get_if<Source>(&test.document)) {
        out.append(u8'\"', Diagnostic_Highlight::code_citation);
        append_specials_escaped(out, source->contents, Diagnostic_Highlight::code_citation);
        out.append(u8'\"', Diagnostic_Highlight::code_citation);
    }
    else if (const auto* const path = std::get_if<Path>(&test.document)) {
        out.append(path->value, Diagnostic_Highlight::code_position);
    }
    out.append(u8':', Diagnostic_Highlight::punctuation);
    out.append(u8' ');
}

TEST_F(Doc_Gen_Test, basic_directive_tests)
{
    for (const Basic_Test& test : basic_tests) {
        clear();

        const bool load_success = load_basic_test_input(*this, test);
        EXPECT_TRUE(load_success);
        if (!load_success) {
            continue;
        }

        std::pmr::vector<char8_t> expectation_storage { &memory };
        const std::u8string_view expected
            = load_basic_test_expectations(expectation_storage, test, &memory);
        EXPECT_FALSE(expected.empty());
        if (expected.empty()) {
            continue;
        }

        const Content_Status status = generate(test.behavior);
        if (status != test.expected_status) {
            Diagnostic_String error { &memory };
            append_test_details(error, test);
            error.build(Diagnostic_Highlight::error_text)
                .append(u8"Test failed because the status (")
                .append(status_name(status))
                .append(u8") was not as expected (")
                .append(status_name(test.expected_status))
                .append(u8')');
            error.append(u8'\n');
            print_code_string_stdout(error);
            flush_stdout();
        }
        EXPECT_TRUE(status == test.expected_status);
        if (expected != output_text()) {
            Diagnostic_String error { &memory };
            append_test_details(error, test);
            error.append(
                u8"Test failed because generated HTML does not match expected HTML.\n",
                Diagnostic_Highlight::error_text
            );
            print_lines_diff(error, output_text(), expected);
            error.append(u8'\n');
            print_code_string_stdout(error);
            flush_stdout();
        }
        EXPECT_TRUE(expected == output_text());

        if (test.expected_diagnostics.size() == 0) {
            if (!logger.diagnostics.empty()) {
                Diagnostic_String error { &memory };
                append_test_details(error, test);
                error.append(
                    u8"Test failed because an unexpected diagnostic was emitted:\n",
                    Diagnostic_Highlight::error_text
                );
                error.append(logger.diagnostics.front().id);
                error.append(u8"\n\n");
                print_code_string_stdout(error);
                flush_stdout();
            }
            EXPECT_TRUE(logger.diagnostics.empty());
            continue;
        }
        for (const std::u8string_view id : test.expected_diagnostics) {
            EXPECT_TRUE(logger.was_logged(id));
        }
    }
}

} // namespace
} // namespace cowel
