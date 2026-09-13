#pragma once
#include <string>

// The single shared list used both for ANSI constants below and for the named
// print functions generated in print.hpp.
#define IGN_DETAIL_FOR_EACH_COLOR_STYLE(X)                  \
    X(black, BLACK, "\033[30m")                            \
    X(red, RED, "\033[31m")                                \
    X(green, GREEN, "\033[32m")                            \
    X(yellow, YELLOW, "\033[33m")                          \
    X(blue, BLUE, "\033[34m")                              \
    X(magenta, MAGENTA, "\033[35m")                        \
    X(purple, PURPLE, "\033[38;5;93m")                     \
    X(pink, PINK, "\033[38;2;255;105;180m")                \
    X(cyan, CYAN, "\033[36m")                              \
    X(white, WHITE, "\033[37m")                            \
    X(grey, GREY, "\033[90m")                              \
    X(orange, ORANGE, "\033[38;5;208m")                    \
    X(bright_red, BRIGHT_RED, "\033[91m")                  \
    X(bright_green, BRIGHT_GREEN, "\033[92m")              \
    X(bright_yellow, BRIGHT_YELLOW, "\033[93m")            \
    X(bright_blue, BRIGHT_BLUE, "\033[94m")                \
    X(bright_magenta, BRIGHT_MAGENTA, "\033[95m")          \
    X(bright_purple, BRIGHT_PURPLE, "\033[38;5;141m")      \
    X(bright_pink, BRIGHT_PINK, "\033[38;2;255;182;193m")  \
    X(bright_cyan, BRIGHT_CYAN, "\033[96m")                \
    X(bright_white, BRIGHT_WHITE, "\033[97m")              \
    X(bold_black, BOLD_BLACK, "\033[1;30m")                \
    X(bold_red, BOLD_RED, "\033[1;31m")                    \
    X(bold_green, BOLD_GREEN, "\033[1;32m")                \
    X(bold_yellow, BOLD_YELLOW, "\033[1;33m")              \
    X(bold_blue, BOLD_BLUE, "\033[1;34m")                  \
    X(bold_magenta, BOLD_MAGENTA, "\033[1;35m")            \
    X(bold_purple, BOLD_PURPLE, "\033[1;38;5;93m")         \
    X(bold_pink, BOLD_PINK, "\033[1;38;2;255;105;180m")    \
    X(bold_cyan, BOLD_CYAN, "\033[1;36m")                  \
    X(bold_white, BOLD_WHITE, "\033[1;37m")                \
    X(bold_orange, BOLD_ORANGE, "\033[1;38;5;208m")        \
    X(bg_black, BG_BLACK, "\033[40m")                      \
    X(bg_red, BG_RED, "\033[41m")                          \
    X(bg_green, BG_GREEN, "\033[42m")                      \
    X(bg_yellow, BG_YELLOW, "\033[43m")                    \
    X(bg_blue, BG_BLUE, "\033[44m")                        \
    X(bg_magenta, BG_MAGENTA, "\033[45m")                  \
    X(bg_purple, BG_PURPLE, "\033[48;5;93m")               \
    X(bg_pink, BG_PINK, "\033[48;2;255;105;180m")          \
    X(bg_cyan, BG_CYAN, "\033[46m")                        \
    X(bg_white, BG_WHITE, "\033[47m")                      \
    X(bg_orange, BG_ORANGE, "\033[48;5;208m")              \
    X(bold, BOLD, "\033[1m")                               \
    X(dim, DIM, "\033[2m")                                 \
    X(italic, ITALIC, "\033[3m")                           \
    X(underline, UNDERLINE, "\033[4m")                     \
    X(blink, BLINK, "\033[5m")                             \
    X(reverse, REVERSE, "\033[7m")                         \
    X(hidden, HIDDEN, "\033[8m")                           \
    X(strike, STRIKE, "\033[9m")

namespace ign::color {

namespace detail {

inline int clamp_channel(int value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return value;
}

inline std::string rgb_code(const char* prefix, int red, int green, int blue) {
    return std::string("\033[") + prefix +
        std::to_string(clamp_channel(red)) + ';' +
        std::to_string(clamp_channel(green)) + ';' +
        std::to_string(clamp_channel(blue)) + 'm';
}

} // namespace detail

#define IGN_DETAIL_DEFINE_COLOR(NAME, CONSTANT, ANSI_CODE) \
    inline constexpr const char* CONSTANT = ANSI_CODE;

IGN_DETAIL_FOR_EACH_COLOR_STYLE(IGN_DETAIL_DEFINE_COLOR)

#undef IGN_DETAIL_DEFINE_COLOR

inline constexpr const char* RESET = "\033[0m";

inline std::string rgb(int red, int green, int blue) {
    return detail::rgb_code("38;2;", red, green, blue);
}

inline std::string bg_rgb(int red, int green, int blue) {
    return detail::rgb_code("48;2;", red, green, blue);
}

inline std::string bold_rgb(int red, int green, int blue) {
    return detail::rgb_code("1;38;2;", red, green, blue);
}

} // namespace ign::color
