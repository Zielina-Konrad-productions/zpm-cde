#pragma once
#include "path.hpp"
#include "status.hpp"

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <vector>

namespace ign::catalog {

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

    }

    // Creates a directory and any missing parent directories.
    // An existing directory is treated as success.
    // Usage example: ign::catalog::create("/tmp/my/catalog");
    inline ign::status create(const std::filesystem::path& path) {
        const std::filesystem::path fullpath =
            ign::detail::expand_user_path(path);
        std::error_code ec;
        std::filesystem::create_directories(fullpath, ec);

        return ec ? detail::error_result(ec) : ign::status::ok;
    }

    // Creates multiple directories. All paths are processed. The return value
    // is the most severe status.
    template<typename... Paths>
    inline ign::status create(
        const std::filesystem::path& first,
        const std::filesystem::path& second,
        const Paths&... remaining) {
        ign::status result = ign::catalog::create(first);
        const auto create_one = [&result](const auto& path) {
            const ign::status current =
                ign::catalog::create(std::filesystem::path(path));
            result = ign::detail::merge_status(result, current);
        };

        create_one(second);
        (create_one(remaining), ...);
        return result;
    }

    // Recursively removes a directory and all of its contents.
    // A missing directory is treated as success.
    // Usage example: ign::catalog::remove("/tmp/my/catalog");
    inline ign::status remove(const std::filesystem::path& path) {
        const std::filesystem::path fullpath =
            ign::detail::expand_user_path(path);
        std::error_code ec;

        if (!std::filesystem::exists(fullpath, ec)) {
            return ec ? detail::error_result(ec) : ign::status::ok;
        }

        const bool is_directory = std::filesystem::is_directory(fullpath, ec);
        if (ec) return detail::error_result(ec);
        if (!is_directory) return ign::status::error;

        std::filesystem::remove_all(fullpath, ec);
        return ec ? detail::error_result(ec) : ign::status::ok;
    }

    // Removes multiple directories recursively. All paths are processed and
    // the most severe result is returned.
    template<typename... Paths>
    inline ign::status remove(
        const std::filesystem::path& first,
        const std::filesystem::path& second,
        const Paths&... remaining) {
        ign::status result = ign::catalog::remove(first);
        const auto remove_one = [&result](const auto& path) {
            const ign::status current =
                ign::catalog::remove(std::filesystem::path(path));
            result = ign::detail::merge_status(result, current);
        };

        remove_one(second);
        (remove_one(remaining), ...);
        return result;
    }

    // Removes everything inside a directory, but keeps the directory itself.
    // Usage example: ign::catalog::clear("/tmp/my/catalog");
    inline ign::status clear(const std::filesystem::path& path) {
        const std::filesystem::path fullpath =
            ign::detail::expand_user_path(path);
        std::error_code ec;

        if (!std::filesystem::exists(fullpath, ec)) {
            return ec ? detail::error_result(ec) : ign::status::not_found;
        }

        const bool is_directory = std::filesystem::is_directory(fullpath, ec);
        if (ec) return detail::error_result(ec);
        if (!is_directory) return ign::status::not_found;

        std::vector<std::filesystem::path> entries;
        std::filesystem::directory_iterator end;
        std::filesystem::directory_iterator iterator(fullpath, ec);
        if (ec) return detail::error_result(ec);

        while (iterator != end) {
            entries.push_back(iterator->path());
            iterator.increment(ec);
            if (ec) return detail::error_result(ec);
        }

        ign::status result = ign::status::ok;
        for (const std::filesystem::path& entry : entries) {
            std::error_code remove_ec;
            std::filesystem::remove_all(entry, remove_ec);

            if (remove_ec) {
                const ign::status current = detail::error_result(remove_ec);
                result = ign::detail::merge_status(result, current);
            }
        }

        return result;
    }

    // Clears multiple directories. All paths are processed and the most
    // severe result is returned.
    template<typename... Paths>
    inline ign::status clear(
        const std::filesystem::path& first,
        const std::filesystem::path& second,
        const Paths&... remaining) {
        ign::status result = ign::catalog::clear(first);
        const auto clear_one = [&result](const auto& path) {
            const ign::status current =
                ign::catalog::clear(std::filesystem::path(path));
            result = ign::detail::merge_status(result, current);
        };

        clear_one(second);
        (clear_one(remaining), ...);
        return result;
    }

    // Checks whether a directory exists.
    // Usage example: ign::catalog::exists("/tmp/my/catalog");
    inline ign::status exists(const std::filesystem::path& path) {
        const std::filesystem::path fullpath =
            ign::detail::expand_user_path(path);
        std::error_code ec;
        const bool result = std::filesystem::is_directory(fullpath, ec);

        if (ec) return detail::error_result(ec);
        return result ? ign::status::ok : ign::status::not_found;
    }

    // Checks multiple directories. Returns ok only when every directory exists.
    template<typename... Paths>
    inline ign::status exists(
        const std::filesystem::path& first,
        const std::filesystem::path& second,
        const Paths&... remaining) {
        ign::status result = ign::catalog::exists(first);
        const auto check_one = [&result](const auto& path) {
            const ign::status current =
                ign::catalog::exists(std::filesystem::path(path));
            result = ign::detail::merge_status(result, current);
        };

        check_one(second);
        (check_one(remaining), ...);
        return result;
    }

    // Checks whether a directory exists and is empty.
    // Usage example: ign::catalog::empty("/tmp/my/catalog");
    inline ign::status empty(const std::filesystem::path& path) {
        const std::filesystem::path fullpath =
            ign::detail::expand_user_path(path);
        std::error_code ec;
        const bool is_directory = std::filesystem::is_directory(fullpath, ec);

        if (ec) return detail::error_result(ec);
        if (!is_directory) return ign::status::not_found;

        const bool result = std::filesystem::is_empty(fullpath, ec);
        if (ec) return detail::error_result(ec);
        return result ? ign::status::ok : ign::status::not_found;
    }

    // Checks multiple directories. Returns ok only when every directory exists
    // and is empty.
    template<typename... Paths>
    inline ign::status empty(
        const std::filesystem::path& first,
        const std::filesystem::path& second,
        const Paths&... remaining) {
        ign::status result = ign::catalog::empty(first);
        const auto check_one = [&result](const auto& path) {
            const ign::status current =
                ign::catalog::empty(std::filesystem::path(path));
            result = ign::detail::merge_status(result, current);
        };

        check_one(second);
        (check_one(remaining), ...);
        return result;
    }

    // Lists the direct contents of a directory. Returns full entry paths.
    // An empty vector is returned when the directory cannot be read.
    // Usage example: auto entries = ign::catalog::list("/tmp/my/catalog");
    inline std::vector<std::filesystem::path> list(
        const std::filesystem::path& path) {
        const std::filesystem::path fullpath =
            ign::detail::expand_user_path(path);
        std::vector<std::filesystem::path> entries;
        std::error_code ec;

        const bool is_directory = std::filesystem::is_directory(fullpath, ec);
        if (ec || !is_directory) return entries;

        std::filesystem::directory_iterator end;
        std::filesystem::directory_iterator iterator(fullpath, ec);
        if (ec) return entries;

        while (iterator != end) {
            entries.push_back(iterator->path());
            iterator.increment(ec);
            if (ec) break;
        }

        std::sort(entries.begin(), entries.end());
        return entries;
    }

    // Recursively copies a directory and its contents.
    // Usage example: ign::catalog::copy("/tmp/source", "/tmp/backup");
    inline ign::status copy(
        const std::filesystem::path& source,
        const std::filesystem::path& destination) {
        const std::filesystem::path source_path =
            ign::detail::expand_user_path(source);
        const std::filesystem::path destination_path =
            ign::detail::expand_user_path(destination);
        std::error_code ec;
        const bool source_is_directory =
            std::filesystem::is_directory(source_path, ec);

        if (ec) return detail::error_result(ec);
        if (!source_is_directory) return ign::status::not_found;

        if (destination_path.has_parent_path()) {
            std::filesystem::create_directories(
                destination_path.parent_path(), ec);
            if (ec) return detail::error_result(ec);
        }

        std::filesystem::copy(
            source_path,
            destination_path,
            std::filesystem::copy_options::recursive |
                std::filesystem::copy_options::overwrite_existing,
            ec);

        return ec ? detail::error_result(ec) : ign::status::ok;
    }

    // Moves or renames a directory. Missing destination parents are created.
    // Usage example: ign::catalog::move("/tmp/source", "/tmp/archive");
    inline ign::status move(
        const std::filesystem::path& source,
        const std::filesystem::path& destination) {
        const std::filesystem::path source_path =
            ign::detail::expand_user_path(source);
        const std::filesystem::path destination_path =
            ign::detail::expand_user_path(destination);
        std::error_code ec;
        const bool source_is_directory =
            std::filesystem::is_directory(source_path, ec);

        if (ec) return detail::error_result(ec);
        if (!source_is_directory) return ign::status::not_found;

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
            ign::catalog::copy(source_path, destination_path);
        if (copy_result != ign::status::ok) return copy_result;

        return ign::catalog::remove(source_path);
    }

}
