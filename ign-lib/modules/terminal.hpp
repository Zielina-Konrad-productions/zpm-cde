#pragma once

#include <cstddef>
#include <iostream>
#include <ostream>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace ign::terminal {

inline constexpr const char* CLEAR_SCREEN = "\033[2J";
inline constexpr const char* CLEAR_SCROLLBACK = "\033[3J";
inline constexpr const char* CURSOR_HOME = "\033[H";
inline constexpr const char* CLEAR_LINE = "\033[2K";
inline constexpr const char* CLEAR_LINE_RIGHT = "\033[K";
inline constexpr const char* CLEAR_LINE_LEFT = "\033[1K";
inline constexpr const char* HIDE_CURSOR = "\033[?25l";
inline constexpr const char* SHOW_CURSOR = "\033[?25h";
inline constexpr const char* SAVE_CURSOR = "\033[s";
inline constexpr const char* RESTORE_CURSOR = "\033[u";

struct terminal_size {
    std::size_t rows = 0;
    std::size_t columns = 0;
    bool available = false;
};

namespace detail {

inline std::size_t one_based(std::size_t value) {
    return value == 0 ? 1 : value;
}

#if defined(_WIN32)
inline bool enable_ansi_for_handle(DWORD handle_name) {
    const HANDLE handle = GetStdHandle(handle_name);
    if (handle == INVALID_HANDLE_VALUE || handle == nullptr) return true;

    DWORD mode = 0;
    if (GetConsoleMode(handle, &mode) == 0) return true;
    if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0) return true;

    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    return SetConsoleMode(handle, mode) != 0;
}
#endif

inline bool enable_ansi_once() {
#if defined(_WIN32)
    static const bool enabled =
        enable_ansi_for_handle(STD_OUTPUT_HANDLE) &&
        enable_ansi_for_handle(STD_ERROR_HANDLE);
    return enabled;
#else
    return true;
#endif
}

inline void prepare_terminal() {
    (void)enable_ansi_once();
}

inline void write_code(std::ostream& stream, const char* code) {
    prepare_terminal();
    stream << code << std::flush;
}

inline void write_count(
    std::ostream& stream,
    std::size_t count,
    char command) {
    if (count == 0) return;

    prepare_terminal();
    stream << "\033[" << count << command << std::flush;
}

inline std::size_t right_aligned_column(
    std::size_t terminal_columns,
    std::size_t width,
    std::size_t margin) {
    if (terminal_columns <= margin) return 1;

    const std::size_t rightmost = terminal_columns - margin;
    if (width == 0) return rightmost;
    if (width >= rightmost) return 1;

    return rightmost - width + 1;
}

inline std::size_t middle_position(std::size_t size) {
    return size == 0 ? 1 : (size + 1) / 2;
}

inline std::size_t centered_column(
    std::size_t terminal_columns,
    std::size_t width) {
    if (terminal_columns == 0 || width >= terminal_columns) return 1;

    return ((terminal_columns - width) / 2) + 1;
}

#if defined(_WIN32)
inline terminal_size query_size_for_handle(DWORD handle_name) {
    const HANDLE handle = GetStdHandle(handle_name);
    if (handle == INVALID_HANDLE_VALUE || handle == nullptr) return {};

    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (GetConsoleScreenBufferInfo(handle, &info) == 0) return {};

    const SHORT columns = static_cast<SHORT>(
        info.srWindow.Right - info.srWindow.Left + 1);
    const SHORT rows = static_cast<SHORT>(
        info.srWindow.Bottom - info.srWindow.Top + 1);

    if (rows <= 0 || columns <= 0) return {};

    return {
        static_cast<std::size_t>(rows),
        static_cast<std::size_t>(columns),
        true
    };
}
#else
inline terminal_size query_size_for_fd(int fd) {
    winsize size{};
    if (::ioctl(fd, TIOCGWINSZ, &size) != 0) return {};
    if (size.ws_row == 0 || size.ws_col == 0) return {};

    return {
        static_cast<std::size_t>(size.ws_row),
        static_cast<std::size_t>(size.ws_col),
        true
    };
}
#endif

inline terminal_size query_size() {
#if defined(_WIN32)
    terminal_size result = query_size_for_handle(STD_OUTPUT_HANDLE);
    if (result.available) return result;

    return query_size_for_handle(STD_ERROR_HANDLE);
#else
    terminal_size result = query_size_for_fd(STDOUT_FILENO);
    if (result.available) return result;

    result = query_size_for_fd(STDERR_FILENO);
    if (result.available) return result;

    return query_size_for_fd(STDIN_FILENO);
#endif
}

} // namespace detail

// Enables ANSI escape-code processing on Windows consoles. On other platforms,
// and for redirected Windows streams, this is a no-op that returns true.
inline bool enable_ansi() {
    return detail::enable_ansi_once();
}

// Clears the visible terminal screen and moves the cursor to the top-left.
inline void clear_screen(std::ostream& stream = std::cout) {
    detail::prepare_terminal();
    stream << CLEAR_SCREEN << CURSOR_HOME << std::flush;
}

// Clears the visible screen, clears scrollback where supported, and goes home.
inline void clear(std::ostream& stream = std::cout) {
    detail::prepare_terminal();
    stream << CLEAR_SCREEN << CLEAR_SCROLLBACK << CURSOR_HOME << std::flush;
}

