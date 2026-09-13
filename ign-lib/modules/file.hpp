#pragma once
#include "color.hpp"
#include "path.hpp"
#include "status.hpp"
#include "terminal.hpp"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace ign::file {

    struct text_size {
        std::size_t rows = 0;
        std::size_t columns = 0;
        bool available = false;
    };

    namespace detail {

        inline ign::status error_result(const std::error_code& ec) {
            if (ec == std::errc::permission_denied ||
                ec == std::errc::operation_not_permitted) {
                return ign::status::permission_denied;
            }

            if (ec == std::errc::no_such_file_or_directory) {
                return ign::status::not_found;
            }

            return ign::status::error;
        }

        inline ign::status stream_error_result() {
            return error_result(
                std::error_code(errno, std::generic_category()));
        }

        inline std::size_t one_based_column(std::size_t column) {
            return column == 0 ? 1 : column;
        }

        inline ign::status read_content(
            const std::filesystem::path& path,
            std::string& content) {
            const std::filesystem::path fullpath =
                ign::detail::expand_user_path(path);

            errno = 0;
            std::ifstream file(fullpath, std::ios::in | std::ios::binary);
            if (!file) return stream_error_result();

            content.assign(
                std::istreambuf_iterator<char>(file),
                std::istreambuf_iterator<char>());

            return file.good() || file.eof()
                ? ign::status::ok
                : stream_error_result();
        }

        inline bool is_utf8_continuation_byte(unsigned char value) {
            return (value & 0xC0) == 0x80;
        }

        // Truncates a single line to at most max_width visible columns,
        // never splitting a multi-byte UTF-8 character in half.
        inline std::string truncate_to_width(
            const std::string& line,
            std::size_t max_width) {
            if (max_width == 0) return std::string();

            std::string result;
            std::size_t visible_width = 0;

            for (char character : line) {
                const unsigned char current =
                    static_cast<unsigned char>(character);

                if (!is_utf8_continuation_byte(current)) {
                    if (visible_width == max_width) break;
                    ++visible_width;
                }

                result.push_back(character);
            }

            return result;
        }

        inline text_size measure_text_content(const std::string& content) {
            if (content.empty()) return {};

            text_size result;
            result.rows = 1;
            result.available = true;

            std::size_t current_width = 0;
            for (std::size_t i = 0; i < content.size(); ++i) {
                const char current = content[i];

                if (current == '\r') {
                    if (current_width > result.columns) {
                        result.columns = current_width;
                    }
                    current_width = 0;

                    if (i + 1 < content.size() && content[i + 1] == '\n') {
                        ++i;
                    }
                    if (i + 1 < content.size()) {
                        ++result.rows;
                    }
                    continue;
                }

                if (current == '\n') {
                    if (current_width > result.columns) {
                        result.columns = current_width;
                    }
                    current_width = 0;

                    if (i + 1 < content.size()) {
                        ++result.rows;
                    }
                    continue;
                }

                if (!is_utf8_continuation_byte(
                        static_cast<unsigned char>(current))) {
                    ++current_width;
                }
            }

            if (current_width > result.columns) {
                result.columns = current_width;
            }

            return result;
        }

        inline std::string collapse_carriage_return(const std::string& line) {
            const std::size_t last_cr = line.find_last_of('\r');
            return last_cr == std::string::npos ? line : line.substr(last_cr + 1);
        }
        
        inline std::vector<std::string> last_lines(
                    const std::string& content,
                    std::size_t line_count) {
                    std::vector<std::string> lines;
                    std::size_t line_start = 0;
        
        while (line_start < content.size()) {
        const std::size_t newline = content.find('\n', line_start);
        const std::size_t line_end =
                            newline == std::string::npos ? content.size() : newline;
                        std::size_t text_end = line_end;
        
        if (text_end > line_start && content[text_end - 1] == '\r') {
        --text_end;
        }
        
        lines.emplace_back(collapse_carriage_return(
        content.substr(line_start, text_end - line_start)));
        
        if (newline == std::string::npos) break;
                        line_start = newline + 1;
        }
        
        if (lines.size() > line_count) {
        lines.erase(lines.begin(), lines.end() - line_count);
        }
        
        if (lines.size() < line_count) {
        lines.insert(
        lines.begin(), line_count - lines.size(), std::string());
        }
        
        return lines;
                }

        inline bool render_file_lines(
            const std::string& content,
            std::size_t line_count,
            bool first_render) {
            if (!first_render) {
                std::cout << "\033[" << line_count << 'A';
            }

            const std::size_t terminal_width = ign::terminal::columns();

            for (const std::string& line : last_lines(content, line_count)) {
                std::cout << "\r\033[2K"
                           << truncate_to_width(line, terminal_width) << '\n';
            }

            std::cout << std::flush;
            return static_cast<bool>(std::cout);
        }

        class file_print_loop {
            public:
                file_print_loop() = default;
                file_print_loop(const file_print_loop&) = delete;
                file_print_loop& operator=(const file_print_loop&) = delete;
            
                ~file_print_loop() {
                    stop();
                }
            
                ign::status start(
                    const std::filesystem::path& path,
                    std::size_t line_count,
                    std::string initial_content) {
                    if (line_count == 0 || worker_.joinable()) {
                        return ign::status::error;
                    }
            
                    has_rendered_ = false;

                    // Reserve exactly line_count blank rows up front. If the
                    // terminal needs to scroll to make room, it happens now -
                    // before any relative cursor-up math starts - so later
                    // redraws always land on an already-stable block instead
                    // of drifting by a row when a scroll happens mid-render.
                    for (std::size_t i = 0; i < line_count; ++i) {
                        std::cout << '\n';
                    }
                    std::cout << std::flush;
                    has_rendered_ = true;

                    if (!initial_content.empty()) {
                        if (!render_file_lines(initial_content, line_count, false)) {
                            return ign::status::error;
                        }
                    }
            
                    {
                        std::lock_guard<std::mutex> lock(control_mutex_);
                        stop_requested_ = false;
                    }
                    last_result_.store(ign::status::ok);
            
                    try {
                        worker_ = std::thread(
                            [this, path, line_count,
                             content = std::move(initial_content)]() mutable {
                                run(path, line_count, std::move(content));
                            });
                    } catch (...) {
                        return ign::status::error;
                    }
            
                    return ign::status::ok;
                }
            
                ign::status stop() {
                    if (!worker_.joinable()) return ign::status::ok;
            
                    {
                        std::lock_guard<std::mutex> lock(control_mutex_);
                        stop_requested_ = true;
                    }
                    control_changed_.notify_all();
                    worker_.join();
                    return last_result_.load();
                }
            
            private:
                void run(
                    const std::filesystem::path& path,
                    std::size_t line_count,
                    std::string previous_content) {
                    while (true) {
                        {
                            std::unique_lock<std::mutex> lock(control_mutex_);
                            if (control_changed_.wait_for(
                                    lock,
                                    std::chrono::milliseconds(250),
                                    [this] { return stop_requested_; })) {
                                break;
                            }
                        }
            
                        std::string content;
                        const ign::status result = read_content(path, content);
                        last_result_.store(result);
                        if (result != ign::status::ok ||
                            content == previous_content) {
                            continue;
                        }
            
                        if (!render_file_lines(content, line_count, !has_rendered_)) {
                            last_result_.store(ign::status::error);
                            continue;
                        }
                        has_rendered_ = true;
            
                        previous_content = std::move(content);
                    }
                }
            
                std::thread worker_;
                std::mutex control_mutex_;
                std::condition_variable control_changed_;
                bool stop_requested_ = false;
                bool has_rendered_ = false;
                std::atomic<ign::status> last_result_{ign::status::ok};
            };

        inline file_print_loop& active_file_print_loop() {
            static file_print_loop loop;
            return loop;
        }

        inline void print_aligned_content(
            const std::string& content,
            std::size_t column,
            bool trailing_newline) {
            for (std::size_t i = 0; i < content.size(); ++i) {
                const char current = content[i];

                if (current == '\r') {
                    if (i + 1 < content.size() && content[i + 1] == '\n') {
                        ++i;
                    }

                    std::cout.put('\n');
                    ign::terminal::cursor::column(column);
                    continue;
                }

                if (current == '\n') {
                    std::cout.put('\n');
                    ign::terminal::cursor::column(column);
                    continue;
                }

                std::cout.put(current);
            }

            if (trailing_newline) {
                std::cout.put('\n');
                ign::terminal::cursor::column(column);
            }
        }

        inline bool has_newline(const std::string& content) {
            return content.find('\n') != std::string::npos ||
                content.find('\r') != std::string::npos;
        }

        inline void cursor_next_line_same_column() {
            std::cout << "\033D" << std::flush;
        }

        inline void move_from_saved_start_to_next_line() {
            ign::terminal::cursor::restore();
            cursor_next_line_same_column();
            ign::terminal::cursor::save();
        }

        inline void print_cursor_aligned_content(
            const std::string& content,
            bool trailing_newline) {
            if (!trailing_newline && !has_newline(content)) {
                std::cout << content;
                return;
            }

            ign::terminal::cursor::save();

            for (std::size_t i = 0; i < content.size(); ++i) {
                const char current = content[i];

                if (current == '\r') {
                    if (i + 1 < content.size() && content[i + 1] == '\n') {
                        ++i;
                    }

                    move_from_saved_start_to_next_line();
                    continue;
                }

                if (current == '\n') {
                    move_from_saved_start_to_next_line();
                    continue;
                }

                std::cout.put(current);
            }

            if (trailing_newline) {
                move_from_saved_start_to_next_line();
            }
        }

        inline ign::status print_content(
            const std::filesystem::path& path,
            const char* style,
            bool newline,
            std::size_t column = 0,
            bool align_to_current_cursor = false) {
            std::string content;
            const ign::status read_result = read_content(path, content);
            if (read_result != ign::status::ok) return read_result;

            if (style != nullptr) {
                std::cout << style;
            }

            bool newline_handled = false;
            if (column != 0) {
                print_aligned_content(content, column, newline);
                newline_handled = true;
            } else if (align_to_current_cursor) {
                print_cursor_aligned_content(content, newline);
                newline_handled = true;
            } else {
                std::cout << content;
            }

            if (style != nullptr) {
                std::cout << ign::color::RESET;
            }

            if (newline && !newline_handled) {
                std::cout.put('\n');
            }

            std::cout.flush();
            return std::cout ? ign::status::ok : ign::status::error;
        }

    }

    // Creates a file without overwriting its existing contents.
    // Usage example: ign::file::create("/tmp/file.txt");
    inline ign::status create(const std::filesystem::path& path) {
        const std::filesystem::path fullpath =
            ign::detail::expand_user_path(path);
        std::error_code ec;

        if (fullpath.has_parent_path()) {
            std::filesystem::create_directories(fullpath.parent_path(), ec);
            if (ec) return detail::error_result(ec);
        }

        errno = 0;
        std::ofstream file(fullpath, std::ios::out | std::ios::app);
        if (!file) return detail::stream_error_result();

        file.close();
        return file ? ign::status::ok : detail::stream_error_result();
    }

    // Creates multiple files. All paths are processed and the most severe
    // status is returned.
    template<typename... Paths>
    inline ign::status create(
        const std::filesystem::path& first,
        const std::filesystem::path& second,
        const Paths&... remaining) {
        ign::status result = ign::file::create(first);
        const auto create_one = [&result](const auto& path) {
            const ign::status current =
                ign::file::create(std::filesystem::path(path));
            result = ign::detail::merge_status(result, current);
        };

        create_one(second);
        (create_one(remaining), ...);
        return result;
    }

    // Removes a file. A missing file is treated as success.
    // Usage example: ign::file::remove("/tmp/file.txt");
    inline ign::status remove(const std::filesystem::path& path) {
        const std::filesystem::path fullpath =
            ign::detail::expand_user_path(path);
        std::error_code ec;

        if (!std::filesystem::exists(fullpath, ec)) {
            return ec ? detail::error_result(ec) : ign::status::ok;
        }

        const bool is_file = std::filesystem::is_regular_file(fullpath, ec);
        if (ec) return detail::error_result(ec);
        if (!is_file) return ign::status::error;

        std::filesystem::remove(fullpath, ec);

        return ec ? detail::error_result(ec) : ign::status::ok;
    }

    // Removes multiple files. All paths are processed and the most severe
    // result is returned.
    template<typename... Paths>
    inline ign::status remove(
        const std::filesystem::path& first,
        const std::filesystem::path& second,
        const Paths&... remaining) {
        ign::status result = ign::file::remove(first);
        const auto remove_one = [&result](const auto& path) {
            const ign::status current =
                ign::file::remove(std::filesystem::path(path));
            result = ign::detail::merge_status(result, current);
        };

        remove_one(second);
        (remove_one(remaining), ...);
        return result;
    }

    // Copies a regular file. Missing destination parent directories are
    // created, and an existing destination file is overwritten.
    // Usage example: ign::file::copy("/tmp/file.txt", "/tmp/backup.txt");
    inline ign::status copy(
        const std::filesystem::path& source,
        const std::filesystem::path& destination) {
        const std::filesystem::path source_path =
            ign::detail::expand_user_path(source);
        const std::filesystem::path destination_path =
            ign::detail::expand_user_path(destination);
        std::error_code ec;
        const bool source_is_file =
            std::filesystem::is_regular_file(source_path, ec);

        if (ec) return detail::error_result(ec);
        if (!source_is_file) return ign::status::not_found;

        if (destination_path.has_parent_path()) {
            std::filesystem::create_directories(
                destination_path.parent_path(), ec);
            if (ec) return detail::error_result(ec);
        }

        std::filesystem::copy_file(
            source_path,
            destination_path,
            std::filesystem::copy_options::overwrite_existing,
            ec);

        return ec ? detail::error_result(ec) : ign::status::ok;
    }

    // Moves or renames a regular file. Missing destination parent directories
    // are created. Moving between filesystems uses copy() and remove().
    // Usage example: ign::file::move("/tmp/file.txt", "/tmp/archive.txt");
    inline ign::status move(
        const std::filesystem::path& source,
        const std::filesystem::path& destination) {
        const std::filesystem::path source_path =
            ign::detail::expand_user_path(source);
        const std::filesystem::path destination_path =
            ign::detail::expand_user_path(destination);
        std::error_code ec;
        const bool source_is_file =
            std::filesystem::is_regular_file(source_path, ec);

        if (ec) return detail::error_result(ec);
        if (!source_is_file) return ign::status::not_found;

        if (destination_path.has_parent_path()) {
            std::filesystem::create_directories(
                destination_path.parent_path(), ec);
            if (ec) return detail::error_result(ec);
        }

        std::filesystem::rename(source_path, destination_path, ec);
        if (!ec) return ign::status::ok;
        if (ec != std::errc::cross_device_link) {
            return detail::error_result(ec);
        }

        const ign::status copy_result =
            ign::file::copy(source_path, destination_path);
        if (copy_result != ign::status::ok) return copy_result;

        return ign::file::remove(source_path);
    }

    // Checks whether a regular file exists.
    // Usage example: ign::file::exists("/tmp/file.txt");
    inline ign::status exists(const std::filesystem::path& path) {
        const std::filesystem::path fullpath =
            ign::detail::expand_user_path(path);
        std::error_code ec;
        const bool result = std::filesystem::is_regular_file(fullpath, ec);

        if (ec) return detail::error_result(ec);
        return result ? ign::status::ok : ign::status::not_found;
    }

    // Checks multiple files. Returns ok only when every file exists.
    template<typename... Paths>
    inline ign::status exists(
        const std::filesystem::path& first,
        const std::filesystem::path& second,
        const Paths&... remaining) {
        ign::status result = ign::file::exists(first);
        const auto check_one = [&result](const auto& path) {
            const ign::status current =
                ign::file::exists(std::filesystem::path(path));
            result = ign::detail::merge_status(result, current);
        };

        check_one(second);
        (check_one(remaining), ...);
        return result;
    }

    // Reads and returns the complete file contents.
    // An empty string is returned when the file cannot be read.
    // Usage example: std::string text = ign::file::read("config.txt");
    inline std::string read(const std::filesystem::path& path) {
        std::string content;
        detail::read_content(path, content);
        return content;
    }

    // Reads the complete file contents into output and returns the read status.
    // output is cleared before reading.
    // Usage example:
    // std::string text;
    // ign::status result = ign::file::read_to("config.txt", text);
    inline ign::status read_to(
        const std::filesystem::path& path,
        std::string& output) {
        output.clear();
        return detail::read_content(path, output);
    }

    // Measures text dimensions after reading the complete file.
    // rows and columns are zero when the file cannot be read.
    // Usage example: auto size = ign::file::dimensions("ascii_art/title.txt");
    inline text_size dimensions(const std::filesystem::path& path) {
        std::string content;
        if (detail::read_content(path, content) != ign::status::ok) {
            return {};
        }

        return detail::measure_text_content(content);
    }

    // Returns the maximum visible line width of a text file.
    // Usage example: auto width = ign::file::width("ascii_art/title.txt");
    inline std::size_t width(const std::filesystem::path& path) {
        return dimensions(path).columns;
    }

    // Returns the visible line count of a text file.
    // Usage example: auto height = ign::file::height("ascii_art/title.txt");
    inline std::size_t height(const std::filesystem::path& path) {
        return dimensions(path).rows;
    }

    // Checks whether a file contains the specified text.
    // Usage example: ign::file::contains("config.txt", "enabled = true");
    inline ign::status contains(
        const std::filesystem::path& path,
        const std::string& searched_text) {
        std::string content;
        const ign::status read_result = detail::read_content(path, content);
        if (read_result != ign::status::ok) return read_result;

        return content.find(searched_text) != std::string::npos
            ? ign::status::ok
            : ign::status::not_found;
    }

    // Compatibility alias. Prefer contains() for text checks so read() keeps
    // the single meaning of returning the complete file contents.
    [[deprecated("use ign::file::contains(path, text)")]]
    inline ign::status read(
        const std::filesystem::path& path,
        const std::string& searched_text) {
        return contains(path, searched_text);
    }

    // Prints the complete file contents to standard output.
    // No extra newline is added.
    // Usage example: ign::file::print("message.txt");
    inline ign::status print(const std::filesystem::path& path) {
        return detail::print_content(path, nullptr, false, 0, true);
    }

    // Prints the complete file contents and adds a newline.
    // Usage example: ign::file::println("message.txt");
    inline ign::status println(const std::filesystem::path& path) {
        return detail::print_content(path, nullptr, true, 0, true);
    }

    // Prints a multiline file so every line starts at the given 1-based
    // column. No extra newline is added.
    // Usage example: ign::file::print_column(6, "ascii_art/title.txt");
    inline ign::status print_column(
        std::size_t column,
        const std::filesystem::path& path) {
        const std::size_t aligned_column = detail::one_based_column(column);
        ign::terminal::cursor::column(aligned_column);
        return detail::print_content(
            path, nullptr, false, aligned_column);
    }

    // Prints a multiline file aligned to a column and adds a newline.
    // Usage example: ign::file::println_column(6, "ascii_art/title.txt");
    inline ign::status println_column(
        std::size_t column,
        const std::filesystem::path& path) {
        const std::size_t aligned_column = detail::one_based_column(column);
        ign::terminal::cursor::column(aligned_column);
        return detail::print_content(
            path, nullptr, true, aligned_column);
    }

    // Moves the cursor and prints a multiline file from that row and column.
    // Usage example: ign::file::print_at(3, 6, "ascii_art/title.txt");
    inline ign::status print_at(
        std::size_t row,
        std::size_t column,
        const std::filesystem::path& path) {
        const std::size_t aligned_column = detail::one_based_column(column);
        ign::terminal::cursor::to(row, aligned_column);
        return detail::print_content(path, nullptr, false, aligned_column);
    }

    // Moves the cursor, prints a multiline file, and adds a newline.
    // Usage example: ign::file::println_at(3, 6, "ascii_art/title.txt");
    inline ign::status println_at(
        std::size_t row,
        std::size_t column,
        const std::filesystem::path& path) {
        const std::size_t aligned_column = detail::one_based_column(column);
        ign::terminal::cursor::to(row, aligned_column);
        return detail::print_content(path, nullptr, true, aligned_column);
    }

    // Styled file print helpers mirror ign::print_NAME() and
    // ign::println_NAME(), but their argument is a file path.
