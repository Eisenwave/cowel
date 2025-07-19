#include <cstddef>
#include <expected>
#include <optional>
#include <string_view>
#include <vector>

#include "ulight/impl/lang/cowel.hpp"

#include "cowel/util/assert.hpp"
#include "cowel/util/chars.hpp"

#include "cowel/fwd.hpp"
#include "cowel/parse.hpp"

namespace cowel {

namespace {

enum struct Content_Context : Default_Underlying { document, argument_value, block };

struct [[nodiscard]] Parser {
private:
    struct [[nodiscard]] Scoped_Attempt {
    private:
        Parser* m_self;
        const std::size_t m_initial_pos;
        const std::size_t m_initial_size;

    public:
        Scoped_Attempt(Parser& self)
            : m_self { &self }
            , m_initial_pos { self.m_pos }
            , m_initial_size { self.m_out.size() }
        {
        }

        Scoped_Attempt(const Scoped_Attempt&) = delete;
        Scoped_Attempt& operator=(const Scoped_Attempt&) = delete;

        void commit()
        {
            COWEL_ASSERT(m_self);
            m_self = nullptr;
        }

        void abort()
        {
            COWEL_ASSERT(m_self);
            COWEL_ASSERT(m_self->m_out.size() >= m_initial_size);

            m_self->m_pos = m_initial_pos;
            m_self->m_out.resize(m_initial_size);

            m_self = nullptr;
        }

        ~Scoped_Attempt() // NOLINT(bugprone-exception-escape)
        {
            if (m_self) {
                abort();
            }
        }
    };

    std::pmr::vector<AST_Instruction>& m_out;
    const std::u8string_view m_source;

    std::size_t m_pos = 0;

public:
    Parser(std::pmr::vector<AST_Instruction>& out, std::u8string_view source)
        : m_out { out }
        , m_source { source }
    {
    }

    void operator()()
    {
        const std::size_t document_instruction_index = m_out.size();
        m_out.push_back({ AST_Instruction_Type::push_document, 0 });
        const std::size_t content_amount = match_content_sequence(Content_Context::document);
        m_out[document_instruction_index].n = content_amount;
        m_out.push_back({ AST_Instruction_Type::pop_document, 0 });
    }

private:
    Scoped_Attempt attempt()
    {
        return Scoped_Attempt { *this };
    }

    /// @brief Returns all remaining text as a `std::string_view_type`, from the current parsing
    /// position to the end of the file.
    /// @return All remaining text.
    [[nodiscard]]
    std::u8string_view peek_all() const
    {
        return m_source.substr(m_pos);
    }

    /// @brief Returns the next character and advances the parser position.
    /// @return The popped character.
    /// @throws Throws if `eof()`.
    char8_t pop()
    {
        const char8_t c = peek();
        ++m_pos;
        return c;
    }

    /// @brief Returns the next character.
    /// @return The next character.
    /// @throws Throws if `eof()`.
    [[nodiscard]]
    char8_t peek() const
    {
        COWEL_ASSERT(!eof());
        return m_source[m_pos];
    }

    /// @return `true` if the parser is at the end of the file, `false` otherwise.
    [[nodiscard]]
    bool eof() const
    {
        return m_pos == m_source.length();
    }

    /// @return `peek_all().starts_with(text)`.
    [[nodiscard]]
    bool peek(std::u8string_view text) const
    {
        return peek_all().starts_with(text);
    }

    /// @brief Checks whether the next character matches an expected value without advancing
    /// the parser.
    /// @param c the character to test
    /// @return `true` if the next character equals `c`, `false` otherwise.
    [[nodiscard]]
    bool peek(char8_t c) const
    {
        return !eof() && m_source[m_pos] == c;
    }

    /// @brief Checks whether the next character satisfies a predicate without advancing
    /// the parser.
    /// @param predicate the predicate to test
    /// @return `true` if the next character satisfies `predicate`, `false` otherwise.
    bool peek(bool predicate(char8_t)) const
    {
        return !eof() && predicate(m_source[m_pos]);
    }

