#ifndef COWEL_H
#define COWEL_H

#ifdef __EMSCRIPTEN__
#define COWEL_EMSCRIPTEN 1
#define COWEL_IF_EMSCRIPTEN(...) __VA_ARGS__
#else
#define COWEL_IF_EMSCRIPTEN(...)
#endif

#ifdef __GNUC__
#define COWEL_EXPORT [[gnu::used]]
#else
#define COWEL_EXPORT
#endif

#ifdef __cplusplus
#define COWEL_NOEXCEPT noexcept
#else
#define COWEL_NOEXCEPT
#endif

#include <stddef.h> // NOLINT(modernize-deprecated-headers)

#ifdef __cplusplus
extern "C" {
#endif

// NOLINTNEXTLINE(performance-enum-size)
enum cowel_mode {
    /// @brief Document generation with the usual `<head>` body etc. orchestration.
    COWEL_MODE_DOCUMENT,
    /// @brief Minimalistic generation.
    /// Content is directly written into the resulting HTML,
    /// without any pre-defined styles, no `<head>, `<body>`, etc.
    COWEL_MODE_MINIMAL,
};

// NOLINTNEXTLINE(performance-enum-size)
enum cowel_io_status {
    /// @brief The operation succeeded.
    COWEL_IO_OK,
    /// @brief The operation failed with a generic I/O error.
    COWEL_IO_ERROR,
    /// @brief The operation failed because a file could not be found.
    COWEL_IO_ERROR_NOT_FOUND,
    /// @brief
    COWEL_IO_ERROR_READ,
    COWEL_IO_ERROR_PERMISSIONS,
};

// NOLINTNEXTLINE(performance-enum-size)
enum cowel_severity {
    COWEL_SEVERITY_MIN = 0,
    COWEL_SEVERITY_DEBUG = 0,
    COWEL_SEVERITY_SOFT_WARNING = 1,
    COWEL_SEVERITY_WARNING = 2,
    COWEL_SEVERITY_ERROR = 3,
    COWEL_SEVERITY_MAX = 3,
    COWEL_SEVERITY_NONE = 4,
};

/// @brief A container for a string and a length.
/// The string does not have to be null-terminated.
struct cowel_string_view {
    /// @brief Pointer to text UTF-8 text data.
    const char* text;
    /// @brief Length of text data in bytes/UTF-8 code units.
    size_t length;
};

/// @brief See `cowel_string_view`.
struct cowel_string_view_u8 {
    const char8_t* text;
    size_t length;
};

/// @brief Like `cowel_string_view`, but storing a pointer to mutable text instead.
struct cowel_mutable_string_view {
    char* text;
    size_t length;
};

/// @brief See `cowel_mutable_string_view`.
struct cowel_mutable_string_view_u8 {
    char8_t* text;
    size_t length;
};

typedef int cowel_file_id; // NOLINT(modernize-use-using)

/// @brief Data associated with a diagnostic message.
struct cowel_diagnostic {
    /// @brief The level of severity for this diagnostic.
    cowel_severity severity;
    /// @brief A unique identifier for the diagnostic.
    cowel_string_view id;
    /// @brief The diagnostic message.
    cowel_string_view message;
    /// @brief The name of the file in which the diagnostic occurred.
    /// This string may become invalid immediately after the logger was invoked.
    cowel_file_id file;
    /// @brief The code unit within the file where the diagnostic occurred.
    size_t begin;
    /// @brief The (possibly zero, in past-the-file cases) amount of code units
    /// starting at `begin` where the diagnostic occurred.
    /// If both `begin` and `length` are zero,
    /// the diagnostic is considered to address the entire file.
    size_t length;
    /// @brief The line index (starting at zero) where the diagnostic occurred.
    size_t line;
    /// @brief The offset from the start of the line in code units.
    size_t column;
};

/// @brief See `cowel_diagnostic`.
struct cowel_diagnostic_u8 {
    cowel_severity severity;
    cowel_string_view_u8 id;
    cowel_string_view_u8 message;
    cowel_file_id file;
    size_t begin;
    size_t length;
    size_t line;
    size_t column;
};

struct cowel_file_result {
    /// @brief The status of loading the file, indicating success or failure.
    cowel_io_status status;
    /// @brief The pointer to the loaded file data,
    /// allocated using `cowel_options::alloc`.
    /// If loading failed, this is null.
    cowel_mutable_string_view data;
    /// @brief A unique identifier for the loaded file.
    /// The identifier zero refers to the main file,
    /// which is not actually loaded, but whose source is provided within `cowel_options`.
    cowel_file_id id;
};

/// @brief See `cowel_file_result`.
struct cowel_file_result_u8 {
    cowel_io_status status;
    cowel_mutable_string_view_u8 data;
    cowel_file_id id;
};

// NOLINTNEXTLINE(modernize-use-using)
typedef void* cowel_alloc_fn(void* data, size_t size, size_t alignment);

// NOLINTNEXTLINE(modernize-use-using)
typedef void cowel_free_fn(void* data, void* pointer, size_t size, size_t alignment);

// NOLINTNEXTLINE(modernize-use-using)
typedef cowel_file_result cowel_load_file_fn(void* data, cowel_string_view path);
// NOLINTNEXTLINE(modernize-use-using)
typedef cowel_file_result_u8 cowel_load_file_fn_u8(void* data, cowel_string_view_u8 path);

// NOLINTNEXTLINE(modernize-use-using)
typedef void cowel_log_fn(void* data, const cowel_diagnostic* diagnostic);
// NOLINTNEXTLINE(modernize-use-using)
typedef void cowel_log_fn_u8(void* data, const cowel_diagnostic_u8* diagnostic);

struct cowel_options {
    /// @brief The cowel source code.
    cowel_string_view source;
    /// @brief The (JSON) source for the highlight theme.
    /// If this string is empty, the builtin theme is used.
    cowel_string_view highlight_theme_source;
    /// @brief The processing mode.
    cowel_mode mode;
    /// @brief The minimum (inclusive) level that log messages must have to be logged.
    cowel_severity min_log_severity;
    /// @brief Reserved space.
    void* reserved_0[4];

    /// @brief A pointer to a function which performs memory allocation.
    /// If this is `nullptr`, `cowel_allloc` is used instead.
    cowel_alloc_fn* alloc;
    /// @brief Additional data passed into `alloc`.
    void* data;

    /// @brief A pointer to a function which frees the memory obtained from `free`.
    /// If this is `nullptr`, `cowel_free` is used instead.
    cowel_free_fn* free;
    /// @brief Additional data passed into `free`.
    void* free_data;

    /// @brief Attempts to load a file at the specified path.
    cowel_load_file_fn* load_file;
    /// @brief Additional data passed into `load_file`.
    void* load_file_data;

    /// @brief Emits a diagnostic.
    cowel_log_fn* log;
    /// @brief Additional data passed into `log`.
    void* log_data;

    /// @brief Reserved space.
    void* reserved_1[4];
};

/// @brief See `cowel_options_u8`.
struct cowel_options_u8 {
    cowel_string_view_u8 source;
    cowel_string_view_u8 highlight_theme_json;
    cowel_mode mode;
    cowel_severity min_log_severity;
    void* reserved_0[4];

    cowel_alloc_fn* alloc;
    void* alloc_data;
    cowel_free_fn* free;
    void* free_data;
    cowel_load_file_fn_u8* load_file;
    void* load_file_data;
    cowel_log_fn_u8* log;
    void* log_data;
    void* reserved_1[4];
};

static_assert(sizeof(cowel_string_view) == sizeof(cowel_string_view_u8));
static_assert(sizeof(cowel_mutable_string_view) == sizeof(cowel_mutable_string_view_u8));
static_assert(sizeof(cowel_diagnostic) == sizeof(cowel_diagnostic_u8));
static_assert(sizeof(cowel_options) == sizeof(cowel_options_u8));

/// @brief The default function for performing allocation.
COWEL_EXPORT
void* cowel_alloc(size_t size, size_t alignment) COWEL_NOEXCEPT;

/// @brief Frees allocations previously allocated by `cowel_alloc`.
COWEL_EXPORT
void cowel_free(void* pointer, size_t size, size_t alignment) COWEL_NOEXCEPT;

/// @brief Runs document generation using the specified options.
/// The result is a string containing the generated HTML,
/// allocated using `options.alloc`,
/// or using `cowel_alloc` if `options.alloc` is null.
COWEL_EXPORT
cowel_mutable_string_view cowel_generate_html(const cowel_options* options) COWEL_NOEXCEPT;

/// @brief See `cowel_run`.
COWEL_EXPORT
cowel_mutable_string_view_u8 cowel_generate_html_u8(const cowel_options_u8* options) COWEL_NOEXCEPT;

/// @brief In WASM, there is no multi-threading,
/// and it is somewhat tedious to allocate individual objects,
/// so we add a bunch of convenience to make the library easier to use in that environment.
#ifdef COWEL_EMSCRIPTEN
extern cowel_options_u8 cowel_global_options;

extern cowel_mutable_string_view_u8 cowel_global_result;

COWEL_EXPORT
void cowel_global_generate_html(void) COWEL_NOEXCEPT;
#endif

#ifdef __cplusplus
}
#endif

#endif
