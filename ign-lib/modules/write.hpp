#pragma once
#include "file.hpp"

#include <cerrno>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <system_error>

namespace ign::file {

    // Opens a file and overwrites its contents.
    // Usage example: ign::file::overwrite("/tmp/myfile.txt", "I like my life");
    inline ign::status overwrite(
        const std::filesystem::path& path, const std::string& content) {
        const std::filesystem::path fullpath =
            ign::detail::expand_user_path(path);

        if (fullpath.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(fullpath.parent_path(), ec);
            if (ec) return detail::error_result(ec);
        }

        errno = 0;
        std::ofstream file(fullpath, std::ios::out | std::ios::trunc);
        if (!file) return detail::stream_error_result();

        file << content;
        file.close();
        return file ? ign::status::ok : detail::stream_error_result();
    }

    // Overwrites a file with multiple lines. No newline is added after the
    // final line.
    // Usage example: ign::file::overwrite("file.txt", {"first", "second"});
    inline ign::status overwrite(
        const std::filesystem::path& path,
        std::initializer_list<std::string> lines) {
        std::string content;
        bool first = true;

        for (const std::string& line : lines) {
            if (line.find_first_of("\r\n") != std::string::npos) {
                return ign::status::error;
            }
            if (!first) content.push_back('\n');
            content += line;
            first = false;
        }

        return overwrite(path, content);
    }

    // Writes text at the end of a file. No newline is added.
    // Usage example: ign::file::write("file.txt", "raw text");
    inline ign::status write(
        const std::filesystem::path& path, const std::string& content) {
        const std::filesystem::path fullpath =
            ign::detail::expand_user_path(path);

        if (fullpath.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(fullpath.parent_path(), ec);
            if (ec) return detail::error_result(ec);
        }

        errno = 0;
        std::ofstream file(fullpath, std::ios::out | std::ios::app);
        if (!file) return detail::stream_error_result();

        file << content;
        file.close();
        return file ? ign::status::ok : detail::stream_error_result();
    }

    // Writes text at the end of a file, followed by a newline.
    // Usage example: ign::file::write_line("/tmp/myfile.txt", "I like my life");
    inline ign::status write_line(
        const std::filesystem::path& path, const std::string& content) {
        return write(path, content + '\n');
    }

    // Writes multiple lines at the end of a file, each followed by a newline.
    // Usage example: ign::file::write_lines("file.txt", {"first", "second"});
    inline ign::status write_lines(
        const std::filesystem::path& path,
        std::initializer_list<std::string> lines) {
        for (const std::string& line : lines) {
            if (line.find_first_of("\r\n") != std::string::npos) {
                return ign::status::error;
            }
        }

        const std::filesystem::path fullpath =
            ign::detail::expand_user_path(path);

        if (fullpath.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(fullpath.parent_path(), ec);
            if (ec) return detail::error_result(ec);
        }

        errno = 0;
        std::ofstream file(fullpath, std::ios::out | std::ios::app);
        if (!file) return detail::stream_error_result();

        for (const std::string& line : lines) {
            file << line << '\n';
        }

        file.close();
        return file ? ign::status::ok : detail::stream_error_result();
    }

    inline ign::status write(
        const std::filesystem::path& path,
        std::initializer_list<std::string> lines) {
        return write_lines(path, lines);
    }

    namespace detail {

        inline ign::status replace_lines(
            const std::filesystem::path& path,
            const std::string& old_line,
            std::initializer_list<std::string> new_lines,
            bool first_only) {
            if (old_line.find_first_of("\r\n") != std::string::npos ||
                new_lines.size() == 0) {
                return ign::status::error;
            }

            for (const std::string& line : new_lines) {
                if (line.find_first_of("\r\n") != std::string::npos) {
                    return ign::status::error;
                }
            }

            std::string content;
            const ign::status read_result = read_content(path, content);
            if (read_result != ign::status::ok) return read_result;

            const bool default_carriage_return =
                content.find("\r\n") != std::string::npos;

            std::string updated;
            updated.reserve(content.size());
            bool replaced = false;
            std::size_t line_start = 0;

            while (line_start < content.size()) {
                const std::size_t newline = content.find('\n', line_start);
                const std::size_t line_end =
                    newline == std::string::npos ? content.size() : newline;
                const bool has_carriage_return =
                    line_end > line_start && content[line_end - 1] == '\r';
                const std::size_t text_end =
                    line_end - (has_carriage_return ? 1 : 0);
                const std::size_t line_length = text_end - line_start;
                const bool matches =
                    line_length == old_line.size() &&
                    content.compare(line_start, line_length, old_line) == 0;

                if (matches && (!first_only || !replaced)) {
                    bool first_new_line = true;
                    const bool use_carriage_return =
                        newline == std::string::npos
                            ? default_carriage_return
                            : has_carriage_return;

                    for (const std::string& new_line : new_lines) {
                        if (!first_new_line) {
                            if (use_carriage_return) updated.push_back('\r');
                            updated.push_back('\n');
                        }
                        updated += new_line;
                        first_new_line = false;
                    }
                    replaced = true;
                } else {
                    updated.append(content, line_start, line_length);
                }

                if (has_carriage_return) updated.push_back('\r');
                if (newline != std::string::npos) updated.push_back('\n');

                if (newline == std::string::npos) break;
                line_start = newline + 1;
            }

            if (!replaced) return ign::status::not_found;
            return overwrite(path, updated);
        }

    }

    // Replaces every line exactly matching old_line with new_line.
    // Other lines and the file's line endings are preserved.
    // Usage example:
    // ign::file::replace("config.txt", "enabled = true", "enabled = false");
    inline ign::status replace(
        const std::filesystem::path& path,
        const std::string& old_line,
        const std::string& new_line) {
        return detail::replace_lines(path, old_line, {new_line}, false);
    }

    // Replaces every matching line with multiple new lines.
    inline ign::status replace(
        const std::filesystem::path& path,
        const std::string& old_line,
        std::initializer_list<std::string> new_lines) {
        return detail::replace_lines(path, old_line, new_lines, false);
    }

    // Replaces only the first line exactly matching old_line.
    // Usage example:
    // ign::file::replace_first("config.txt", "mode = old", "mode = new");
    inline ign::status replace_first(
        const std::filesystem::path& path,
        const std::string& old_line,
        const std::string& new_line) {
        return detail::replace_lines(path, old_line, {new_line}, true);
    }

    // Replaces the first matching line with multiple new lines.
    inline ign::status replace_first(
        const std::filesystem::path& path,
        const std::string& old_line,
        std::initializer_list<std::string> new_lines) {
        return detail::replace_lines(path, old_line, new_lines, true);
    }

}
