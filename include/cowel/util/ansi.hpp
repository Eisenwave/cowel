#ifndef COWEL_ANSI_HPP
#define COWEL_ANSI_HPP

#include <string_view>

namespace cowel::ansi {

// Regular colors

inline constexpr std::u8string_view black = u8"\x1B[30m";
inline constexpr std::u8string_view red = u8"\x1B[31m";
inline constexpr std::u8string_view green = u8"\x1B[32m";
inline constexpr std::u8string_view yellow = u8"\x1B[33m";
inline constexpr std::u8string_view blue = u8"\x1B[34m";
inline constexpr std::u8string_view magenta = u8"\x1B[35m";
inline constexpr std::u8string_view cyan = u8"\x1B[36m";
inline constexpr std::u8string_view white = u8"\x1B[37m";

// High-intensity colors

inline constexpr std::u8string_view h_black = u8"\x1B[0;90m";
inline constexpr std::u8string_view h_red = u8"\x1B[0;91m";
inline constexpr std::u8string_view h_green = u8"\x1B[0;92m";
inline constexpr std::u8string_view h_yellow = u8"\x1B[0;93m";
inline constexpr std::u8string_view h_blue = u8"\x1B[0;94m";
inline constexpr std::u8string_view h_magenta = u8"\x1B[0;95m";
inline constexpr std::u8string_view h_cyan = u8"\x1B[0;96m";
inline constexpr std::u8string_view h_white = u8"\x1B[0;97m";

// Other sequences

inline constexpr std::u8string_view reset = u8"\033[0m";

}; // namespace cowel::ansi

#endif
