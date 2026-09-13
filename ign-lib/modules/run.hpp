#pragma once
#include "path.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <ostream>
#include <streambuf>
#include <string>
#include <system_error>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
#include <sstream> 

#include "../third_party/subprocess.h/subprocess.h"

// Usage:
//   ign::run("program --flag \"argument with spaces\"");
//   ign::run({"program", "--flag", "argument with spaces"});
//
// run() executes a program directly. Shell operators such as &&, | and > are
// not interpreted; use run_shell() when they are needed:
//   ign::run_shell("echo hello && echo world");
namespace ign::run_detail {

inline bool is_space(char character) {
    return std::isspace(static_cast<unsigned char>(character)) != 0;
}

// A small, deliberately non-expanding shell-like lexer. It handles quoting
// and escaping but does not perform variable, wildcard or command expansion.
inline std::vector<std::string> split_posix_command(const std::string& command) {
    std::vector<std::string> arguments;
    std::size_t position = 0;

    while (position < command.size()) {
        while (position < command.size() && is_space(command[position])) {
            ++position;
        }

        if (position == command.size()) {
            break;
        }

        std::string argument;
        bool in_single_quotes = false;
        bool in_double_quotes = false;

        while (position < command.size()) {
            const char character = command[position];

            if (!in_single_quotes && !in_double_quotes && is_space(character)) {
                break;
            }

            if (!in_double_quotes && character == '\'') {
                in_single_quotes = !in_single_quotes;
                ++position;
                continue;
            }

            if (!in_single_quotes && character == '"') {
                in_double_quotes = !in_double_quotes;
                ++position;
                continue;
            }

            if (!in_single_quotes && character == '\\' &&
                position + 1 < command.size()) {
                const char escaped = command[position + 1];
                if (!in_double_quotes || escaped == '"' || escaped == '\\' ||
                    escaped == '$' || escaped == '`' || escaped == '\n') {
                    if (escaped != '\n') {
                        argument.push_back(escaped);
                    }
                    position += 2;
                    continue;
                }
            }

            argument.push_back(character);
            ++position;
        }

        arguments.push_back(std::move(argument));
    }

    return arguments;
}

// Implements the quoting rules used by the Windows C runtime: whitespace
// separates arguments, quotes group text and backslashes only escape quotes.
inline std::vector<std::string> split_windows_command(
    const std::string& command
    ) {
    std::vector<std::string> arguments;
    std::size_t position = 0;

    while (position < command.size()) {
        while (position < command.size() && is_space(command[position])) {
            ++position;
        }

        if (position == command.size()) {
            break;
        }

        std::string argument;
        bool in_quotes = false;

        while (position < command.size()) {
            if (!in_quotes && is_space(command[position])) {
                break;
            }

            if (command[position] == '\\') {
                const std::size_t slash_start = position;
                while (position < command.size() && command[position] == '\\') {
                    ++position;
                }

                const std::size_t slash_count = position - slash_start;
                if (position < command.size() && command[position] == '"') {
                    argument.append(slash_count / 2, '\\');

                    if (slash_count % 2 != 0) {
                        argument.push_back('"');
                        ++position;
                    } else {
                        ++position;
                        if (in_quotes && position < command.size() &&
                            command[position] == '"') {
                            argument.push_back('"');
                            ++position;
                        } else {
                            in_quotes = !in_quotes;
                        }
                    }
                } else {
                    argument.append(slash_count, '\\');
                }
                continue;
            }

            if (command[position] == '"') {
                ++position;
                if (in_quotes && position < command.size() &&
                    command[position] == '"') {
                    argument.push_back('"');
                    ++position;
                } else {
                    in_quotes = !in_quotes;
                }
                continue;
            }

            argument.push_back(command[position]);
            ++position;
        }

        arguments.push_back(std::move(argument));
    }

    return arguments;
}

inline std::vector<std::string> split_command(const std::string& command) {
#if defined(_WIN32)
    return split_windows_command(command);
#else
    return split_posix_command(command);
#endif
}

inline bool print_available_output(
    subprocess_s& process,
    std::ostream* output = nullptr) {
    std::array<char, 4096> buffer{};
    bool success = true;

    while (true) {
        const unsigned bytes_read = subprocess_read_stdout(
            &process,
            buffer.data(),
            static_cast<unsigned>(buffer.size())
            );

        if (bytes_read == 0) {
            break;
        }

        if (output != nullptr) {
            output->write(
                buffer.data(), static_cast<std::streamsize>(bytes_read));
            output->flush();
            if (!*output) success = false;
        } else {
            std::size_t bytes_written = 0;
            while (bytes_written < bytes_read) {
                const std::size_t written = std::fwrite(
                    buffer.data() + bytes_written,
                    1,
                    bytes_read - bytes_written,
                    stdout
                    );
                if (written == 0) {
                    success = false;
                    break;
                }
                bytes_written += written;
            }
            if (std::fflush(stdout) != 0) success = false;
        }
    }

    return success;
}

// Forwards every character written to it into two underlying stream buffers
// at once. Used to make a single subprocess output pass write to a file and
// to stdout simultaneously (see run_to_file(..., copy_output = true)).
class tee_streambuf : public std::streambuf {
public:
    tee_streambuf(std::streambuf* first, std::streambuf* second)
        : first_(first), second_(second) {}

protected:
    int overflow(int character) override {
        if (character == EOF) return traits_type::not_eof(character);

        const int first_result = first_->sputc(static_cast<char>(character));
        const int second_result = second_->sputc(static_cast<char>(character));

        return (first_result == EOF || second_result == EOF)
                   ? EOF
                   : character;
    }

