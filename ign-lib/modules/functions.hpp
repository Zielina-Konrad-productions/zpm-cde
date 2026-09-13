#pragma once
#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace ign {

    namespace args_detail {

        inline std::vector<std::string> make_args(
            int argc,
            const char* const argv[],
            bool include_program_name
        ) {
            std::vector<std::string> arguments;

            if (argc <= 0 || argv == nullptr) {
                return arguments;
            }

            const int first_argument = include_program_name ? 0 : 1;

            if (argc > first_argument) {
                arguments.reserve(static_cast<std::size_t>(argc - first_argument));
            }

            for (int i = first_argument; i < argc; ++i) {
                if (argv[i] != nullptr) {
                    arguments.emplace_back(argv[i]);
                }
            }

            return arguments;
        }

        inline std::vector<std::string>& stored_args() {
            static std::vector<std::string> arguments;
            return arguments;
        }

        inline void remember_args(int argc, const char* const argv[]) {
            stored_args() = make_args(argc, argv, true);
        }

        inline const std::vector<std::string>& current_args() {
            return stored_args();
        }

        inline std::vector<std::string> skip_program_name(
            const std::vector<std::string>& arguments
        ) {
            if (arguments.empty()) {
                return {};
            }

            return std::vector<std::string>(
                arguments.begin() + 1,
                arguments.end()
            );
        }

        inline bool contains(std::string_view expected) {
            const auto& arguments = current_args();

            for (std::size_t i = 1; i < arguments.size(); ++i) {
                if (std::string_view(arguments[i]) == expected) {
                    return true;
                }
            }

            return false;
        }

    } // namespace args_detail

    
    //any() function
    // if any of args are true do something
    //example
    //
    //if (any(arg1, arg2) == true){
    // std::cout << "it works!" << std::endl;
    // }
    template <typename... Args>
    inline constexpr bool any(Args... args) {
        return (static_cast<bool>(args) || ...);
    }

    //args_init() function
    // stores command line arguments for later ign::args() calls
    // usage: ign::args_init(argc, argv);
    inline void args_init(int argc, const char* const argv[]) {
        args_detail::remember_args(argc, argv);
    }

    //args() function
    // returns command line arguments as strings, without the program name
    // usage: auto arguments = ign::args();
    inline std::vector<std::string> args() {
        return args_detail::skip_program_name(args_detail::current_args());
    }

    //args() flag check
    // usage: if (ign::args("--full")) { ... }
    inline bool args(std::string_view argument) {
        return args_detail::contains(argument);
    }

    inline bool args(const std::string& argument) {
        return args(std::string_view(argument));
    }

    inline bool args(const char* argument) {
        if (argument == nullptr) {
            return false;
        }

        return args(std::string_view(argument));
    }

    //args() from main()
    // also remembers arguments for later calls such as ign::args("--full")
    inline std::vector<std::string> args(
        int argc,
        const char* const argv[],
        bool include_program_name = false
    ) {
        args_init(argc, argv);

        if (include_program_name) {
            return args_detail::current_args();
        }

        return args_detail::skip_program_name(args_detail::current_args());
    }

    // args_any() flag check
    // returns true when at least one of the specified arguments was passed
    // usage: if (ign::args_any("--full", "-y")) { ... }
    template <typename... Arguments>
    inline bool args_any(const Arguments&... arguments) {
        return (ign::args(arguments) || ...);
    }

    // args_all() flag check
    // returns true only when every specified argument was passed
    // usage: if (ign::args_all("--full", "-y")) { ... }
    template <typename... Arguments>
    inline bool args_all(const Arguments&... arguments) {
        return (ign::args(arguments) && ...);
    }

    // args_none() flag check
    // returns true when none of the specified arguments were passed
    // usage: if (ign::args_none("--help", "--version")) { ... }
    template <typename... Arguments>
    inline bool args_none(const Arguments&... arguments) {
        return (!ign::args(arguments) && ...);
    }

    //all() function
    // returns true if all arguments evaluate to true
    template <typename... Args>
    inline constexpr bool all(Args... args) {
        return (static_cast<bool>(args) && ...);
    }

    //none() function
    // returns true if no argument evaluates to true
    template <typename... Args>
    inline constexpr bool none(Args... args) {
        return (!static_cast<bool>(args) && ...);
    }

    //input() function
    //usage ign::input(variable);
    // string 
    template <typename T>
    inline void input(T& variable) {
    std::cin >> variable;
    }
}
