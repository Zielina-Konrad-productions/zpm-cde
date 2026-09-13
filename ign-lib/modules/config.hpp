#pragma once
#include "write.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <initializer_list>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace ign::config {

    using values = std::map<std::string, std::string>;

    struct options {
        char separator = '=';
        std::vector<std::string> comments = {"#", ";"};
        bool trim_keys = true;
        bool trim_values = true;
        bool inline_comments = false;
        bool allow_empty_keys = false;
        bool overwrite_existing = true;
    };

    namespace detail {

        inline bool is_space(char value) {
            return std::isspace(static_cast<unsigned char>(value)) != 0;
        }

        inline std::string trim(std::string value) {
            const auto first = std::find_if(
                value.begin(),
                value.end(),
                [](char current) { return !is_space(current); });

            if (first == value.end()) return {};

            const auto last = std::find_if(
                value.rbegin(),
                value.rend(),
                [](char current) { return !is_space(current); }).base();

            return std::string(first, last);
        }

        inline bool starts_with(
            const std::string& text,
            std::size_t position,
            const std::string& prefix) {
            return !prefix.empty() &&
                position + prefix.size() <= text.size() &&
                text.compare(position, prefix.size(), prefix) == 0;
        }

        inline bool is_comment_line(
            const std::string& line,
            const options& parse_options) {
            const std::string trimmed = trim(line);
            if (trimmed.empty()) return true;

            for (const std::string& comment : parse_options.comments) {
                if (starts_with(trimmed, 0, comment)) return true;
            }

            return false;
        }

        inline std::size_t find_inline_comment(
            const std::string& value,
            const options& parse_options) {
            if (!parse_options.inline_comments) return std::string::npos;

            for (std::size_t index = 0; index < value.size(); ++index) {
                if (index != 0 && !is_space(value[index - 1])) continue;

                for (const std::string& comment : parse_options.comments) {
                    if (starts_with(value, index, comment)) return index;
                }
            }

            return std::string::npos;
        }

        inline std::string clean_key(
            std::string key,
            const options& parse_options) {
            if (parse_options.trim_keys) key = trim(std::move(key));
            return key;
        }

        inline std::string clean_value(
            std::string value,
            const options& parse_options) {
            const std::size_t comment =
                find_inline_comment(value, parse_options);

            if (comment != std::string::npos) {
                value.erase(comment);
            }

            if (parse_options.trim_values) value = trim(std::move(value));
            return value;
        }

        inline void parse_line(
            const std::string& line,
            const options& parse_options,
            values& result) {
            if (parse_options.separator == '\0' ||
                is_comment_line(line, parse_options)) {
                return;
            }

            const std::size_t separator = line.find(parse_options.separator);
            if (separator == std::string::npos) return;

            std::string key =
                clean_key(line.substr(0, separator), parse_options);
            if (key.empty() && !parse_options.allow_empty_keys) return;

            std::string value =
                clean_value(line.substr(separator + 1), parse_options);

            if (parse_options.overwrite_existing ||
                result.find(key) == result.end()) {
                result[std::move(key)] = std::move(value);
            }
        }

        inline values parse(
            const std::string& content,
            const options& parse_options) {
            values result;
            std::istringstream stream(content);
            std::string line;

            while (std::getline(stream, line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                parse_line(line, parse_options, result);
            }

            return result;
        }

        inline values select_keys(
            const values& data,
            std::initializer_list<std::string> keys) {
            values result;

            for (const std::string& key : keys) {
                const auto found = data.find(key);
                if (found != data.end()) {
                    result.emplace(found->first, found->second);
                }
            }

            return result;
        }

        inline bool contains_all_keys(
            const values& data,
            std::initializer_list<std::string> keys) {
            for (const std::string& key : keys) {
                if (data.find(key) == data.end()) return false;
            }

            return true;
        }

        inline bool contains_any_key(
            const values& data,
            std::initializer_list<std::string> keys) {
            for (const std::string& key : keys) {
                if (data.find(key) != data.end()) return true;
            }

            return false;
        }

        inline ign::status read_file_values(
            const std::filesystem::path& path,
            const options& parse_options,
            values& result) {
            std::string content;
            const ign::status read_result =
                ign::file::detail::read_content(path, content);
            if (read_result != ign::status::ok) return read_result;

            result = parse(content, parse_options);
            return ign::status::ok;
        }

        inline bool matches_expected(
            const values& data,
            const std::string& expected,
            const options& parse_options) {
            if (expected.find(parse_options.separator) == std::string::npos) {
                if (expected.find_first_of("\r\n") != std::string::npos ||
                    is_comment_line(expected, parse_options)) {
                    return false;
                }

                const std::string key = clean_key(expected, parse_options);
                return data.find(key) != data.end();
            }

            const values expected_values = parse(expected, parse_options);
            if (expected_values.empty()) return false;

            for (const auto& item : expected_values) {
                const auto found = data.find(item.first);
                if (found == data.end() || found->second != item.second) {
                    return false;
                }
            }

            return true;
        }

        inline bool matches_all_expected(
            const values& data,
            std::initializer_list<std::string> expected,
            const options& parse_options) {
            if (expected.size() == 0) return false;

            for (const std::string& item : expected) {
                if (!matches_expected(data, item, parse_options)) {
                    return false;
                }
            }

            return true;
        }

        struct config_item {
            std::string key;
            std::string value;
            bool has_value = false;
            bool valid = false;
        };

        inline config_item parse_item(
            const std::string& text,
            const options& parse_options) {
            config_item item;

            if (parse_options.separator == '\0' ||
                text.find_first_of("\r\n") != std::string::npos ||
                is_comment_line(text, parse_options)) {
                return item;
            }

            const std::size_t separator = text.find(parse_options.separator);

            if (separator == std::string::npos) {
                item.key = clean_key(text, parse_options);
                item.valid =
                    !item.key.empty() || parse_options.allow_empty_keys;
                return item;
            }

            item.key = clean_key(text.substr(0, separator), parse_options);
            if (item.key.empty() && !parse_options.allow_empty_keys) {
                return item;
            }

            item.value =
                clean_value(text.substr(separator + 1), parse_options);
            item.has_value = true;
            item.valid = true;
            return item;
        }

        inline std::string format_item(
            const config_item& replacement,
            const std::string& current_value,
            const options& parse_options) {
            std::string line = replacement.key;
            line.push_back(' ');
            line.push_back(parse_options.separator);
            line.push_back(' ');
            line += replacement.has_value ? replacement.value : current_value;
            return line;
        }

        inline bool replace_line(
            const std::string& line,
            const std::string& old_item,
            const config_item& replacement,
            const options& parse_options,
            std::string& updated_line) {
            values line_values;
            parse_line(line, parse_options, line_values);

            if (line_values.empty() ||
                !matches_expected(line_values, old_item, parse_options)) {
                updated_line = line;
                return false;
            }

            updated_line = format_item(
                replacement,
                line_values.begin()->second,
                parse_options);
            return true;
        }

        inline std::string lower(std::string value) {
            std::transform(
                value.begin(),
                value.end(),
                value.begin(),
                [](char current) {
                    return static_cast<char>(
                        std::tolower(static_cast<unsigned char>(current)));
                });

            return value;
        }

        template<typename T>
        inline T parse_number(const std::string& value, T fallback) {
            std::istringstream stream(value);
            T parsed{};

            stream >> std::ws;
            stream >> parsed;
            if (stream.fail()) return fallback;

            if (!stream.eof()) stream >> std::ws;
            return stream.eof() ? parsed : fallback;
        }

    }

    // Reads a key/value config file. Invalid lines are skipped.
    inline values read(
        const std::filesystem::path& path,
        const options& parse_options = options{}) {
        values data;
        detail::read_file_values(path, parse_options, data);
        return data;
    }

    // Checks whether the config contains the expected key or key/value expression.
    inline ign::status read(
        const std::filesystem::path& path,
        const std::string& expected,
        const options& parse_options = options{}) {
        values data;
        const ign::status read_result =
            detail::read_file_values(path, parse_options, data);
        if (read_result != ign::status::ok) return read_result;

        return detail::matches_expected(data, expected, parse_options)
            ? ign::status::ok
            : ign::status::not_found;
    }

    // Checks multiple keys or key/value expressions. Every item must match.
    inline ign::status read(
        const std::filesystem::path& path,
        std::initializer_list<std::string> expected,
        const options& parse_options = options{}) {
        values data;
        const ign::status read_result =
            detail::read_file_values(path, parse_options, data);
        if (read_result != ign::status::ok) return read_result;

        return detail::matches_all_expected(data, expected, parse_options)
            ? ign::status::ok
            : ign::status::not_found;
    }

    template<typename... Expected>
    inline ign::status read(
        const std::filesystem::path& path,
        const std::string& first_expected,
        const std::string& second_expected,
        const Expected&... remaining_expected) {
        return read(
            path,
            {first_expected, second_expected,
             std::string(remaining_expected)...},
            options{});
    }

    template<typename... Expected>
    inline ign::status read(
        const std::filesystem::path& path,
        const options& parse_options,
        const std::string& first_expected,
        const Expected&... remaining_expected) {
        return read(
            path,
            {first_expected, std::string(remaining_expected)...},
            parse_options);
    }

    // Reads a single value. The fallback is returned when the key is missing.
    inline std::string value(
        const std::filesystem::path& path,
        const std::string& key,
        const std::string& fallback,
        const options& parse_options = options{}) {
        const values data = read(path, parse_options);
        const auto found = data.find(key);
        return found == data.end() ? fallback : found->second;
    }

    inline std::string value(
        const std::filesystem::path& path,
        const std::string& key,
        const options& parse_options) {
        return value(path, key, std::string(), parse_options);
    }

    inline std::string value(
        const std::filesystem::path& path,
        const std::string& key) {
        return value(path, key, std::string(), options{});
    }

    // Replaces every matching config item. If replacement is only a key, the
    // previous value is kept.
    inline ign::status replace(
        const std::filesystem::path& path,
        const std::string& old_item,
        const std::string& new_item,
        const options& parse_options = options{}) {
        const detail::config_item replacement =
            detail::parse_item(new_item, parse_options);
        if (!replacement.valid) return ign::status::error;

        std::string content;
        const ign::status read_result =
            ign::file::detail::read_content(path, content);
        if (read_result != ign::status::ok) return read_result;

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

            std::string line =
                content.substr(line_start, text_end - line_start);
            std::string updated_line;

            if (detail::replace_line(
                    line,
                    old_item,
                    replacement,
                    parse_options,
                    updated_line)) {
                replaced = true;
            }

            updated += updated_line;
            if (has_carriage_return) updated.push_back('\r');
            if (newline != std::string::npos) updated.push_back('\n');

            if (newline == std::string::npos) break;
            line_start = newline + 1;
        }

        if (!replaced) return ign::status::not_found;
        return ign::file::overwrite(path, updated);
    }

    inline ign::status replace(
        const std::filesystem::path& path,
        const options& parse_options,
        const std::string& old_item,
        const std::string& new_item) {
        return replace(path, old_item, new_item, parse_options);
    }

    // Reads only the requested keys. Missing keys are skipped.
    inline values read_keys(
        const std::filesystem::path& path,
        std::initializer_list<std::string> keys,
        const options& parse_options = options{}) {
        return detail::select_keys(read(path, parse_options), keys);
    }

    template<typename... Keys>
    inline values read_keys(
        const std::filesystem::path& path,
        const std::string& first_key,
        const Keys&... remaining_keys) {
        return read_keys(
            path,
            {first_key, std::string(remaining_keys)...},
            options{});
    }

    template<typename... Keys>
    inline values read_keys(
        const std::filesystem::path& path,
        const options& parse_options,
        const std::string& first_key,
        const Keys&... remaining_keys) {
        return read_keys(
            path,
            {first_key, std::string(remaining_keys)...},
            parse_options);
    }

    inline bool exists(
        const std::filesystem::path& path,
        const std::string& key,
        const options& parse_options = options{}) {
        const values data = read(path, parse_options);
        return data.find(key) != data.end();
    }

    inline bool exists_all(
        const std::filesystem::path& path,
        std::initializer_list<std::string> keys,
        const options& parse_options = options{}) {
        return detail::contains_all_keys(read(path, parse_options), keys);
    }

    template<typename... Keys>
    inline bool exists_all(
        const std::filesystem::path& path,
        const std::string& first_key,
        const Keys&... remaining_keys) {
        return exists_all(
            path,
            {first_key, std::string(remaining_keys)...},
            options{});
    }

    template<typename... Keys>
    inline bool exists_all(
        const std::filesystem::path& path,
        const options& parse_options,
        const std::string& first_key,
        const Keys&... remaining_keys) {
        return exists_all(
            path,
            {first_key, std::string(remaining_keys)...},
            parse_options);
    }

    inline bool exists_any(
        const std::filesystem::path& path,
        std::initializer_list<std::string> keys,
        const options& parse_options = options{}) {
        return detail::contains_any_key(read(path, parse_options), keys);
    }

    template<typename... Keys>
    inline bool exists_any(
        const std::filesystem::path& path,
        const std::string& first_key,
        const Keys&... remaining_keys) {
        return exists_any(
            path,
            {first_key, std::string(remaining_keys)...},
            options{});
    }

    template<typename... Keys>
    inline bool exists_any(
        const std::filesystem::path& path,
        const options& parse_options,
        const std::string& first_key,
        const Keys&... remaining_keys) {
        return exists_any(
            path,
            {first_key, std::string(remaining_keys)...},
            parse_options);
    }

    template<typename... Keys>
    inline bool exists(
        const std::filesystem::path& path,
        const std::string& first_key,
        const std::string& second_key,
        const Keys&... remaining_keys) {
        return exists_all(path, first_key, second_key, remaining_keys...);
    }

    inline int read_int(
        const std::filesystem::path& path,
        const std::string& key,
        int fallback = 0,
        const options& parse_options = options{}) {
        return detail::parse_number(value(path, key, parse_options), fallback);
    }

    inline double read_double(
        const std::filesystem::path& path,
        const std::string& key,
        double fallback = 0.0,
        const options& parse_options = options{}) {
        return detail::parse_number(value(path, key, parse_options), fallback);
    }

    inline bool read_bool(
        const std::filesystem::path& path,
        const std::string& key,
        bool fallback = false,
        const options& parse_options = options{}) {
        const std::string value =
            detail::lower(detail::trim(
                ign::config::value(path, key, parse_options)));

        if (value == "1" || value == "true" || value == "yes" ||
            value == "on" || value == "enabled") {
            return true;
        }

        if (value == "0" || value == "false" || value == "no" ||
            value == "off" || value == "disabled") {
            return false;
        }

        return fallback;
    }

}
