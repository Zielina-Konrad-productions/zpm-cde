#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

struct options {
    fs::path target;
    bool custom_target = false;
    bool assume_yes = false;
    bool show_help = false;
};

class log_file {
public:
    explicit log_file(const fs::path& path) : path_(path) {
        std::error_code ec;
        if (path_.has_parent_path()) {
            fs::create_directories(path_.parent_path(), ec);
        }
        stream_.open(path_, std::ios::out | std::ios::trunc);
    }

    void line(const std::string& text) {
        if (stream_) {
            stream_ << text << '\n';
        }
    }

    const fs::path& path() const {
        return path_;
    }

private:
    fs::path path_;
    std::ofstream stream_;
};

fs::path default_target() {
#if defined(_WIN32)
    if (const char* program_files = std::getenv("ProgramFiles")) {
        if (*program_files != '\0') {
            return fs::path(program_files) / "ign-lib";
        }
    }
    return fs::path("C:\\Program Files") / "ign-lib";
#else
    return fs::path("/usr/local/include/ign-lib");
#endif
}

fs::path default_log_path() {
#if defined(_WIN32)
    if (const char* temp = std::getenv("TEMP")) {
        if (*temp != '\0') {
            return fs::path(temp) / "ign-lib-uninstall.log";
        }
    }
    return fs::path("ign-lib-uninstall.log");
#else
    return fs::path("/tmp/ign-lib-uninstall.log");
#endif
}

fs::path absolute_path(const fs::path& path) {
    std::error_code ec;
    const fs::path absolute = fs::absolute(path, ec);
    return ec ? path : absolute;
}

void print_help(const char* executable) {
    std::cout
        << "Usage: " << executable << " [options]\n"
        << "\n"
        << "Options:\n"
        << "  --target <path>   Install destination to remove. Defaults to "
        << default_target() << "\n"
        << "  -y, --yes         Remove without asking.\n"
        << "  -h, --help        Show this help.\n";
}

bool read_value(int& index, int argc, char* argv[], std::string& value) {
    if (index + 1 >= argc) {
        return false;
    }

    ++index;
    value = argv[index];
    return true;
}

bool parse_options(int argc, char* argv[], options& parsed, std::string& error) {
    parsed.target = default_target();

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];

        if (argument == "-h" || argument == "--help") {
            parsed.show_help = true;
            return true;
        }

        if (argument == "-y" || argument == "--yes") {
            parsed.assume_yes = true;
            continue;
        }

        if (argument == "--target") {
            std::string value;
            if (!read_value(index, argc, argv, value)) {
                error = "--target requires a path";
                return false;
            }
            parsed.target = value;
            parsed.custom_target = true;
            continue;
        }

        if (argument.rfind("--target=", 0) == 0) {
            parsed.target = argument.substr(9);
            parsed.custom_target = true;
            continue;
        }

        error = "unknown option: " + argument;
        return false;
    }

    parsed.target = absolute_path(parsed.target);
    return true;
}

bool has_required_privileges() {
#if defined(_WIN32)
    return std::system("net session >nul 2>&1") == 0;
#else
    return geteuid() == 0;
#endif
}

bool confirm_remove(const fs::path& target) {
    std::cout << "Remove ign-lib from " << target << " [y/N]? ";

    std::string answer;
    std::getline(std::cin, answer);

    return answer == "y" || answer == "Y" ||
           answer == "yes" || answer == "YES";
}

bool uninstall_library(const options& opts, log_file& log) {
    std::error_code ec;
    const bool target_exists = fs::exists(opts.target, ec);

    if (ec) {
        std::cerr << "Error: cannot inspect " << opts.target << ": "
                  << ec.message() << '\n';
        log.line("checking target failed: " + ec.message());
        return false;
    }

    if (!target_exists) {
        std::cout << "ign-lib is not installed at " << opts.target << ".\n";
        log.line("target missing, nothing to remove");
        return true;
    }

    if (!fs::is_directory(opts.target, ec)) {
        std::cerr << "Error: target exists but is not a directory: "
                  << opts.target << '\n';
        log.line("target exists but is not a directory");
        return false;
    }

    if (!opts.assume_yes && !confirm_remove(opts.target)) {
        std::cerr << "Uninstall canceled.\n";
        log.line("uninstall canceled by user");
        return false;
    }

    fs::remove_all(opts.target, ec);
    if (ec) {
        std::cerr << "Error: cannot remove " << opts.target << ": "
                  << ec.message() << '\n';
        log.line("removing target failed: " + ec.message());
        return false;
    }

    if (fs::exists(opts.target, ec)) {
        std::cerr << "Error: target still exists after removal.\n";
        log.line("target still exists after removal");
        return false;
    }

    log.line("uninstall complete");
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    options opts;
    std::string error;

    if (!parse_options(argc, argv, opts, error)) {
        std::cerr << "Error: " << error << "\n\n";
        print_help(argv[0]);
        return 2;
    }

    if (opts.show_help) {
        print_help(argv[0]);
        return 0;
    }

    log_file log(default_log_path());
    log.line("ign-lib uninstaller started");

    if (!opts.custom_target && !has_required_privileges()) {
#if defined(_WIN32)
        std::cerr << "Error: run this uninstaller as Administrator, or pass "
                     "--target for a user-writable directory.\n";
#else
        std::cerr << "Error: run this uninstaller as root, or pass --target "
                     "for a user-writable directory.\n";
#endif
        log.line("missing privileges for default target");
        return 1;
    }

    std::cout << "Uninstalling ign-lib\n"
              << "  target: " << opts.target << '\n';

    if (!uninstall_library(opts, log)) {
        std::cerr << "Uninstall failed. Log: " << log.path() << '\n';
        return 1;
    }

    std::cout << "Success: ign-lib removed.\n"
              << "Log: " << log.path() << '\n';
    return 0;
}