#define IGN_CREATE_FILE_PRINT_STYLE(NAME, CODE)                         \
    inline ign::status print_##NAME(const std::filesystem::path& path) { \
        return detail::print_content(                                   \
            path, ign::color::CODE, false, 0, true);                    \
    }                                                                   \
                                                                        \
    inline ign::status println_##NAME(                                  \
        const std::filesystem::path& path) {                             \
        return detail::print_content(                                   \
            path, ign::color::CODE, true, 0, true);                     \
    }                                                                   \
                                                                        \
    inline ign::status print_column_##NAME(                             \
        std::size_t column,                                             \
        const std::filesystem::path& path) {                             \
        const std::size_t aligned_column =                              \
            detail::one_based_column(column);                           \
        ign::terminal::cursor::column(aligned_column);                  \
        return detail::print_content(                                   \
            path, ign::color::CODE, false, aligned_column);             \
    }                                                                   \
                                                                        \
    inline ign::status println_column_##NAME(                           \
        std::size_t column,                                             \
        const std::filesystem::path& path) {                             \
        const std::size_t aligned_column =                              \
            detail::one_based_column(column);                           \
        ign::terminal::cursor::column(aligned_column);                  \
        return detail::print_content(                                   \
            path, ign::color::CODE, true, aligned_column);              \
    }                                                                   \
                                                                        \
    inline ign::status print_at_##NAME(                                 \
        std::size_t row,                                                \
        std::size_t column,                                             \
        const std::filesystem::path& path) {                             \
        const std::size_t aligned_column =                              \
            detail::one_based_column(column);                           \
        ign::terminal::cursor::to(row, aligned_column);                 \
        return detail::print_content(                                   \
            path, ign::color::CODE, false, aligned_column);             \
    }                                                                   \
                                                                        \
    inline ign::status println_at_##NAME(                               \
        std::size_t row,                                                \
        std::size_t column,                                             \
        const std::filesystem::path& path) {                             \
        const std::size_t aligned_column =                              \
            detail::one_based_column(column);                           \
        ign::terminal::cursor::to(row, aligned_column);                 \
        return detail::print_content(                                   \
            path, ign::color::CODE, true, aligned_column);              \
    }

#define IGN_DETAIL_CREATE_FILE_PRINT_STYLE(NAME, CONSTANT, ANSI_CODE) \
    IGN_CREATE_FILE_PRINT_STYLE(NAME, CONSTANT)

IGN_DETAIL_FOR_EACH_COLOR_STYLE(IGN_DETAIL_CREATE_FILE_PRINT_STYLE)

#undef IGN_DETAIL_CREATE_FILE_PRINT_STYLE
#undef IGN_CREATE_FILE_PRINT_STYLE

    // Starts refreshing the last line_count lines of a file in place.
    // Only one print loop can be active at a time.
    inline ign::status print_loopstart(
        const std::filesystem::path& path,
        std::size_t line_count) {
        std::string content;
        const ign::status read_result = detail::read_content(path, content);
        if (read_result != ign::status::ok) return read_result;

        return detail::active_file_print_loop().start(
            path, line_count, std::move(content));
    }

    // Stops the active file print loop and waits for its worker to finish.
    inline ign::status print_loopend() {
        return detail::active_file_print_loop().stop();
    }

}