    std::streamsize xsputn(const char* data, std::streamsize count) override {
        const std::streamsize first_written = first_->sputn(data, count);
        const std::streamsize second_written = second_->sputn(data, count);
        return std::min(first_written, second_written);
    }

    int sync() override {
        const int first_result = first_->pubsync();
        const int second_result = second_->pubsync();
        return (first_result == 0 && second_result == 0) ? 0 : -1;
    }

private:
    std::streambuf* first_;
    std::streambuf* second_;
};

inline int run_process(
    const char* const arguments[],
    std::ostream* output = nullptr) {
    if (arguments == nullptr || arguments[0] == nullptr ||
        arguments[0][0] == '\0') {
        return subprocess_error_not_found;
    }

    subprocess_s process{};
    const int create_result = subprocess_create(
        arguments,
        subprocess_option_search_user_path |
            subprocess_option_inherit_environment |
            subprocess_option_combined_stdout_stderr |
            subprocess_option_enable_async |
            subprocess_option_enable_async_no_wait,
        &process
        );

    if (create_result != subprocess_error_success) {
        return create_result;
    }

    // run() has no API for supplying input. Closing this stream immediately is
    // essential: otherwise programs waiting for EOF on stdin never terminate.
    if (process.stdin_file != nullptr) {
        std::fclose(process.stdin_file);
        process.stdin_file = nullptr;
    }

    int alive_result = 0;
    bool output_success = true;
    do {
        if (!print_available_output(process, output)) output_success = false;
        alive_result = subprocess_alive(&process);
        if (alive_result > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    } while (alive_result > 0);

    // Bytes can remain buffered in the pipe after the process exits.
    if (!print_available_output(process, output)) output_success = false;

    int return_code = 0;
    const int join_result = subprocess_join(&process, &return_code);
    const int destroy_result = subprocess_destroy(&process);

    if (alive_result < 0) {
        return subprocess_error_unknown;
    }
    if (join_result != subprocess_error_success) {
        return join_result;
    }
    if (destroy_result != subprocess_error_success) {
        return destroy_result;
    }
    if (!output_success) {
        return subprocess_error_unknown;
    }

    return return_code;
}

inline int run_arguments(
    const std::vector<std::string>& arguments,
    std::ostream* output = nullptr) {
    if (arguments.empty() || arguments.front().empty()) {
        return subprocess_error_not_found;
    }

    std::vector<const char*> argv;
    argv.reserve(arguments.size() + 1);
    for (const std::string& argument : arguments) {
        argv.push_back(argument.c_str());
    }
    argv.push_back(nullptr);

    return run_process(argv.data(), output);
}

inline int output_error_result() {
    return errno == EACCES || errno == EPERM
               ? subprocess_error_permission_denied
               : subprocess_error_unknown;
}

// When copy_output is true, output is written to the file and echoed to
// stdout in the same pass using tee_streambuf.
inline int run_arguments_to_file(
    const std::filesystem::path& fullpath,
    const std::vector<std::string>& arguments,
    bool append,
    bool copy_output) {
    const std::filesystem::path output_path =
        ign::detail::expand_user_path(fullpath);

    if (output_path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(output_path.parent_path(), ec);
        if (ec == std::errc::permission_denied ||
            ec == std::errc::operation_not_permitted) {
            return subprocess_error_permission_denied;
        }
        if (ec) return subprocess_error_unknown;
    }

    errno = 0;
    const std::ios::openmode mode =
        std::ios::out | std::ios::binary |
        (append ? std::ios::app : std::ios::trunc);
    std::ofstream output(output_path, mode);
    if (!output) return output_error_result();

    int result = 0;
    if (copy_output) {
        tee_streambuf tee_buf(output.rdbuf(), std::cout.rdbuf());
        std::ostream tee_stream(&tee_buf);
        result = run_arguments(arguments, &tee_stream);
        std::cout.flush();
    } else {
        result = run_arguments(arguments, &output);
    }

    output.close();
    if (!output && result == 0) return output_error_result();
    return result;
}

} // namespace ign::run_detail

namespace ign {

// The vector overload avoids parsing entirely and is recommended whenever the
// arguments are already available separately.
inline int run(const std::vector<std::string>& arguments) {
    return run_detail::run_arguments(arguments);
}

inline int run(std::initializer_list<std::string> arguments) {
    return run(std::vector<std::string>(arguments));
}

inline int run(const std::string& command) {
    return run_detail::run_arguments(run_detail::split_command(command));
}

// Runs a program and writes its combined stdout and stderr to a file.
// Missing parent directories and the file itself are created automatically.
// Output is appended by default; pass false as the third argument to overwrite.
// Pass true as the fourth argument (copy_output) to also echo the output to
// stdout as it is produced, in addition to writing it to the file.
inline int run_to_file(
    const std::filesystem::path& fullpath,
    const std::vector<std::string>& arguments,
    bool append = true,
    bool copy_output = false) {
    return run_detail::run_arguments_to_file(
        fullpath, arguments, append, copy_output);
}

inline int run_to_file(
    const std::filesystem::path& fullpath,
    std::initializer_list<std::string> arguments,
    bool append = true,
    bool copy_output = false) {
    return run_to_file(
        fullpath, std::vector<std::string>(arguments), append, copy_output);
}

inline int run_to_file(
    const std::filesystem::path& fullpath,
    const std::string& command,
    bool append = true,
    bool copy_output = false) {
    return run_to_file(
        fullpath, run_detail::split_command(command), append, copy_output);
}

// Runs any number of complete commands and appends all their output to the
// same file. Every command is attempted and the first non-zero result wins.
// Output is not echoed to stdout; call run_to_file() individually with
// copy_output = true if that is needed for a specific command.
template<
    typename FirstCommand,
    typename SecondCommand,
    typename... Commands,
    std::enable_if_t<
        std::is_constructible_v<std::string, FirstCommand&&> &&
            std::is_constructible_v<std::string, SecondCommand&&> &&
            (std::is_constructible_v<std::string, Commands&&> && ...),
        int> = 0>
inline int run_to_file(
    const std::filesystem::path& fullpath,
    FirstCommand&& first_command,
    SecondCommand&& second_command,
    Commands&&... remaining_commands) {
    int result = ign::run_to_file(
        fullpath, std::string(std::forward<FirstCommand>(first_command)));

    const auto run_one = [&fullpath, &result](auto&& command) {
        const int current = ign::run_to_file(
            fullpath,
            std::string(std::forward<decltype(command)>(command)));
        if (result == 0 && current != 0) result = current;
    };

    run_one(std::forward<SecondCommand>(second_command));
    (run_one(std::forward<Commands>(remaining_commands)), ...);
    return result;
}

// Runs multiple complete commands in sequence. Every command is attempted.
// Returns 0 when all commands succeed, otherwise the first non-zero result.
// Use the initializer-list overload to pass separate arguments to one program.
// Usage example: ign::run("git status", "git log -1");
template<typename... Commands>
inline int run(
    const std::string& first_command,
    const std::string& second_command,
    const Commands&... remaining_commands) {
    int result = ign::run(first_command);
    const auto run_one = [&result](const auto& command) {
        const int current = ign::run(std::string(command));
        if (result == 0 && current != 0) result = current;
    };

    run_one(second_command);
    (run_one(remaining_commands), ...);
    return result;
}

inline int run_shell(const std::string& command) {
    if (command.empty()) {
        return subprocess_error_not_found;
    }

#if defined(_WIN32)
    // /D disables potentially surprising AutoRun commands. /S makes /C quote
    // handling deterministic. subprocess.h converts UTF-8 arguments to UTF-16.
    const char* const arguments[] = {
        "cmd.exe", "/D", "/S", "/C", command.c_str(), nullptr
    };
#else
    const char* const arguments[] = {
        "/bin/sh", "-c", command.c_str(), nullptr
    };
#endif

    return run_detail::run_process(arguments);
}

// Shell-enabled counterpart of run_to_file(). Use only with trusted input.
inline int run_shell_to_file(
    const std::filesystem::path& fullpath,
    const std::string& command,
    bool append = true,
    bool copy_output = false) {
    if (command.empty()) {
        return subprocess_error_not_found;
    }

#if defined(_WIN32)
    const std::vector<std::string> arguments = {
        "cmd.exe", "/D", "/S", "/C", command
    };
#else
    const std::vector<std::string> arguments = {
        "/bin/sh", "-c", command
    };
#endif

    return run_to_file(fullpath, arguments, append, copy_output);
}

// Runs any number of complete shell commands and appends their output to one
// file. Every command is attempted and the first non-zero result is returned.
// Output is not echoed to stdout; call run_shell_to_file() individually with
// copy_output = true if that is needed for a specific command.
template<
    typename FirstCommand,
    typename SecondCommand,
    typename... Commands,
    std::enable_if_t<
        std::is_constructible_v<std::string, FirstCommand&&> &&
            std::is_constructible_v<std::string, SecondCommand&&> &&
            (std::is_constructible_v<std::string, Commands&&> && ...),
        int> = 0>
inline int run_shell_to_file(
    const std::filesystem::path& fullpath,
    FirstCommand&& first_command,
    SecondCommand&& second_command,
    Commands&&... remaining_commands) {
    int result = ign::run_shell_to_file(
        fullpath, std::string(std::forward<FirstCommand>(first_command)));

    const auto run_one = [&fullpath, &result](auto&& command) {
        const int current = ign::run_shell_to_file(
            fullpath,
            std::string(std::forward<decltype(command)>(command)));
        if (result == 0 && current != 0) result = current;
    };

    run_one(std::forward<SecondCommand>(second_command));
    (run_one(std::forward<Commands>(remaining_commands)), ...);
    return result;
}

// Runs multiple complete shell commands in sequence. Every command is
// attempted. Returns 0 when all commands succeed, otherwise the first
// non-zero result.
// Usage example: ign::run_shell("echo first", "echo second");
template<typename... Commands>
inline int run_shell(
    const std::string& first_command,
    const std::string& second_command,
    const Commands&... remaining_commands) {
    int result = ign::run_shell(first_command);
    const auto run_one = [&result](const auto& command) {
        const int current = ign::run_shell(std::string(command));
        if (result == 0 && current != 0) result = current;
    };

    run_one(second_command);
    (run_one(remaining_commands), ...);
    return result;
}

    // Like run_to_string, but also returns the process exit code. Needed
    // whenever "did this produce output" is not a reliable enough signal
    // on its own (stdout and stderr are combined, so a program that prints
    // an error message on failure can otherwise look like a success).
    inline std::pair<std::string, int> run_to_string_with_status(
        const std::string& command) {
        std::stringstream ss;
        const int status =
            run_detail::run_arguments(run_detail::split_command(command), &ss);
        return {ss.str(), status};
    }

    // Runs a shell command and captures its full stdout as a string (includes newlines, if any)
    inline std::string run_to_string(const std::string& command) {
        std::stringstream ss;
        run_detail::run_arguments(run_detail::split_command(command), &ss);
        return ss.str();
    }

    // Counterpart
    inline std::string run_shell_to_string(const std::string& command) {
        std::stringstream ss;
    #if defined(_WIN32)
        std::vector<std::string> arguments = {"cmd.exe", "/D", "/S", "/C", command};
    #else
        std::vector<std::string> arguments = {"/bin/sh", "-c", command};
    #endif
        run_detail::run_arguments(arguments, &ss);
        return ss.str();
}





} // namespace ign
