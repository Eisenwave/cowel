#include <bit>
#include <memory_resource>
#include <new>

#include "cowel/util/assert.hpp"

#include "cowel/assets.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/cowel.h"
#include "cowel/document_content_behavior.hpp"
#include "cowel/document_generation.hpp"
#include "cowel/parse.hpp"
#include "cowel/services.hpp"
#include "cowel/ulight_highlighter.hpp"

namespace cowel {
namespace {

static_assert(std::is_same_v<cowel::File_Id, cowel_file_id>);

[[nodiscard]]
std::u8string_view as_u8string_view(cowel_string_view_u8 str)
{
    return { str.text, str.length };
}

[[nodiscard]]
std::u8string_view as_u8string_view(cowel_mutable_string_view_u8 str)
{
    return { str.text, str.length };
}

[[nodiscard]]
cowel_string_view_u8 as_cowel_string_view(std::u8string_view str)
{
    return { str.data(), str.length() };
}

struct Pointer_Memory_Resource final : std::pmr::memory_resource {
private:
    cowel_alloc_fn* m_alloc;
    void* m_alloc_data;
    cowel_free_fn* m_free;
    void* m_free_data;

public:
    [[nodiscard]]
    explicit Pointer_Memory_Resource(
        cowel_alloc_fn* alloc,
        void* alloc_data,
        cowel_free_fn* free,
        void* free_data
    )
        : m_alloc { alloc }
        , m_alloc_data { alloc_data }
        , m_free { free }
        , m_free_data { free_data }
    {
    }

    [[nodiscard]]
    explicit Pointer_Memory_Resource(const cowel_options_u8& options)
        : Pointer_Memory_Resource { options.alloc, options.alloc_data, options.free,
                                    options.free_data }
    {
    }

    [[nodiscard]]
    void* do_allocate(std::size_t bytes, std::size_t alignment) final
    {
        void* const result = m_alloc(m_alloc_data, bytes, alignment);
        if (!result) {
            throw std::bad_alloc();
        }
        return result;
    }

    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) noexcept final
    {
        m_free(m_free_data, p, bytes, alignment);
    }

    [[nodiscard]]
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept final
    {
        return dynamic_cast<const Pointer_Memory_Resource*>(&other) != nullptr;
    }
};

struct Global_Memory_Resource final : std::pmr::memory_resource {

    [[nodiscard]]
    void* do_allocate(std::size_t bytes, std::size_t alignment) final
    {
        void* const result = cowel_alloc(bytes, alignment);
        if (!result) {
            throw std::bad_alloc();
        }
        return result;
    }

    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) noexcept final
    {
        cowel_free(p, bytes, alignment);
    }

    [[nodiscard]]
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept final
    {
        return dynamic_cast<const Pointer_Memory_Resource*>(&other) != nullptr;
    }
};

struct File_Loader_Less {
    using is_transparent = void;

    [[nodiscard]]
    bool operator()(const auto& x, const auto& y) const noexcept
    {
        return std::u8string_view { x.data(), x.size() }
        < std::u8string_view { y.data(), y.size() };
    }
};

struct Loaded_File {
    cowel_file_id id;
    std::pmr::vector<char8_t> text;
};

[[nodiscard]]
constexpr File_Load_Error io_status_to_load_error(cowel_io_status error)
{
    COWEL_ASSERT(error != COWEL_IO_OK);
    switch (error) {
    case COWEL_IO_ERROR_READ: return File_Load_Error::read_error;
    case COWEL_IO_ERROR_NOT_FOUND: return File_Load_Error::not_found;
    case COWEL_IO_ERROR_PERMISSIONS: return File_Load_Error::permissions;
    default: return File_Load_Error::error;
    }
}

struct File_Loader_From_Options final : File_Loader {
private:
    cowel_load_file_fn_u8* m_load_file;
    void* m_load_file_data;

public:
    explicit File_Loader_From_Options(cowel_load_file_fn_u8* load_file, void* load_file_data)
        : m_load_file { load_file }
        , m_load_file_data { load_file_data }
    {
    }

    explicit File_Loader_From_Options(const cowel_options_u8& options)
        : File_Loader_From_Options { options.load_file, options.load_file_data }
    {
    }

    [[nodiscard]]
    Result<File_Entry, File_Load_Error> load(std::u8string_view path) final
    {
        if (!m_load_file) {
            return File_Load_Error::error;
        }
        const cowel_file_result_u8 result
            = m_load_file(m_load_file_data, as_cowel_string_view(path));
        if (result.status != COWEL_IO_OK) {
            return io_status_to_load_error(result.status);
        }
        return File_Entry { .id = result.id,
                            .source = as_u8string_view(result.data),
                            .name = path };
    }
};

struct Logger_From_Options final : Logger {
private:
    cowel_log_fn_u8* m_log;
    void* m_log_data;
    std::pmr::vector<cowel_string_view_u8> m_message_parts;

public:
    explicit Logger_From_Options(
        cowel_log_fn_u8* log,
        void* log_data,
        cowel_severity min_severity,
        std::pmr::memory_resource* memory
    )
        : Logger { Severity(min_severity) }
        , m_log { log }
        , m_log_data { log_data }
        , m_message_parts { memory }
    {
    }