    [[nodiscard]]
    bool expect(char8_t c)
    {
        if (!peek(c)) {
            return false;
        }
        ++m_pos;
        return true;
    }

    [[nodiscard]]
    bool expect(bool predicate(char8_t))
    {
        if (eof()) {
            return false;
        }
        const char8_t c = m_source[m_pos];
        if (!predicate(c)) {
            return false;
        }
        // This function is only safe to call when we have expectations towards ASCII characters.
        // Any non-ASCII character should have already been rejected.
        COWEL_ASSERT(is_ascii(c));
        ++m_pos;
        return true;
    }

    [[nodiscard]]
    std::size_t match_content_sequence(Content_Context context)
    {
        Bracket_Levels levels {};
        std::size_t elements = 0;

        while (try_match_content(context, levels)) {
            ++elements;
        }

        return elements;
    }

    struct Bracket_Levels {
        std::size_t square = 0;
        std::size_t brace = 0;
    };

    /// @brief Attempts to match the next piece of content,
    /// which is an escape sequence, directive, or plaintext.
    ///
    /// Returns `false` if none of these could be matched.
    /// This may happen because the parser is located at e.g. a `}` and the given `context`
    /// is terminated by `}`.
    /// It may also happen if the parser has already reached the EOF.
    [[nodiscard]]
    bool try_match_content(Content_Context context, Bracket_Levels& levels)
    {
        if (peek(u8'\\') && (try_match_escaped() || try_match_comment() || try_match_directive())) {
            return true;
        }

        const std::size_t initial_pos = m_pos;

        for (; !eof(); ++m_pos) {
            const char8_t c = m_source[m_pos];
            if (c == u8'\\') {
                const std::u8string_view remainder { m_source.substr(m_pos + 1) };

                // Trailing \ at the end of the file.
                // No need to break, we'll just run into it next iteration.
                if (remainder.empty()) {
                    continue;
                }
                if (is_cowel_allowed_after_backslash(remainder.front())) {
                    break;
                }
                continue;
            }
            // At the document level, we don't care about brace mismatches,
            // commas, etc.
            if (context == Content_Context::document) {
                continue;
            }
            if (context == Content_Context::argument_value) {
                if (c == u8',') {
                    break;
                }
                if (c == u8'[') {
                    ++levels.square;
                }
                else if (c == u8']') {
                    if (levels.square == 0) {
                        break;
                    }
                    --levels.square;
                }
            }
            if (c == u8'{') {
                ++levels.brace;
            }
            else if (c == u8'}') {
                if (levels.brace == 0) {
                    break;
                }
                --levels.brace;
                continue;
            }
        }

        COWEL_ASSERT(m_pos >= initial_pos);
        if (m_pos == initial_pos) {
            return false;
        }

        m_out.push_back({ AST_Instruction_Type::text, m_pos - initial_pos });
        return true;
    }

    [[nodiscard]]
    bool try_match_directive()
    {
        Scoped_Attempt a = attempt();

        if (!expect('\\')) {
            return {};
        }
        const std::size_t name_length = ulight::cowel::match_directive_name(peek_all());
        if (name_length == 0) {
            return false;
        }
        m_pos += name_length;

        m_out.push_back({ AST_Instruction_Type::push_directive, name_length + 1 });

        try_match_argument_list();
        try_match_block();

        m_out.push_back({ AST_Instruction_Type::pop_directive, 0 });

        a.commit();
        return true;
    }

