#include <cstdlib>
#include <ctime>
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
    fs::path source_root;
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
            return fs::path(temp) / "ign-lib-install.log";
        }
    }
    return fs::path("ign-lib-install.log");
#else
    return fs::path("/tmp/ign-lib-install.log");
#endif
}

fs::path absolute_path(const fs::path& path) {
    std::error_code ec;
    const fs::path absolute = fs::absolute(path, ec);
    return ec ? path : absolute;
}

fs::path normalize_existing_path(const fs::path& path) {
    std::error_code ec;
    const fs::path normalized = fs::weakly_canonical(path, ec);
    return ec ? absolute_path(path) : normalized;
}

void print_help(const char* executable) {
    std::cout
        << "Usage: " << executable << " [source-root] [options]\n"
        << "\n"
        << "Options:\n"
        << "  --source <path>   Source ign-lib directory. Defaults to cwd.\n"
        << "  --target <path>   Install destination. Defaults to "
        << default_target() << "\n"
        << "  -y, --yes         Replace an existing installation without asking.\n"
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
    parsed.source_root = fs::current_path();
    parsed.target = default_target();

    bool source_set = false;

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

        if (argument == "--source") {
            std::string value;
            if (!read_value(index, argc, argv, value)) {
                error = "--source requires a path";
                return false;
            }
            parsed.source_root = value;
            source_set = true;
            continue;
        }

        if (argument.rfind("--source=", 0) == 0) {
            parsed.source_root = argument.substr(9);
            source_set = true;
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

        if (!argument.empty() && argument.front() == '-') {
            error = "unknown option: " + argument;
            return false;
        }

        if (source_set) {
            error = "unexpected extra positional argument: " + argument;
            return false;
        }

        parsed.source_root = argument;
        source_set = true;
    }

    parsed.source_root = normalize_existing_path(parsed.source_root);
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

bool confirm_replace(const fs::path& target) {
    std::cout << "Existing installation found at " << target
              << ". Replace it [y/N]? ";

    std::string answer;
    std::getline(std::cin, answer);

    return answer == "y" || answer == "Y" ||
           answer == "yes" || answer == "YES";
}

bool require_regular_file(const fs::path& path, const std::string& label) {
    std::error_code ec;
    const bool exists = fs::is_regular_file(path, ec);
    if (ec) {
        std::cerr << "Error: cannot read " << label << ": "
                  << ec.message() << '\n';
        return false;
    }
    if (!exists) {
        std::cerr << "Error: missing " << label << " at " << path << '\n';
        return false;
    }
    return true;
}

bool require_directory(const fs::path& path, const std::string& label) {
    std::error_code ec;
    const bool exists = fs::is_directory(path, ec);
    if (ec) {
        std::cerr << "Error: cannot read " << label << ": "
                  << ec.message() << '\n';
        return false;
    }
    if (!exists) {
        std::cerr << "Error: missing " << label << " at " << path << '\n';
        return false;
    }
    return true;
}

bool validate_source_tree(const fs::path& source_root, log_file& log) {
    log.line("checking source tree: " + source_root.string());

    return require_regular_file(source_root / "ign.hpp", "ign.hpp") &&
           require_directory(source_root / "modules", "modules directory") &&
           require_directory(source_root / "third_party", "third_party directory") &&
           require_directory(
               source_root / "third_party" / "subprocess.h",
               "third_party/subprocess.h directory") &&
           require_regular_file(
               source_root / "third_party" / "subprocess.h" / "subprocess.h",
               "third_party/subprocess.h/subprocess.h");
}

bool remove_existing_target(const options& opts, log_file& log) {
    std::error_code ec;
    const bool target_exists = fs::exists(opts.target, ec);
    if (ec) {
        std::cerr << "Error: cannot inspect " << opts.target << ": "
                  << ec.message() << '\n';
        log.line("checking target failed: " + ec.message());
        return false;
    }

    if (!target_exists) {
        return true;
    }

    if (!fs::is_directory(opts.target, ec)) {
        std::cerr << "Error: target exists but is not a directory: "
                  << opts.target << '\n';
        log.line("target exists but is not a directory");
        return false;
    }

    if (!opts.assume_yes && !confirm_replace(opts.target)) {
        std::cerr << "Installation canceled.\n";
        log.line("installation canceled by user");
        return false;
    }

    log.line("removing old installation");
    fs::remove_all(opts.target, ec);
    if (ec) {
        std::cerr << "Error: cannot remove old installation: "
                  << ec.message() << '\n';
        log.line("removing old installation failed: " + ec.message());
        return false;
    }

    return true;
}

bool copy_file(
    const fs::path& source,
    const fs::path& destination,
    log_file& log) {
    std::error_code ec;
    fs::copy_file(
        source,
        destination,
        fs::copy_options::overwrite_existing,
        ec);

    if (ec) {
        std::cerr << "Error: cannot copy " << source << " to "
                  << destination << ": " << ec.message() << '\n';
        log.line("copy file failed: " + source.string() + " -> " +
                 destination.string() + ": " + ec.message());
        return false;
    }

    return true;
}

bool copy_directory(
    const fs::path& source,
    const fs::path& destination,
    log_file& log) {
    std::error_code ec;
    fs::copy(
        source,
        destination,
        fs::copy_options::recursive |
            fs::copy_options::overwrite_existing,
        ec);

    if (ec) {
        std::cerr << "Error: cannot copy " << source << " to "
                  << destination << ": " << ec.message() << '\n';
        log.line("copy directory failed: " + source.string() + " -> " +
                 destination.string() + ": " + ec.message());
        return false;
    }

    return true;
}

bool copy_optional_file(
    const fs::path& source,
    const fs::path& destination,
    log_file& log) {
    std::error_code ec;
    if (!fs::is_regular_file(source, ec)) {
        return true;
    }

    return copy_file(source, destination, log);
}

bool copy_optional_directory(
    const fs::path& source,
    const fs::path& destination,
    log_file& log) {
    std::error_code ec;
    if (!fs::is_directory(source, ec)) {
        return true;
    }

    return copy_directory(source, destination, log);
}

bool write_manifest(
    const fs::path& source_root,
    const fs::path& target,
    log_file& log) {
    const fs::path manifest_path = target / "INSTALL-MANIFEST.txt";
    std::ofstream manifest(manifest_path, std::ios::out | std::ios::trunc);
    if (!manifest) {
        std::cerr << "Error: cannot write manifest at "
                  << manifest_path << '\n';
        log.line("writing manifest failed");
        return false;
    }

    const std::time_t now = std::time(nullptr);
    char time_buffer[64] = {};
    if (const std::tm* local_time = std::localtime(&now)) {
        std::strftime(
            time_buffer,
            sizeof(time_buffer),
            "%Y-%m-%d %H:%M:%S %z",
            local_time);
    }

    manifest
        << "ign-lib installation\n"
        << "source=" << source_root << '\n'
        << "target=" << target << '\n'
        << "installed_at=" << time_buffer << '\n'
        << "files=ign.hpp, modules/, third_party/, README.txt, documentation/\n";

    return static_cast<bool>(manifest);
}

bool install_library(const options& opts, log_file& log) {
    if (!validate_source_tree(opts.source_root, log)) {
        return false;
    }

    if (!remove_existing_target(opts, log)) {
        return false;
    }

    std::error_code ec;
    fs::create_directories(opts.target, ec);
    if (ec) {
        std::cerr << "Error: cannot create " << opts.target << ": "
                  << ec.message() << '\n';
        log.line("creating target failed: " + ec.message());
        return false;
    }

    log.line("copying files");

    if (!copy_file(opts.source_root / "ign.hpp", opts.target / "ign.hpp", log)) {
        return false;
    }

    if (!copy_directory(
            opts.source_root / "modules",
            opts.target / "modules",
            log)) {
        return false;
    }

    if (!copy_directory(
            opts.source_root / "third_party",
            opts.target / "third_party",
            log)) {
        return false;
    }

    if (!copy_optional_file(
            opts.source_root / "README.txt",
            opts.target / "README.txt",
            log)) {
        return false;
    }

    if (!copy_optional_directory(
            opts.source_root / "documentation",
            opts.target / "documentation",
            log)) {
        return false;
    }

    if (!write_manifest(opts.source_root, opts.target, log)) {
        return false;
    }

    const bool install_ok =
        fs::is_regular_file(opts.target / "ign.hpp", ec) &&
        fs::is_directory(opts.target / "modules", ec) &&
        fs::is_directory(opts.target / "third_party", ec);

    if (!install_ok) {
        std::cerr << "Error: installation verification failed.\n";
        log.line("installation verification failed");
        return false;
    }

    log.line("install complete");
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
    log.line("ign-lib installer started");

    if (!opts.custom_target && !has_required_privileges()) {
#if defined(_WIN32)
        std::cerr << "Error: run this installer as Administrator, or pass "
                     "--target for a user-writable directory.\n";
#else
        std::cerr << "Error: run this installer as root, or pass --target "
                     "for a user-writable directory.\n";
#endif
        log.line("missing privileges for default target");
        return 1;
    }

    std::cout << "Installing ign-lib\n"
              << "  source: " << opts.source_root << '\n'
              << "  target: " << opts.target << '\n';

    if (!install_library(opts, log)) {
        std::cerr << "Install failed. Log: " << log.path() << '\n';
        return 1;
    }

    std::cout
        << "Success: ign-lib installed.\n"
        << "Usage: #include <ign-lib/ign.hpp>\n";

    std::cout << "Compile example: g++ -std=c++17 main.cpp -I\""
              << opts.target.parent_path().string() << "\"\n";

    std::cout << "Log: " << log.path() << '\n';
    return 0;
}
