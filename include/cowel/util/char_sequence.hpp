#ifndef COWEL_CHAR_SEQUENCE_HPP
#define COWEL_CHAR_SEQUENCE_HPP

#include <algorithm>
#include <cstring>
#include <optional>
#include <span>
#include <type_traits>

#include "cowel/util/assert.hpp"
#include "cowel/util/static_string.hpp"

namespace cowel {

template <typename F>
concept char_source8 = std::invocable<F&, std::span<char8_t>, std::size_t>;

struct Char_Sequence8_Iterator;
struct Char_Sequence8_Sentinel { };

/// @brief A non-owning, type-erased sized input range of characters.
struct Char_Sequence8 {
private:
    union Storage {
        const void* const_pointer;
        void* pointer;
        std::array<char8_t, sizeof(const_pointer)> code_units;
    };

    using Extractor_Function = Storage(Storage entity, std::span<char8_t> buffer, std::size_t n);

    static_assert(sizeof(char8_t) == 1);

    template <std::size_t N>
    static constexpr Storage to_storage(Static_String8<N> str)
    {
        static_assert(N <= sizeof(const void*)); // NOLINT(bugprone-sizeof-expression)
        decltype(Storage::code_units) result_array {};
        std::ranges::copy(str, result_array.data());
        return { .code_units = result_array };
    }

    template <std::size_t N>
    static constexpr Static_String8<N> to_static_string(Storage storage, std::size_t size)
    {
        static_assert(N <= sizeof(const void*)); // NOLINT(bugprone-sizeof-expression)
        std::array<char8_t, N> result_array {};
        std::ranges::copy_n(storage.code_units.data(), N, result_array.data());
        return Static_String8<N> { result_array, size };
    }

    template <std::size_t capacity>
    static constexpr Storage
    extract_from_static_string(Storage entity, std::span<char8_t> buffer, std::size_t n)
    {
        COWEL_ASSERT(n <= buffer.size());
        COWEL_ASSERT(n <= capacity);
        auto entity_as_string = to_static_string<capacity>(entity, n);
        std::ranges::copy(entity_as_string, buffer.data());
        entity_as_string.remove_prefix(n);
        return to_storage(entity_as_string);
    };

    static constexpr Storage
    extract_from_string_view(Storage storage, std::span<char8_t> buffer, std::size_t n)
    {
        COWEL_ASSERT(n <= buffer.size());
        const auto* const data = static_cast<const char8_t*>(storage.const_pointer);
        std::ranges::copy_n(data, std::ptrdiff_t(n), buffer.data());
        return { .const_pointer = data + n };
    };

public:
    using iterator = Char_Sequence8_Iterator;
    using value_type = char8_t;
    using difference_type = std::ptrdiff_t;

private:
    std::size_t m_size = 0;
    Extractor_Function* m_extract;
    Storage m_entity;

public:
    /// @brief Constructs an empty sequence.
    [[nodiscard]]
    constexpr Char_Sequence8() noexcept
        : m_extract { &extract_from_string_view }
        , m_entity { .const_pointer = nullptr }
    {
    }

    /// @brief Constructs a sequence with the same length and contents as the given `str`.
    [[nodiscard]]
    constexpr Char_Sequence8(std::u8string_view str) noexcept
        : m_size { str.size() }
        , m_extract { &extract_from_string_view }
        , m_entity { .const_pointer = str.data() }
    {
    }

    /// @brief Constructs a sequence with length `1`, containing a single code unit `c`.
    [[nodiscard]]
    constexpr Char_Sequence8(char8_t c) noexcept
        : m_size { 1 }
        , m_extract { &extract_from_static_string<1> }
        , m_entity { .code_units = { c } }
    {
    }

    /// @brief Constructs a sequence with length `n`, filled with `n` repetitions of `c`.
    [[nodiscard]]
    constexpr Char_Sequence8(std::size_t n, char8_t c) noexcept
        : m_size { n }
        , m_extract { [](Storage entity, std::span<char8_t> buffer, std::size_t n) -> Storage {
            COWEL_ASSERT(n <= buffer.size());
            std::ranges::fill_n(buffer.data(), std::ptrdiff_t(n), entity.code_units[0]);
            return entity;
        } }
        , m_entity { .code_units = { c } }
    {
    }

    /// @brief Constructs an empty sequence.
    [[nodiscard]]
    constexpr Char_Sequence8(Static_String8<0>) noexcept
        : Char_Sequence8()
    {
    }

    /// @brief Constructs a sequence with the same length and contents as `str`.
    /// `capacity <= sizeof(const void*)` shall be `true`.
    template <std::size_t capacity>
        requires(capacity != 0 && capacity <= sizeof(const void*))
    [[nodiscard]]
    constexpr Char_Sequence8(Static_String8<capacity> str) noexcept
        : m_size { str.size() }
        , m_extract { &extract_from_static_string<capacity> }
        , m_entity { to_storage(str) }
    {
    }