// Clears the whole current line and returns the cursor to column 1.
inline void clear_line(std::ostream& stream = std::cout) {
    detail::prepare_terminal();
    stream << '\r' << CLEAR_LINE << std::flush;
}

inline void clear_line_right(std::ostream& stream = std::cout) {
    detail::write_code(stream, CLEAR_LINE_RIGHT);
}

inline void clear_line_left(std::ostream& stream = std::cout) {
    detail::write_code(stream, CLEAR_LINE_LEFT);
}

inline terminal_size size() {
    return detail::query_size();
}

inline std::size_t rows(std::size_t fallback = 24) {
    const terminal_size current = size();
    return current.available ? current.rows : fallback;
}

inline std::size_t columns(std::size_t fallback = 80) {
    const terminal_size current = size();
    return current.available ? current.columns : fallback;
}

namespace cursor {

inline void home(std::ostream& stream = std::cout) {
    detail::write_code(stream, CURSOR_HOME);
}

inline void top_left(std::ostream& stream = std::cout) {
    home(stream);
}

// Moves the cursor to a 1-based row and column. Zero is treated as one.
inline void to(
    std::size_t row,
    std::size_t column,
    std::ostream& stream = std::cout) {
    detail::prepare_terminal();
    stream << "\033[" << detail::one_based(row) << ';'
           << detail::one_based(column) << 'H' << std::flush;
}

inline void top_left(
    std::size_t column,
    std::ostream& stream = std::cout) {
    to(1, column, stream);
}

// Moves to a 1-based column on the current line. Zero is treated as one.
inline void column(
    std::size_t column,
    std::ostream& stream = std::cout) {
    detail::prepare_terminal();
    stream << "\033[" << detail::one_based(column) << 'G' << std::flush;
}

inline void up(
    std::size_t count = 1,
    std::ostream& stream = std::cout) {
    detail::write_count(stream, count, 'A');
}

inline void down(
    std::size_t count = 1,
    std::ostream& stream = std::cout) {
    detail::write_count(stream, count, 'B');
}

inline void right(
    std::size_t count = 1,
    std::ostream& stream = std::cout) {
    detail::write_count(stream, count, 'C');
}

inline void left(
    std::size_t count = 1,
    std::ostream& stream = std::cout) {
    detail::write_count(stream, count, 'D');
}

inline void left_edge(std::ostream& stream = std::cout) {
    detail::prepare_terminal();
    stream << '\r' << std::flush;
}

inline void right_edge(std::ostream& stream = std::cout) {
    const terminal_size current = size();
    if (current.available) {
        column(current.columns, stream);
        return;
    }

    right(9999, stream);
}

// Moves so a text block of width characters ends at the right edge.
inline void right_edge(
    std::size_t width,
    std::size_t margin = 0,
    std::ostream& stream = std::cout) {
    column(
        detail::right_aligned_column(columns(), width, margin),
        stream
    );
}

// Moves to row and to the column where width characters fit on the right.
inline void to_right(
    std::size_t row,
    std::size_t width,
    std::size_t margin = 0,
    std::ostream& stream = std::cout) {
    to(
        row,
        detail::right_aligned_column(columns(), width, margin),
        stream
    );
}

inline void to_middle(
    std::size_t row,
    std::size_t width = 1,
    std::ostream& stream = std::cout) {
    to(row, detail::centered_column(columns(), width), stream);
}

inline void middle(std::ostream& stream = std::cout) {
    to(detail::middle_position(rows()), detail::middle_position(columns()),
       stream);
}

inline void middle(
    std::size_t width,
    std::ostream& stream = std::cout) {
    to_middle(detail::middle_position(rows()), width, stream);
}

inline void top_middle(std::ostream& stream = std::cout) {
    to(1, detail::middle_position(columns()), stream);
}

inline void top_middle(
    std::size_t width,
    std::ostream& stream = std::cout) {
    to_middle(1, width, stream);
}

inline void top_right(std::ostream& stream = std::cout) {
    to(1, columns(), stream);
}

inline void top_right(
    std::size_t width,
    std::size_t margin = 0,
    std::ostream& stream = std::cout) {
    to_right(1, width, margin, stream);
}

inline void bottom_left(std::ostream& stream = std::cout) {
    to(rows(), 1, stream);
}

inline void bottom_left(
    std::size_t column,
    std::ostream& stream = std::cout) {
    to(rows(), column, stream);
}

inline void bottom_middle(std::ostream& stream = std::cout) {
    to(rows(), detail::middle_position(columns()), stream);
}

inline void bottom_middle(
    std::size_t width,
    std::ostream& stream = std::cout) {
    to_middle(rows(), width, stream);
}

inline void bottom_right(std::ostream& stream = std::cout) {
    const terminal_size current = size();
    to(
        current.available ? current.rows : 24,
        current.available ? current.columns : 80,
        stream
    );
}

inline void bottom_right(
    std::size_t width,
    std::size_t margin = 0,
    std::ostream& stream = std::cout) {
    to_right(rows(), width, margin, stream);
}

inline void hide(std::ostream& stream = std::cout) {
    detail::write_code(stream, HIDE_CURSOR);
}

inline void show(std::ostream& stream = std::cout) {
    detail::write_code(stream, SHOW_CURSOR);
}

inline void save(std::ostream& stream = std::cout) {
    detail::write_code(stream, SAVE_CURSOR);
}

inline void restore(std::ostream& stream = std::cout) {
    detail::write_code(stream, RESTORE_CURSOR);
}

} // namespace cursor

} // namespace ign::terminal
