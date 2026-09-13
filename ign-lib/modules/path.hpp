#pragma once

#include <cstdlib>
#include <filesystem>
#include <string>

namespace ign::detail {

    inline std::filesystem::path home_path() {
        if (const char* home = std::getenv("HOME")) {
            if (*home != '\0') return std::filesystem::path(home);
        }

#if defined(_WIN32)
        if (const char* profile = std::getenv("USERPROFILE")) {
            if (*profile != '\0') return std::filesystem::path(profile);
        }

        const char* drive = std::getenv("HOMEDRIVE");
        const char* path = std::getenv("HOMEPATH");
        if (drive != nullptr && path != nullptr &&
            *drive != '\0' && *path != '\0') {
            return std::filesystem::path(std::string(drive) + path);
        }
#endif

        return {};
    }

    inline std::filesystem::path expand_user_path(
        const std::filesystem::path& path) {
        const std::string text = path.string();
        if (text.empty() || text.front() != '~') return path;

        const std::filesystem::path home = home_path();
        if (home.empty()) return path;

        if (text == "~") return home;

        if (text.size() >= 2 && (text[1] == '/' || text[1] == '\\')) {
            return home / std::filesystem::path(text.substr(2));
        }

        return path;
    }

}