    /// @brief Constructs a sequence with length `size`,
    /// where characters are obtaining through calls to the given `extract` function.
    /// `std::remove_reference_t<F>` shall model `char_source8`.
    template <typename F>
    [[nodiscard]]
    // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward, bugprone-exception-escape)
    constexpr Char_Sequence8(std::size_t size, F&& extract) noexcept
        : m_size { size }
        , m_extract { nullptr }
        , m_entity { nullptr }
    {
        using U = std::remove_reference_t<F>;
        static_assert(char_source8<U>);

        // NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer)
        m_extract = [](Storage entity, std::span<char8_t> buffer, std::size_t n) -> Storage {
            COWEL_ASSERT(n <= buffer.size());
            U& entity_as_extract = *[&]() -> U* {
                // We need if constexpr here because we would otherwise
                // cast away constness.
                if constexpr (std::is_const_v<U>) {
                    return static_cast<U*>(entity.const_pointer);
                }
                else {
                    return static_cast<U*>(entity.pointer);
                }
            }();
            entity_as_extract(buffer, n);
            return entity;
        };
        if constexpr (std::is_const_v<U>) {
            m_entity = { .const_pointer = std::addressof(extract) };
        }
        else {
            m_entity = { .pointer = std::addressof(extract) };
        }
    }

    /// @brief Returns `true` iff the sequence is empty.
    [[nodiscard]]
    constexpr bool empty() const noexcept
    {
        return m_size == 0;
    }

    /// @brief Returns the size of the sequence, in code units.
    [[nodiscard]]
    constexpr std::size_t size() const noexcept
    {
        return m_size;
    }

    /// @brief Equivalent to `size()`.
    [[nodiscard]]
    constexpr std::size_t length() const noexcept
    {
        return m_size;
    }

    /// @brief Consumes `n = min(buffer.size(), size())` characters from the underlying sequence.
    /// After this operation, `size()` is reduced by `n`.
    constexpr std::size_t extract(std::span<char8_t> buffer)
    {
        const std::size_t n = std::min(buffer.size(), m_size);
        m_entity = m_extract(m_entity, buffer, n);
        m_size -= n;
        return n;
    }

    /// @brief Extracts a single character from the sequence.
    /// `empty()` shall be `true`.
    constexpr char8_t pop()
    {
        COWEL_ASSERT(!empty());
        char8_t c;
        extract({ &c, 1 });
        return c;
    }

    friend Char_Sequence8_Iterator;

    /// @brief Returns a naive input iterator over this sequence.
    [[nodiscard]]
    constexpr Char_Sequence8_Iterator begin() noexcept;

    /// @brief Returns an end sentinel for this sequence.
    [[nodiscard]]
    constexpr Char_Sequence8_Sentinel end() noexcept
    {
        return {};
    }

    /// @brief Attempts to return a pointer to the contiguous data
    /// that the sequence was originally constructed with.
    /// That is the case if the sequence was constructed using a `u8string_view`,
    /// a single `char8_t`, or `Static_String8`.
    /// If none of these constructors were used, returns `nullptr`.
    ///
    /// If `empty()` is `true`, may return `nullptr` even if the data is contiguous.
    [[nodiscard]]
    constexpr const char8_t* as_contiguous() const noexcept
    {
        if (m_extract == &extract_from_string_view) {
            return static_cast<const char8_t*>(m_entity.const_pointer);
        }
        if constexpr (sizeof(const void*) >= 2) {
            if (m_extract == &extract_from_static_string<1> //
                || m_extract == &extract_from_static_string<2>) {
                return m_entity.code_units.data();
            }
        }
        if constexpr (sizeof(const void*) >= 4) {
            if (m_extract == &extract_from_static_string<3> //
                || m_extract == &extract_from_static_string<4>) {
                return m_entity.code_units.data();
            }
        }
        if constexpr (sizeof(const void*) >= 8) {
            if (m_extract == &extract_from_static_string<5> //
                || m_extract == &extract_from_static_string<6> //
                || m_extract == &extract_from_static_string<7> //
                || m_extract == &extract_from_static_string<8>) {
                return m_entity.code_units.data();
            }
        }
        return nullptr;
    }

    /// @brief If `as_contiguous` returns a non-null pointer,
    /// returns `std::u8string_view{as_contiguous(), size()}`.
    /// Otherwise, returns an empty string view.
    [[nodiscard]]
    constexpr std::u8string_view as_string_view() const noexcept
    {
        const char8_t* const result = as_contiguous();
        return result == nullptr ? std::u8string_view {} : std::u8string_view { result, m_size };
    }
};

/// @brief A naive input iterator over a `Char_Sequence8`.
/// This is suboptimal because it traverses character-by-character.
/// Prefer to use bulk extraction.
struct Char_Sequence8_Iterator {
    using difference_type = std::ptrdiff_t;
    using value_type = char8_t;

private:
    Char_Sequence8* m_chars;
    std::optional<char8_t> m_current;

public:
    [[nodiscard]]
    constexpr explicit Char_Sequence8_Iterator(Char_Sequence8& chars)
        : m_chars { &chars }
    {
        if (!m_chars->empty()) {
            m_current = m_chars->pop();
        }
    }

    [[nodiscard]]
    constexpr char8_t operator*() const
    {
        return m_current.value();
    }

    constexpr Char_Sequence8_Iterator& operator++()
    {
        if (m_chars->empty()) {
            m_current.reset();
        }
        else {
            m_current = m_chars->pop();
        }
        return *this;
    }

    constexpr void operator++(int)
    {
        ++*this;
    }

    [[nodiscard]]
    friend constexpr bool
    operator==(const Char_Sequence8_Iterator& it, Char_Sequence8_Sentinel) noexcept
    {
        return !it.m_current.has_value();
    }
};

constexpr Char_Sequence8_Iterator Char_Sequence8::begin() noexcept
{
    return Char_Sequence8_Iterator { *this };
}

static_assert(std::input_iterator<Char_Sequence8_Iterator>);
static_assert(std::ranges::input_range<Char_Sequence8>);
static_assert(std::is_trivially_copyable_v<Char_Sequence8>);

} // namespace cowel

#endif
