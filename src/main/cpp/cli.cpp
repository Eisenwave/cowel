#include <cstdio>
#include <filesystem>
#include <memory_resource>
#include <string_view>
#include <vector>

#include "cowel/cowel.h"
#include "cowel/services.hpp"
#include "cowel/util/annotated_string.hpp"
#include "cowel/util/ansi.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/assets.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/print.hpp"

namespace cowel {
namespace {

using cowel::as_u8string_view;

[[nodiscard]]
cowel_string_view_u8 as_cowel_string_view(std::u8string_view str)
{
    return { str.data(), str.length() };
}

[[nodiscard]]
std::u8string_view as_u8string_view(cowel_string_view_u8 str)
{
    return { str.text, str.length };
}

[[nodiscard]]
constexpr cowel_io_status io_error_to_io_status(IO_Error_Code error)
{
    switch (error) {
    case IO_Error_Code::read_error: return COWEL_IO_ERROR_READ;
    case IO_Error_Code::cannot_open: return COWEL_IO_ERROR_NOT_FOUND;
    default: return COWEL_IO_ERROR;
    }
}

struct Owned_File_Entry {
    std::pmr::vector<char8_t> name;
    std::pmr::vector<char8_t> text;
};

struct Relative_File_Loader {
    std::filesystem::path base;
    std::pmr::vector<Owned_File_Entry> entries;

    explicit Relative_File_Loader(std::filesystem::path&& base, std::pmr::memory_resource* memory)
        : base { std::move(base) }
        , entries { memory }
    {
    }

    [[nodiscard]]
    cowel_file_result_u8 load(std::u8string_view path)
    {
        const std::filesystem::path relative { path, std::filesystem::path::generic_format };
        const std::filesystem::path resolved = base / relative;

        std::pmr::memory_resource* const memory = entries.get_allocator().resource();
        std::pmr::vector<char8_t> path_copy { path.begin(), path.end(), memory };
        Result<std::pmr::vector<char8_t>, IO_Error_Code> result
            = load_utf8_file(resolved.generic_u8string(), memory);

        if (!result) {
            // Even though loading failed, we store the file as an entry
            // so that we can later get its name during logging.
            entries.emplace_back(std::move(path_copy));
            // Using entries.size() as the id is correct because id 0 refers to the main file,
            // whereas the first loaded file gets id 1.
            return {
                .status = io_error_to_io_status(result.error()),
                .data = {},
                .id = File_Id(entries.size()),
            };
        }

        auto& entry = entries.emplace_back(std::move(path_copy), std::move(*result));
        return cowel_file_result_u8 {
            .status = COWEL_IO_OK,
            .data = { entry.text.data(), entry.text.size() },
            .id = File_Id(entries.size()),
        };
    }
};

[[nodiscard]]
std::u8string_view severity_highlight(Severity severity)
{
    using enum Severity;
    switch (severity) {
    case debug: return ansi::h_black;
    case soft_warning: return ansi::green;
    case warning: return ansi::h_yellow;
    case error: return ansi::h_red;
    default: return ansi::magenta;
    }
}

[[nodiscard]]
std::u8string_view severity_tag(Severity severity)
{
    using enum Severity;
    switch (severity) {
    case debug: return u8"DEBUG";
    case soft_warning: return u8"SOFTWARN";
    case warning: return u8"WARNING";
    case error: return u8"ERROR";
    default: return u8"???";
    }
}

[[nodiscard]]
constexpr File_Source_Span as_file_source_span(const cowel_diagnostic_u8& diagnostic)
{
    return {
        { { .line = diagnostic.line, .column = diagnostic.column, .begin = diagnostic.begin },
          diagnostic.length },
        diagnostic.file,
    };
}

struct Stderr_Logger {
    const Relative_File_Loader& file_loader;
    const std::u8string_view main_file_name;
    const std::u8string_view main_file_source;
    Diagnostic_String out;
    bool any_errors = false;

    [[nodiscard]]
    constexpr Stderr_Logger(
        Relative_File_Loader& file_loader,
        std::u8string_view main_file_name,
        std::u8string_view main_file_source,
        std::pmr::memory_resource* memory
    )
        : file_loader { file_loader }
        , main_file_name { main_file_name }
        , main_file_source { main_file_source }
        , out { memory }
    {
    }

    // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
    void operator()(const cowel_diagnostic_u8& diagnostic)
    {
        const auto severity = Severity(diagnostic.severity);
        any_errors |= severity >= Severity::error;

        const auto file_entry = [&] -> File_Entry {
            if (diagnostic.file <= 0) {
                return { .id = diagnostic.file,
                         .source = main_file_source,
                         .name = main_file_name };
            }
            const Owned_File_Entry& result
                = file_loader.entries.at(std::size_t(diagnostic.file - 1));
            return { .id = diagnostic.file,
                     .source = as_u8string_view(result.text),
                     .name = as_u8string_view(result.name) };
        }();

        const File_Source_Span location = as_file_source_span(diagnostic);

        out.append(severity_highlight(severity));
        out.append(severity_tag(severity));
        out.append(ansi::reset);
        out.append(u8": ");
        if (diagnostic.length == 0) {
            print_location_of_file(out, file_entry.name);
        }
        else {
            print_file_position(out, file_entry.name, location);
        }
        out.append(u8' ');
        out.append(as_u8string_view(diagnostic.message));
        out.append(ansi::h_black);
        out.append(u8" [");
        out.append(as_u8string_view(diagnostic.id));
        out.append(u8']');
        out.append(ansi::reset);
        out.append(u8'\n');
        if (diagnostic.length != 0) {
            print_affected_line(out, file_entry.source, location);
        }
        print_code_string_stderr(out);
        out.clear();
    }
};

int main(int argc, const char* const* argv)
{
    if (argc < 1) {
        return EXIT_FAILURE;
    }
    const std::u8string_view program_name { reinterpret_cast<const char8_t*>(argv[0]) };

    std::pmr::unsynchronized_pool_resource memory;

    if (argc < 3) {
        Basic_Annotated_String<char8_t, Diagnostic_Highlight> error { &memory };
        error.append(u8"Usage: ");
        error.append(program_name);
        error.append(u8" IN_FILE.cowel OUT_FILE.html\n");
        print_code_string_stderr(error);
        return EXIT_FAILURE;
    }

    const std::string_view in_path = argv[1];
    const std::u8string_view in_path_u8 = as_u8string_view(in_path);
    auto in_path_directory = std::filesystem::path { in_path }.parent_path();

    const std::string_view out_path = argv[2];
    const std::u8string_view out_path_u8 = as_u8string_view(out_path);

    const Result<std::pmr::vector<char8_t>, IO_Error_Code> in_text
        = load_utf8_file(in_path_u8, &memory);
    if (!in_text) {
        Diagnostic_String error { &memory };
        print_io_error(error, in_path_u8, in_text.error());
        print_code_string_stderr(error);
        return EXIT_FAILURE;
    }
    const auto in_source = as_u8string_view(*in_text);

    Relative_File_Loader file_loader { std::move(in_path_directory), &memory };
    Stderr_Logger logger { file_loader, in_path_u8, in_source, &memory };

    // TODO: allow custom ulight themes instead of hardcoding wg21.json

    const cowel_options_u8 options {
        .source = as_cowel_string_view(in_source),
        .highlight_theme_json = as_cowel_string_view(assets::wg21_json),
        .mode = COWEL_MODE_DOCUMENT,
        .min_log_severity = COWEL_SEVERITY_MIN,
        .reserved_0 {},
        .alloc = [](void* self, std::size_t size, std::size_t alignment) -> void* {
            auto* const memory = static_cast<std::pmr::memory_resource*>(self);
            return memory->allocate(size, alignment);
        },
        .alloc_data = &memory,
        .free = [](void* self, void* pointer, std::size_t size, std::size_t alignment) -> void {
            auto* const memory = static_cast<std::pmr::memory_resource*>(self);
            memory->deallocate(pointer, size, alignment);
        },
        .free_data = &memory,
        .load_file = [](void* self, cowel_string_view_u8 path) -> cowel_file_result_u8 {
            auto* const file_loader = static_cast<Relative_File_Loader*>(self);
            return file_loader->load(as_u8string_view(path));
        },
        .load_file_data = &file_loader,
        .log = [](void* self, const cowel_diagnostic_u8* diagnostic) -> void {
            auto* const logger = static_cast<Stderr_Logger*>(self);
            (*logger)(*diagnostic);
        },
        .log_data = &logger,
        .reserved_1 {}
    };

    cowel_mutable_string_view_u8 result = cowel_generate_html_u8(&options);

    const auto out_file = fopen_unique(out_path.data(), "wb");
    if (!out_file) {
        Diagnostic_String error { &memory };
        print_location_of_file(error, out_path_u8);
        error.append(u8" Failed to open file.");
        print_code_string_stderr(error);
        return EXIT_FAILURE;
    }

    if (result.text != nullptr) {
        std::fwrite(result.text, 1, result.length, out_file.get());
        memory.deallocate(result.text, result.length, alignof(char8_t));
    }

    return logger.any_errors ? EXIT_FAILURE : EXIT_SUCCESS;
}

} // namespace
} // namespace cowel

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, const char* const* argv)
{
    return cowel::main(argc, argv);
}