    explicit Logger_From_Options(const cowel_options_u8& options, std::pmr::memory_resource* memory)
        : Logger_From_Options { options.log, options.log_data, options.min_log_severity, memory }
    {
    }

    void operator()(const Diagnostic& diagnostic) final
    {
        if (!m_log) {
            return;
        }
        m_message_parts.reserve(diagnostic.message.size());
        for (const std::u8string_view& part : diagnostic.message) {
            m_message_parts.push_back(as_cowel_string_view(part));
        }

        const cowel_diagnostic_u8 diagnostic_u8 {
            .severity = cowel_severity(diagnostic.severity),
            .id = as_cowel_string_view(diagnostic.id),
            .message_parts = m_message_parts.data(),
            .message_parts_size = m_message_parts.size(),
            .file = diagnostic.location.file,
            .begin = diagnostic.location.begin,
            .length = diagnostic.location.length,
            .line = diagnostic.location.line,
            .column = diagnostic.location.column,
        };
        m_log(m_log_data, &diagnostic_u8);
        m_message_parts.clear();
    }
};

[[nodiscard]]
cowel_mutable_string_view_u8 do_generate_html(const cowel_options_u8& options)
{
    Pointer_Memory_Resource pointer_memory { options };
    Global_Memory_Resource global_memory;
    auto* const memory = options.alloc && options.free
        ? static_cast<std::pmr::memory_resource*>(&pointer_memory)
        : static_cast<std::pmr::memory_resource*>(&global_memory);

    using output_type = std::pmr::vector<char8_t>;

    // We use placement new to prevent the destructor from std::vector from being called.
    // This lets us steal the storage that std::vector is managing.
    // Normally, this wouldn't be safe, but we use pmr and so we know that allocation can only
    // have been performed using that.
    alignas(output_type) std::byte output_storage[sizeof(output_type)];
    output_type& output = *new (output_storage) output_type { memory };

    Builtin_Directive_Set builtin_behavior {};
    Document_Content_Behavior document_behavior { builtin_behavior.get_macro_behavior() };
    Minimal_Content_Behavior minimal_behavior { builtin_behavior.get_macro_behavior() };
    Content_Behavior& root_behavior = options.mode == COWEL_MODE_MINIMAL
        ? static_cast<Content_Behavior&>(minimal_behavior)
        : static_cast<Content_Behavior&>(document_behavior);

    File_Loader_From_Options file_loader { options.load_file, options.load_file_data };
    Logger_From_Options logger { options, memory };
    static constinit Ulight_Syntax_Highlighter highlighter;

    const auto source = as_u8string_view(options.source);

    const std::pmr::vector<ast::Content> root_content = parse_and_build(
        source, File_Id {}, memory,
        [&](std::u8string_view id, File_Source_Span pos, std::u8string_view message) {
            logger(Diagnostic { Severity::error, id, pos, { &message, 1 } });
        }
    );

    const std::u8string_view highlight_theme_source = options.highlight_theme_json.length == 0
        ? assets::wg21_json
        : as_u8string_view(options.highlight_theme_json);

    const Generation_Options gen_options { .output = output,
                                           .root_behavior = root_behavior,
                                           .root_content = root_content,
                                           .builtin_behavior = builtin_behavior,
                                           .error_behavior = &builtin_behavior.get_error_behavior(),
                                           .highlight_theme_source = highlight_theme_source,
                                           .file_loader = file_loader,
                                           .logger = logger,
                                           .highlighter = highlighter,
                                           .memory = memory };

    generate_document(gen_options);

    // This is safe because output is never destroyed,
    // so we keep its allocation alive.
    return { output.data(), output.size() };
}

} // namespace
} // namespace cowel

extern "C" {

void* cowel_alloc(size_t size, size_t alignment) noexcept
{
    return ::operator new(size, std::align_val_t(alignment), std::nothrow);
}

void cowel_free(void* pointer, size_t size, size_t alignment) noexcept
{
    ::operator delete(pointer, size, std::align_val_t(alignment));
}

cowel_mutable_string_view cowel_generate_html(const cowel_options* options) noexcept
{
    try {
        COWEL_ASSERT(options != nullptr);
        const cowel_options_u8& options_u8 = std::bit_cast<cowel_options_u8>(*options);
        const cowel_mutable_string_view_u8 result_u8 = cowel_generate_html_u8(&options_u8);
        return std::bit_cast<cowel_mutable_string_view>(result_u8);
    } catch (...) {
        return {};
    }
}

cowel_mutable_string_view_u8 cowel_generate_html_u8(const cowel_options_u8* options) noexcept
{
    try {
        COWEL_ASSERT(options != nullptr);
        return cowel::do_generate_html(*options);
    } catch (...) {
        return {};
    }
}

#ifdef COWEL_EMSCRIPTEN
constinit cowel_options_u8 cowel_global_options;

constinit cowel_mutable_string_view_u8 cowel_global_result;

void cowel_global_generate_html(void) noexcept
{
    cowel_global_result = cowel_generate_html_u8(&cowel_global_options);
}
#endif
}