    // intentionally discardable
    bool try_match_argument_list()
    {
        Scoped_Attempt a = attempt();

        if (!expect(u8'[')) {
            return false;
        }
        const std::size_t arguments_instruction_index = m_out.size();
        m_out.push_back({ AST_Instruction_Type::push_arguments, 0 });

        for (std::size_t i = 0; try_match_argument(); ++i) {
            if (expect(u8']')) {
                m_out[arguments_instruction_index].n = i + 1;
                m_out.push_back({ AST_Instruction_Type::pop_arguments });
                a.commit();
                return true;
            }
            if (expect(u8',')) {
                m_out.push_back({ AST_Instruction_Type::argument_comma });
                continue;
            }
            COWEL_ASSERT_UNREACHABLE(
                u8"Successfully matched arguments must be followed by ']' or ','"
            );
        }

        return true;
    }

    [[nodiscard]]
    bool try_match_escaped()
    {
        const std::u8string_view remainder = m_source.substr(m_pos);
        if (const std::size_t length = ulight::cowel::match_escape(remainder)) {
            m_pos += length;
            m_out.push_back({ AST_Instruction_Type::escape, length });
            return true;
        }
        return false;
    }

    [[nodiscard]]
    bool try_match_comment()
    {
        const std::u8string_view remainder = m_source.substr(m_pos);
        if (const std::size_t length = ulight::cowel::match_line_comment(remainder)) {
            COWEL_ASSERT(remainder.starts_with(u8"\\:"));
            const std::u8string_view suffix = remainder.substr(length);
            const std::size_t suffix_length = //
                suffix.starts_with(u8"\r\n") ? 2
                : suffix.starts_with(u8'\n') ? 1
                                             : 0;

            m_pos += length + suffix_length;
            m_out.push_back({ AST_Instruction_Type::comment, length + suffix_length });
            return true;
        }
        return false;
    }

    [[nodiscard]]
    bool try_match_argument()
    {
        if (eof()) {
            return false;
        }
        Scoped_Attempt a = attempt();

        const std::size_t argument_instruction_index = m_out.size();
        m_out.push_back({ AST_Instruction_Type::push_argument });

        try_match_argument_name();

        const std::optional<std::size_t> result = try_match_trimmed_argument_value();
        if (!result) {
            return false;
        }

        m_out[argument_instruction_index].n = *result;
        m_out.push_back({ AST_Instruction_Type::pop_argument });

        a.commit();
        return true;
    }

    /// @brief Matches the name of an argument, including any surrounding whitespace and the `=`
    /// character following it.
    /// If the argument couldn't be matched, returns `false` and keeps the parser state unchanged.
    bool try_match_argument_name()
    {
        Scoped_Attempt a = attempt();

        const std::size_t leading_whitespace = ulight::cowel::match_whitespace(peek_all());
        if (leading_whitespace != 0) {
            m_out.push_back({ AST_Instruction_Type::skip, leading_whitespace });
        }
        m_pos += leading_whitespace;

        if (eof()) {
            return false;
        }

        const std::size_t name_length = ulight::cowel::match_argument_name(peek_all());
        m_out.push_back({ AST_Instruction_Type::argument_name, name_length });
        if (name_length == 0) {
            return false;
        }
        m_pos += name_length;

        const std::size_t trailing_whitespace = ulight::cowel::match_whitespace(peek_all());
        m_pos += trailing_whitespace;
        if (eof()) {
            return false;
        }

        if (!expect(u8'=')) {
            return false;
        }

        m_out.push_back({ AST_Instruction_Type::skip, trailing_whitespace });
        m_out.push_back({ AST_Instruction_Type::argument_equal });
        a.commit();
        return true;
    }

    [[nodiscard]]
    std::optional<std::size_t> try_match_trimmed_argument_value()
    {
        Scoped_Attempt a = attempt();

        const std::size_t leading_whitespace = ulight::cowel::match_whitespace(peek_all());
        if (leading_whitespace != 0) {
            m_out.push_back({ AST_Instruction_Type::skip, leading_whitespace });
        }
        m_pos += leading_whitespace;

        const std::size_t content_amount = match_content_sequence(Content_Context::argument_value);
        if (eof() || peek(u8'}')) {
            return {};
        }
        // match_content_sequence is very aggressive, so I think at this point,
        // we have to be at the end of an argument due to a comma separator or closing square.
        const char8_t c = m_source[m_pos];
        COWEL_ASSERT(c == u8',' || c == u8']');

        trim_trailing_whitespace_in_matched_content();

        a.commit();
        return content_amount;
    }

    /// @brief Trims trailing whitespace in just matched content.
    ///
    /// This is done by splitting the most recently written instruction
    /// into `text` and `skip` if that instruction is `text`.
    /// If the most recent instruction is entirely made of whitespace,
    /// it is simply replaced with `skip`.
    void trim_trailing_whitespace_in_matched_content()
    {
        COWEL_ASSERT(!m_out.empty());

        AST_Instruction& latest = m_out.back();
        if (latest.type != AST_Instruction_Type::text) {
            return;
        }
        const std::size_t total_length = latest.n;
        COWEL_ASSERT(total_length != 0);

        const std::size_t text_begin = m_pos - total_length;

        const std::u8string_view last_text = m_source.substr(text_begin, total_length);
        const std::size_t last_non_white = last_text.find_last_not_of(u8" \t\r\n\f");
        const std::size_t non_white_length = last_non_white + 1;

        if (last_non_white == std::u8string_view::npos) {
            latest.type = AST_Instruction_Type::skip;
        }
        else if (non_white_length < total_length) {
            latest.n = non_white_length;
            m_out.push_back({ AST_Instruction_Type::skip, total_length - non_white_length });
        }
        else {
            COWEL_ASSERT(non_white_length == total_length);
        }
    }

    bool try_match_block()
    {
        if (!expect(u8'{')) {
            return false;
        }

        Scoped_Attempt a = attempt();

        const std::size_t block_instruction_index = m_out.size();
        m_out.push_back({ AST_Instruction_Type::push_block });

        // A possible optimization should be to find the closing brace and then run the parser
        // on the brace-enclosed block.
        // This would prevent ever discarding any matched content, but might not be worth it.
        //
        // I suspect we only have to discard if we reach the EOF unexpectedly,
        // and that seems like a broken file anyway.
        const std::size_t elements = match_content_sequence(Content_Context::block);

        if (!expect(u8'}')) {
            a.abort();
            m_out.push_back({ AST_Instruction_Type::error_unclosed_block });
            return false;
        }

        m_out[block_instruction_index].n = elements;
        m_out.push_back({ AST_Instruction_Type::pop_block });

        a.commit();
        return true;
    }
};

} // namespace

std::u8string_view ast_instruction_type_name(AST_Instruction_Type type)
{
    using enum AST_Instruction_Type;
    switch (type) {
        COWEL_ENUM_STRING_CASE8(skip);
        COWEL_ENUM_STRING_CASE8(escape);
        COWEL_ENUM_STRING_CASE8(text);
        COWEL_ENUM_STRING_CASE8(comment);
        COWEL_ENUM_STRING_CASE8(argument_name);
        COWEL_ENUM_STRING_CASE8(argument_equal);
        COWEL_ENUM_STRING_CASE8(argument_comma);
        COWEL_ENUM_STRING_CASE8(push_document);
        COWEL_ENUM_STRING_CASE8(pop_document);
        COWEL_ENUM_STRING_CASE8(push_directive);
        COWEL_ENUM_STRING_CASE8(pop_directive);
        COWEL_ENUM_STRING_CASE8(push_arguments);
        COWEL_ENUM_STRING_CASE8(pop_arguments);
        COWEL_ENUM_STRING_CASE8(push_argument);
        COWEL_ENUM_STRING_CASE8(pop_argument);
        COWEL_ENUM_STRING_CASE8(push_block);
        COWEL_ENUM_STRING_CASE8(pop_block);
        COWEL_ENUM_STRING_CASE8(error_unclosed_block);
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid type.");
}

void parse(std::pmr::vector<AST_Instruction>& out, std::u8string_view source)
{
    Parser { out, source }();
}

} // namespace cowel
